/*** includes ***/

#define _DEFAULT_SOURCE 
#define _BSD_SOURCE
#define _GNU_SOURCE 
// feature test macre
// for when complier complains about getline() -> portable!

#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <stdarg.h>
#include <fcntl.h>
#include <ncurses.h>
#include <locale.h>

/*** defines ***/

#define KILO_VERSION "0.0.1"
#define KILO_TAB_STOP 8
#define KILO_QUIT_TIMES 3

#define CTRL_KEY(k) ((k) & 0x1f) 
// 0x1f: hexadecimal(0x), 31(dec), 00011111(bi)
// mirrors Ctrl: strips bits 5,6 from key

enum editorkey{ // number larger than char
	BACKSPACE = 127,
	/* doesn'thave human-readable backslash-escape representation( eg)\r \n*/
	ARROW_LEFT = 1000,
	ARROW_RIGHT,
	ARROW_UP,
	ARROW_DOWN,
	DEL_KEY,
	HOME_KEY,
	END_KEY,
	PAGE_UP,
	PAGE_DOWN
};

enum editorHighlight{
	HL_NORMAL = 0,
	HL_COMMENT,
	HL_MLCOMMENT,
	HL_KEYWORD1,
	HL_KEYWORD2,
	HL_STRING,
	HL_NUMBER,
	HL_MATCH,
	HL_BRACKET_GOOD,
	HL_BRACKET_BAD
};

#define HL_HIGHLIGHT_NUMBERS (1<<0)
#define HL_HIGHLIGHT_STRINGS (1<<1)

/*** data ***/

struct editorSyntax{
	char* filetype;
	char** filematch; // each string contains pattern to match a filname against -> filetype
	// ends with NULL
	char** keywords;
	char* singleline_comment_start;
	char* multiline_comment_start;
	char* multiline_comment_end;
	int flags; // bit field: wheter to highlight number/strings etc
};

typedef struct erow{
	int idx;
	int size;
	int rsize;
	char* chars;
	char* render; //actual character to draw on the screen (eg) tabs, ^A: Ctrl-A 
	unsigned char* hl; // for each char: string, comment or number
	int hl_open_comment; // boolean, part of unclosed mlcomment?
} erow;

struct editorConfig {
	int cx, cy; // cursor pos within file 
	int rx; // index for render (>=cx bec tabs)
	int rowoff; // row of file currently displayed to on screen
	int coloff; // index into chars of each erow displayed on screen
	int screenrows;
	int screencols;
	int numrows;
	erow* row; // to store multiple lines
	int dirty; // how much it has been modified since last save
	char* filename;
	char statusmsg[80]; 
	time_t statusmsg_time; // to earse statusmsg after few seconds
	struct editorSyntax* syntax;
	struct termios orig_termios;
};

struct editorConfig E;

char bracket_pair[2][2];

/*** filetypes ***/

char* C_HL_extensions[] = { ".c", ".h", ".cpp", NULL};
char *C_HL_keywords[] = {
	"switch", "if", "while", "for", "break", "continue", "return", "else", 
	"struct|", "union|", "typedef", "static", "enum", "class|", "case",
	"int|", "long|", "double|", "float|", "char|", "unsigned|", "signed|",
	"void|", "FILE|","define|",  NULL
};
// c| : common C types: keyword2

struct editorSyntax HLDB[] ={
	// highlight database
	{
		"c",
		C_HL_extensions,
		C_HL_keywords,
		"//", "/*", "*/",
		HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS
	},
};

#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))
// length of HLDB array

/*** prototypes ***/
// to call functs before defined

void editorSetStatusMessage(const char* fmt, ...);
void editorRefreshScreen();
char* editorPrompt(char* prompt, void(*callback)(char*, int));
void InsertList(char*, int);
void EndRecommend();
char* fail[2]={NULL,};
void FailFunc(char*);
char* Pmatch(char*, char*, int*, int);
void FindBracket();

/*** terminal ***/

void die(const char* s) {
	write(STDOUT_FILENO, "\x1b[2J", 4);
	write(STDOUT_FILENO, "\x1b[H", 3);
	// don't clear the screen atexit() to keep error message
	perror(s); // prints error message for global errno(fail) 
	//<stdio.h>
	exit(1); // indicate failure (nonzero)
	//<stdlib.h>
}

void disableRawMode() {
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) die("tcsetattr"); // read terminal attributes
// apply to terminal
// TCSAFLUSH: waits for all pending output to be written to the terminal, and also discards any input after 'q' is discarded
}

void enableRawMode() { // turn of echo (doesn't show what you type)
	//--> to discard input after q (after TE ends)
	if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr");
	atexit(disableRawMode); // from <stdlib.h>, automatically called when program(kilo) exits

	struct termios raw = E.orig_termios;
	raw.c_oflag &= ~(OPOST); // disable translating output '\n'->'\r\n'
	// have to write \r\n for newline 
	// OPOST: <termios.h>, output flag. 
	raw.c_iflag &= ~(ICRNL | IXON | BRKINT | INPCK | ISTRIP);
	// ICRNL: <termios.h>, input flag. CR:carriage return, NL: new line -> stop translating carriage returns(13,'\r') inputted by the user into newlines(10,'\n')
	// IXON: <termios.h>, input flag. disable Ctrl-S, Ctrl-Q
	raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
	// IEXTEN: <termios.h> disable Ctrl-V
	//ISIG -> disable Ctrl-C, Ctrl-Z
	// c_lflag: "local flags"-dumping ground for other state
	// ECHO: bitflag.
	// ICANON: <termios.h> read input byte-by-byte (prev: line-by-line)
	raw.c_cflag |= (CS8);
	raw.c_cc[VMIN] = 0; // set minimum number of bytes of input needed before read() can return.
	raw.c_cc[VTIME] = 1; // maximum amount of time to wait before read() returns: 1/10 sec

	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

int editorReadKey() { // int because of enum ARROW
	// return one keypress
	// what about multiple bytes representing single keypress? (eg) -> [C
	int nread;
	char c;
	while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
		if (nread == -1 && errno != EAGAIN) die("read");
	}

	if(c=='\x1b'){ // to read arrow keys +  escape chars
		char seq[3];

		if(read(STDIN_FILENO, &seq[0],1)!=1) return '\x1b';
		if(read(STDIN_FILENO, &seq[1],1)!=1) return '\x1b';

		if(seq[0]=='['){
			if(seq[1]>='0' && seq[1]<='9'){
				if(read(STDIN_FILENO, &seq[2],1)!=1) return '\x1b';
				if(seq[2]=='~'){
					switch(seq[1]){
						case '1': return HOME_KEY;
					    case '3': return DEL_KEY; /* doesn't do anything for now*/
            			case '4': return END_KEY;
						case '5': return PAGE_UP;
						case '6': return PAGE_DOWN;
						case '7': return HOME_KEY;
            			case '8': return END_KEY;
					}
				}
			}
			else{
				switch(seq[1]){
					case 'A': return ARROW_UP;
					case 'B': return ARROW_DOWN;
					case 'C': return ARROW_RIGHT;
					case 'D': return ARROW_LEFT;
					case 'H': return HOME_KEY;
          			case 'F': return END_KEY;
				}
			}
		}
		else if(seq[0]=='0'){
			switch (seq[1]) {
        		case 'H': return HOME_KEY;
		        case 'F': return END_KEY;
			}
		}
		
		return '\x1b';
	}
	else{
		return c;
	}
}

int getCursorPosition(int* rows, int* cols){
	char buf[32];
	unsigned int i =0;
	if (write(STDOUT_FILENO, "\x1b[6n",4) != 4) return -1;
	// n: query terminal for status info
	// 6: ask for cursor pos.
	// read the reply from the standard input
	// <esc>[24;80R or similar

	while( i<sizeof(buf)-1){
		if(read(STDIN_FILENO,&buf[i],1)!=1) break;
		if(buf[i]=='R') break;
		i++;
	}
	buf[i]='\0';

	if(buf[0]!='\x1b' || buf[1]!='[') return -1;
	if(sscanf(&buf[2],"%d;%d",rows,cols)!=2) return -1;

	return -1;
}

int getWindowSize(int* rows, int* cols) {
	// <sys/ioctl.h>
	struct winsize ws;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
		// ioctl(): find number of rows, coloums of terminal
		if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B",12) != 12) return -1;
		//C: Cursor Forward, not past the screen
		//B: cursor down, not past the screen
		///999: large value to ensure cursor reaches right and bottom edge of screen,  
		return getCursorPosition(rows,cols);
		return -1;
	}
	else {
		*cols = ws.ws_col;
		*rows = ws.ws_row;
		return 0;
	}
}

/*** syntax highlighting ***/

int is_separator(int c){
	if(c == '\0') return 2;
	if(isspace(c)) return 1;
	if(strchr(",.()+-/*=~%<>[];",c) != NULL) return 2;
	return 0;
	// strchr(): <string.h> returns pointer to first occurring char
}

void editorUpdateSyntax(erow* row){
	// highlight chars of row
	row->hl = realloc(row->hl, row->rsize);
	memset(row->hl, HL_NORMAL, row->rsize);

	if(E.syntax == NULL) return;
	// txt -> no colour 

	char** keywords = E.syntax->keywords;

	char *scs = E.syntax->singleline_comment_start;
	char* mcs = E.syntax->multiline_comment_start;
	char* mce = E.syntax->multiline_comment_end;
	int scs_len = scs ? strlen(scs) : 0;
	int mcs_len = mcs ? strlen(mcs) : 0;
	int mce_len = mce ? strlen(mce) : 0;

	int prev_sep = 1; // previous char was a separator?
	// beginning of line is separator
	int in_string = 0;
	int in_comment = (row->idx > 0 && E.row[row->idx-1].hl_open_comment); // multiline comment
	bool name=0; // name gets inserted into RecommendList

	int i = 0;

/*	if(bracket_pair[0][0] == row->idx ){
		if(bracket_pair[1][0] == -1){
			row->hl[bracket_pair[0][1]] = HL_BRACKET_BAD;
			editorSetStatusMessage(" bracket pair not found"); // db
		}
		else{
			row->hl[bracket_pair[0][1]] = HL_NUMBER; //HL_BRACKET_GOOD; // dbg
		}
	}
	if(bracket_pair[1][0] == row->idx){
		row->hl[bracket_pair[1][1]] = HL_NUMBER; // HL_BRACKET_GOOD; //dbg
		editorSetStatusMessage(" bracket pair found"); // db
	}
*/
	while( i < row->rsize){
		char c = row->render[i];
		unsigned char prev_hl = (i>0) ? row->hl[i-1]:HL_NORMAL;

	    if (scs_len && !in_string && !in_comment) {
		    if (!strncmp(&row->render[i], scs, scs_len)) {
			// hl rest of line as comment
		    	memset(&row->hl[i], HL_COMMENT, row->rsize - i);
			    break;
		    }
	    }

		if(mcs_len && mce_len && !in_string){
			if(in_comment){
				row->hl[i] = HL_COMMENT;
				if(!strncmp(&row->render[i], mce, mce_len)){
					memset(&row->hl[i], HL_MLCOMMENT, mce_len);
					i += mce_len;
					in_comment=0;
					prev_sep = 1;
					continue;
				}
				else{
					i++; continue;
				}
			}
			else if (!strncmp(&row->render[i],mcs, mcs_len)){
				memset(&row->hl[i], HL_MLCOMMENT, mcs_len);
				i+=mcs_len;
				in_comment=1;
				continue;
			}
		}

	    if (E.syntax->flags & HL_HIGHLIGHT_STRINGS) {
		    if (in_string) {
		    	row->hl[i] = HL_STRING;
				if(c=='\\' && i + 1 < row->rsize){
					// for '\''
					// in string & current char is \ & next char exists -> hightlight both cur and next
					row->hl[i+1] = HL_STRING;
					i += 2;
					continue;
				}
			    if (c == in_string) in_string = 0;
				// quote close
			    i++;
		    	prev_sep = 1;
		    	continue;
		    } 
			else {
		    	if (c == '"' || c == '\'') {
		        	in_string = c;
			        row->hl[i] = HL_STRING;
			        i++;
		    	    continue;
		      	}
		    }
	    }

		if(E.syntax->flags & HL_HIGHLIGHT_NUMBERS){
			// check syntax flag if numbers should be hled for cur ft
			if((isdigit(c) && (prev_sep || prev_hl == HL_NUMBER)) || (c == '.' && prev_hl == HL_NUMBER)){
			// pre char of c is num or separator
			// or (num). : decimal
				row->hl[i] = HL_NUMBER;
				i++;
				prev_sep=0;
				continue;
			}
		}

		/* keyword2 다음에 오는 단어는 함수명/변수명으로 봄 */
		if(prev_sep){
			if(name){ // name=1 : 현재 보고 있는 char는 함수명/변수명에 속함
				switch(is_separator(c)){ // is_separator 수정: return 0: separator 아님, return 1 -> 공백, return 2 -> 나머지 separator
					case 2: name = 0; break; // keyword2가 변수/함수 선언이 아닌 다른 용도로 사용될 때 (eg) sizeof(char)
					case 0:
					{
						int a;
						// 함수명/변수명 길이 찾기
						for(a=0; i+a< row->rsize && !is_separator(row->render[i+a]); a++); 
						InsertList(&row->render[i], a); // 함수명/변수명 리스트에 삽입
						name=0; prev_sep=0;
						i+=a;
						continue;
					}
					default: break;
				}
				if(!is_separator(c)){ 
				}
			}
			else{
				int j;
				for(j=0; keywords[j]; j++){
					int klen = strlen(keywords[j]);
					int kw2 = keywords[j][klen-1]=='|';
					if(kw2) klen--; // remove |
	
					if(!strncmp(&row->render[i], keywords[j], klen) && is_separator(row->render[i + klen])){
						// (sep)keyword(sep)
						memset(&row->hl[i], kw2 ? HL_KEYWORD2 : HL_KEYWORD1, klen);
						i += klen;
						if(kw2) name=1; // next word is inserted into RecommendList
						break;
					}
				}
				if(keywords[j] != NULL){
					prev_sep = 0;
					continue;
				}
			}
		}

		prev_sep = is_separator(c);
		i++;
	}
	int changed = (row->hl_open_comment != in_comment);
	row->hl_open_comment = in_comment;
	if(changed && row->idx + 1 < E.numrows) editorUpdateSyntax(&E.row[row->idx+1]);
	// /* or */ might change syntax for follwing rows
}

int editorSyntaxToColor(int hl){
	// map hl -> ANSI colour code
	switch(hl){
		case HL_COMMENT: 
		case HL_MLCOMMENT: return 36; // cyan(skyblue)
		case HL_KEYWORD1: return 33; // yello
		case HL_KEYWORD2: return 32; // green
		case HL_STRING: return 35; // magenta(pink)
		case HL_NUMBER: return 31; //red
		case HL_MATCH: return 34; // blue
		case HL_BRACKET_GOOD: return 32; //42;
		case HL_BRACKET_BAD: return 31;//41
		default: return 37;
	}
}

void editorSelectSyntaxHighlight(){
	// match cur fn to fm fields in HLDB
	E.syntax = NULL;
	if(E.filename == NULL) return;

	char* ext = strrchr(E.filename, '.');
	// pointer to extension part of fn

	for(unsigned int j=0; j<HLDB_ENTRIES; j++){
		struct editorSyntax* s = &HLDB[j];
		unsigned int i=0;
		while(s->filematch[i]){
			int is_ext = (s->filematch[i][0] == '.');
			// is filematch extension?
			if((is_ext && ext && !strcmp(ext, s->filematch[i])) || (!is_ext && strstr(E.filename, s->filematch[i]))){
			E.syntax = s;
			
			int filerow; // rehighlight when no ft -> syntax set
			for(filerow=0; filerow<E.numrows; filerow++){
				editorUpdateSyntax(&E.row[filerow]);
			}

			return;
			}
			i++;
		}
	}
}

/*** row operations ***/

int editorRowCxToRx(erow* row, int cx){ 
	//convert chars index into render index
	int rx=0;
	int j;
	for(j=0;j<cx;j++){
		if(row->chars[j]=='\t'){
			rx += (KILO_TAB_STOP -1) -(rx%KILO_TAB_STOP);
			// % :how many columns we are to the right of last tabstop
		}
		rx++;
	}
	return rx;
}

int editorRowRxToCx(erow* row, int rx){
	int cur_rx=0;
	int cx;
	for(cx=0; cx<row->size; cx++){
		if(row->chars[cx]=='\t'){
			cur_rx+=(KILO_TAB_STOP-1)-(cur_rx % KILO_TAB_STOP);
		}
		cur_rx++;
		if(cur_rx>rx) return cx; 
	}
	return cx;
}

void editorUpdateRow(erow* row){ 
	// fill in the contents of render string whenever text of row changes
	// check for tabs
	int tabs=0;
	int j;
	for(j=0;j<row->size;j++){
		//count tabs for malloc.
		if(row->chars[j]=='\t') tabs++;
	}
	free(row->render);
	row->render=malloc(row->size+tabs*(KILO_TAB_STOP - 1)+1); // tab: 8 spaces?

	int idx=0;
	for(j=0; j<row->size; j++){
		if(row->chars[j]=='\t'){
			row->render[idx++] = ' ';
			while(idx%KILO_TAB_STOP !=0) row->render[idx++]=' ';
			// append until tab stop: column divisible by 8
		}
		else{
			row->render[idx++] = row->chars[j];
		}
	}
	row->render[idx]='\0';
	row->rsize=idx;

	editorUpdateSyntax(row);
}

void editorInsertRow(int at, char* s, size_t len){
	// insert new row to E.row 
	if(at<0 || at>E.numrows) return;

	E.row = realloc(E.row, sizeof(erow)*(E.numrows+1));
	memmove(&E.row[at+1], &E.row[at],sizeof(erow)*(E.numrows-at));
	for(int j=at+1; j<=E.numrows; j++) E.row[j].idx++;
	// update idx of each row whenever a row is inserted or deleted

	E.row[at].idx = at;

	E.row[at].size=len;
	E.row[at].chars=malloc(len+1);
	memcpy(E.row[at].chars, s, len);
	E.row[at].chars[len]='\0';

	E.row[at].rsize=0;
	E.row[at].render=NULL;
	E.row[at].hl = NULL;
	E.row[at].hl_open_comment = 0;
	editorUpdateRow(&E.row[at]);

	E.numrows++;
	E.dirty++;
}

void editorFreeRow(erow* row){
	free(row->render);
	free(row->chars);
	free(row->hl);
}	

void editorDelRow(int at){
	if(at<0 || at>=E.numrows) return;
	editorFreeRow(&E.row[at]);
	memmove(&E.row[at], &E.row[at+1], sizeof(erow)*(E.numrows-at-1));
	for(int j = at; j<E.numrows-1;j++) E.row[j].idx--;
	E.numrows--;
	E.dirty++;
}

void editorRowInsertChar(erow* row, int at, int c) {
	// inserts a single character into an erow
	 if (at < 0 || at > row->size) at = row->size;
	 row->chars = realloc(row->chars, row->size + 2);
	 // add mem for: char , null
	 memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
	 // memmove() <string.h> like memcpy(), but is safe to use when the source and destination arrays overlap.
	 row->size++;
	 row->chars[at] = c;
	 editorUpdateRow(row);
	 E.dirty++;
}

void editorRowAppendString(erow* row, char* s, size_t len){
	// append string to end of row
	row->chars = realloc(row->chars, row->size + len+1);
	memcpy(&row->chars[row->size],s, len);
	row->size+=len;
	row->chars[row->size] = '\0';
	editorUpdateRow(row);
	E.dirty++;
}

void editorRowDelChar(erow* row, int at) {
	if (at < 0 || at >= row->size) return;
	memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
	row->size--;
	editorUpdateRow(row);
	E.dirty++;
}
/*** editor operations ***/
// functs in  editorProcessKeyPress()

void editorInsertChar(int c) {
	// insert c into the position that the cursor is at
	if (E.cy == E.numrows) {
		// line after eof. need to append new row
		editorInsertRow(E.numrows,"", 0);
	}
	editorRowInsertChar(&E.row[E.cy], E.cx, c);
	E.cx++;
}

void editorInsertNewline(){
	if(E.cx == 0){
		editorInsertRow(E.cy, "", 0);
	}
	else{
		erow* row=&E.row[E.cy];
		editorInsertRow(E.cy+1, &row->chars[E.cx], row->size-E.cx);
		// split the line
		// might move memory and invalidate the pointer->reset pointer
		row=&E.row[E.cy];
		row->size=E.cx;
		row->chars[row->size]='\0';
		editorUpdateRow(row);
	}
	E.cy++;
	E.cx=0;
}

void editorDelChar() {
	// delete one char to the left of cursor
	if (E.cy == E.numrows) return;
	// cursor past the end of line -> nothing to delete
	if(E.cx == 0 && E.cy==0) return;
	// cursor at beginning of first line

	erow *row = &E.row[E.cy];
	if (E.cx > 0) {
		editorRowDelChar(row, E.cx - 1);
		E.cx--;
	}
	else{
		// cursur at start of row
		// add row to the end of previous row
		E.cx=E.row[E.cy-1].size;
		editorRowAppendString(&E.row[E.cy-1],row->chars, row->size);
		editorDelRow(E.cy);
		E.cy--;
	}
}

/*** file i/o ***/

char *editorRowsToString(int *buflen) {
	// erow structs -> single string to write on file 
	// buflen: string length
	int totlen = 0;
	int j;
	for (j = 0; j < E.numrows; j++)
	  totlen += E.row[j].size + 1;
	// +1: newline char
	*buflen = totlen;
	char *buf = malloc(totlen);
	char *p = buf;
	for (j = 0; j < E.numrows; j++) {
	  memcpy(p, E.row[j].chars, E.row[j].size);
	  p += E.row[j].size;
	  *p = '\n';
	  p++;
	}
	return buf;
	// expects caller to free() mem
}

void editorOpen(char* filename){ // for rading file
	free(E.filename);
	E.filename = strdup(filename);
	// <string.h> makes a copy & malloc(presumes free() before)

	editorSelectSyntaxHighlight();

	FILE* fp=fopen(filename, "r");
	if(!fp) die("fopen");

	char* line=NULL;
	size_t linecap=0;
	ssize_t linelen;
	//<sys/types.h>
	while((linelen=getline(&line, &linecap, fp))!=-1){// 유용한 getline!
		while(linelen>0 && (line[linelen-1]=='\n' || line[linelen-1]=='\r')) linelen--;
	// each erow represents one line of text, so there’s no use storing a newline character at the end of each one.
		editorInsertRow(E.numrows, line,linelen);
	}
	free(line);
	fclose(fp);
	E.dirty=0;
}


void editorSave() {
	// write string to disk
	if (E.filename == NULL) {
		E.filename = editorPrompt("Save as: %s (ESC to cancel)", NULL);
		if(E.filename==NULL){
			editorSetStatusMessage("Save aborted");
			return;
		}
		editorSelectSyntaxHighlight();
	}

	int len;
	char *buf = editorRowsToString(&len);
	int fd = open(E.filename, O_RDWR | O_CREAT, 0644);
	//<fcntl.h>
	// O_CREAT: create new file it it doesn't already exist
	// O_RDWR: open for read and write
	// 0644: permission: gives owner rw, others: r
	if(fd!=-1){
		if(ftruncate(fd,len)!=-1){
	//<unistd.h>
	// sets file size to len. cut of extra data, add 0 if short
	// not used O_TRUNC to save data if write fails
	// bec O_TRUNC truncates file completely before writing
			if(write(fd,buf,len) == len){
				close(fd);
				//<unistd.h>
				free(buf);
				E.dirty=0;
				editorSetStatusMessage("%d bytes written to disk",len);
				return;
			}
		}
		close(fd);
	}
	// More advanced editors will write to a temporary file, and then rename
	free(buf);
	editorSetStatusMessage("Can't save! I/O error: %s",strerror(errno));
	// strerror: <string.h>
}

/*** find ***/

void editorFindCallback(char* query, int key){
	if(query[0] == '\0') return; // chk
	static int last_match = -1; // index of row the last match was on
	static int direction = 1; // 1: forward, -1: backward

	static int saved_hl_line; // to reset hl after search
	static char* saved_hl = NULL;
	static int idx = -1;
	static int last_direction = 1;

	if(saved_hl){
		memcpy(E.row[saved_hl_line].hl, saved_hl, E.row[saved_hl_line].rsize);
		free(saved_hl);
		saved_hl=NULL;
	}

	if(key=='\r' || key=='\x1b'){
		last_match = -1;
		direction = 1;
		last_direction = 1;
		return;
	} 
	else if(key==ARROW_RIGHT || key==ARROW_DOWN){
		direction =1;
	}
	else if(key == ARROW_LEFT || key == ARROW_UP){
		direction = -1;
	}
	else{
		// only advance to next/pre search when arrow key
		last_match = -1;
		direction = last_direction = 1;
	}
	
	if(last_match == -1) direction = last_direction = 1;
	int current = last_match;
	int i;
	FailFunc(query);
	for(i=0; i<E.numrows;i++){
		if(idx < 0 || idx >= E.row[current].rsize) { // 전에 탐색한 줄에 단어 없음
			current += direction;
			if(current == -1) current = E.numrows-1;
			else if(current == E.numrows) current = 0;
			if(direction == 1) idx = 0; // 현재 줄을 처음으로 탐색하므로 방향에 따라 시작할 인덱스 설정
			else idx = E.row[current].rsize - 1;
		}

		erow* row=&E.row[current];
		if(last_direction != direction){ 
			// 전에 탐색한 방향과 현재 탐색할 방향이 다르면 전에 탐색한 단어를 중복해서 탐색하므로
			// 탐색을 시작할 인덱스(idx)를 옮겨줌.
			if(last_direction == 1) idx -= (strlen(query)+1);
			else idx += strlen(query) + 1;
		}
		char* match = Pmatch(row->render, query, &idx, direction);
		// return pointer to matching substring
		if(match){
			last_direction = direction;
			last_match = current;
			E.cy = current;
			E.cx=editorRowRxToCx(row, match-row->render);
			E.rowoff=E.numrows;
			E.coloff = E.cx - E.screencols/2 ; // chk
			if(E.coloff < 0 ) E.coloff = 0;
			// scrolled to bottom -> matching line scrolled to the top at screen refresh
			saved_hl_line = current; // to reset hl when search ends
			saved_hl = malloc(row->rsize);
// guaranteed to be free()'d <- <esc>/ENTER -> editorPromt() calls callback
			memcpy(saved_hl, row->hl, row->rsize);
			memset(&row->hl[match - row->render], HL_MATCH, strlen(query));
			break;
		}
		else {
			idx = -1;
			last_direction = direction;
		}
	}	
}

void editorFind(){
	int saved_cx = E.cx;
	int saved_cy = E.cy;
	int saved_coloff = E.coloff;
	int saved_rowoff = E.rowoff;

	char* query = editorPrompt("Search: %s (Use ESC/Arrows/Enter)", editorFindCallback);

	if(query){
		free(query);
	}
	else{
		// cursor goes back before search
		E.cx = saved_cx;
	    E.cy = saved_cy;
	    E.coloff = saved_coloff;
	    E.rowoff = saved_rowoff;
	}
}

/*** append buffer ***/

struct abuf{
	char* b;
	int len;
};

#define ABUF_INIT {NULL,0}

void abAppend(struct abuf* ab, const char* s, int len){
	char* new = realloc(ab->b, ab->len+len);
	if(new==NULL) return;
	memcpy(&new[ab->len],s,len);
	ab->b=new;
	ab->len+=len;
}

void abFree(struct abuf* ab){
	free(ab->b);
}

/*** output ***/

void editorScroll(){
	E.rx=0;
	if(E.cy<E.numrows){
		E.rx = editorRowCxToRx(&E.row[E.cy],E.cx);
	}

	if(E.cy < E.rowoff){
		//cursor is above visible window->scrolls up to cursor
		E.rowoff = E.cy;
	}
	if(E.cy >= E.rowoff+E.screenrows){
		//cursor is below visible window
		E.rowoff = E.cy-E.screenrows+1;
	}
	if(E.rx < E.coloff){
		E.coloff=E.rx;
	}
	if(E.rx>=E.coloff+E.screencols){
		E.coloff=E.rx-E.screencols+1;
	}
}

void editorDrawRows(struct abuf* ab) {
	int y;
	for (y = 0; y < E.screenrows; y++) { // what is the size of terminal?
		int filerow = y+E.rowoff; // row of file to display at each y pos
		if(filerow>=E.numrows){
			if(E.numrows==0 && y==E.screenrows/3){
				// welcome message only when text buffer is empty
				char welcome[80];
				int welcomelen = snprintf(welcome, sizeof(welcome),"Kilo editor -- version %s", KILO_VERSION);
				// snprintf <stdio.h>
				if(welcomelen>E.screencols) welcomelen=E.screencols;
				// cut to fit terminal
				int padding=(E.screencols-welcomelen)/2;
				// how far from the left edge of the screen to start printing welcome
				if(padding){
					abAppend(ab, "~",1);
					padding--;
				}
				while(padding--) abAppend(ab, " ", 1);
				// center welcome
				abAppend(ab, welcome, welcomelen);
			}
			else{
				abAppend(ab,"~",1);
			}
		}
		else{
			int len=E.row[filerow].rsize - E.coloff;
			if(len<0) len=0; // user scrolled horizontally past the end of line
			if(len>E.screencols) len=E.screencols;
			char* c = &E.row[filerow].render[E.coloff];
			unsigned char* hl = &E.row[filerow].hl[E.coloff];
			int current_color = -1; // only send esc when color changes
			int j;
			for(j = 0; j < len; j++){
				if(iscntrl(c[j])){
// control char
// Ctrl-A(1) ->A, ..., Ctrl-Z(26) -> Z, Ctrl-@(0) -> @
// others -> ?
					char sym = (c[j] <= 26)? '@' + c[j] : '?';
					abAppend(ab, "\x1b[7m", 4); // inverted colours
					abAppend(ab, &sym, 1);
					abAppend(ab, "\x1b[m", 3); // turns off all text formatting
					if(current_color != -1){
						char buf[16];
						int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", current_color);
						abAppend(ab, buf, clen);
					}
				}
				else if(hl[j] == HL_NORMAL){
					if(current_color != -1){
						abAppend(ab, "\x1b[39m",5); // reset colour;
						current_color = -1;
					}
					abAppend(ab, &c[j], 1);
				}
/*				else if(hl[j] == HL_BRACKET_GOOD || hl[j] == HL_BRACKET_BAD){
//					int color = editorSyntaxToColor(hl[j]);
//					if(color != current_color){
//						current_color = color;
//						char buf[16];
//						int clen = snprintf(buf, sizeof(buf), "\x1b[39;%dm", color);
//						abAppend(ab, buf, clen);
//					}
//					abAppend(ab, &c[j], 1);
//				}*/
				else{
					int color = editorSyntaxToColor(hl[j]);
					if(color != current_color){
						current_color = color;
						char buf[16];
						int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", color);
						abAppend(ab, buf, clen);
					}
					abAppend(ab, &c[j], 1);
				}
			}
			abAppend(ab, "\x1b[39m", 5);
		}

		abAppend(ab, "\x1b[K",3);
		// clear each line as we redraw them
		//K: erase part of the current line
		//2: whole line, 1: part of the line to the left of the cursor, 0: to the right of the cursor(default)
		abAppend(ab, "\r\n", 2);
		// make rrom for status bar last line
	}
}

void editorDrawStatusBar(struct abuf* ab){
	abAppend(ab, "\x1b[7m",4);
	char status[80], rstatus[80];
	int len=snprintf(status, sizeof(status), "%.20s - %d lines %s", E.filename ? E.filename:"[No Name]", E.numrows, E.dirty? "(modified)" : "");
	int rlen=snprintf(rstatus, sizeof(rstatus), "%s | %d/%d", E.syntax? E.syntax->filetype : "no ft", E.cy+1, E.numrows);
	if(len>E.screencols) len=E.screencols;
	abAppend(ab, status, len);
	while(len<E.screencols){
		if(E.screencols-len==rlen){
			abAppend(ab, rstatus, rlen);
			break;
		}
		else{
			abAppend(ab," ",1);
			len++;
		}
	}
	abAppend(ab, "\x1b[m",3);
	abAppend(ab, "\r\n",2);
}

void editorDrawMessageBar(struct abuf* ab){
	abAppend(ab, "\x1b[K",3);
	int msglen=strlen(E.statusmsg);
	if(msglen>E.screencols) msglen=E.screencols;
	if(msglen && time(NULL) - E.statusmsg_time < 5){
		abAppend(ab, E.statusmsg, msglen);
		// statusmsg will disappear when you press a key after 5 sec ( screen refresh only when key pressed
	}
}

void editorRefreshScreen() {
	editorScroll();

	struct abuf ab=ABUF_INIT;
	
	abAppend(&ab, "\x1b[?25l",6);
	// abAppend(&ab, "\x1b[2J",4); -> instead of clearing entire screen, clear each line(\x1b[K)
	// \x1b(escape char/27dec), [, 2, J
	// escape sequence: starts with (escapeChar:27)[
	// J: Erase In Display
	// <esc>[1J : clear the screen up to where the cursor is
	//<esc>[0J : clear the screen from the cursor up to the end of screen(default)
	// <esc>[2J : clear the entire screen
 	abAppend(&ab, "\x1b[H", 3);	
	// H(Cursor Position). arguments: [row num];[column num]. start at 1;1

	editorDrawRows(&ab);
	editorDrawStatusBar(&ab);
	editorDrawMessageBar(&ab);

	char buf[32];
	snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) +1, (E.rx-E.coloff)+1);
	// bec E.cx,cy starts (0,0) while terminal coord starts (1,1)
	// E.cy - E.rowoff bec E.cy refers to the pos of cursor within the text file not on the screen
	abAppend(&ab, buf, strlen(buf));

	abAppend(&ab, "\x1b[?25h",6); 
	//h,l: set mode on,off
	//?25: hide cursor mode

	write(STDOUT_FILENO, ab.b, ab.len);
	abFree(&ab);
}

void editorSetStatusMessage(const char* fmt, ...){
	// ...: variadic function. can take any number of args
	// <stdarg.h>
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(E.statusmsg, sizeof(E.statusmsg),fmt,ap);
	// <stdio.h> reads the format string and calls va_arg() to get each argument
	va_end(ap);
	E.statusmsg_time = time(NULL); // timestamp when we set a status message
}

/*** input ***/

char* editorPrompt(char* prompt, void (*callback)(char*, int)){
	// to prompt for new file's name + search
	// call callback func after each keypress -> search after each keypress
	// filename -> callback: NULL
	size_t bufsize=128;
	char* buf=malloc(bufsize);
	// user input stored

	size_t buflen=0;
	buf[0]='\0';

	while(1){
		editorSetStatusMessage(prompt,buf);
		editorRefreshScreen();

		int c=editorReadKey();
		if(c==DEL_KEY || c==CTRL_KEY('h') || c==BACKSPACE){
			if(buflen!=0) buf[--buflen]='\0';
		}
		else if(c=='\x1b'){
			// user pressed <esc> to cancel prompt
			editorSetStatusMessage("");
			if(callback) callback(buf, c);
			free(buf);
			return NULL;
		}
		else if(c=='\r'){
			// user pressed ENTER
			if(buflen!=0){
				editorSetStatusMessage("");
				if(callback) callback(buf,c);
				return buf;
			}
		}
		else if( !iscntrl(c) && c<128){
			// c isn't special key in editorKey enum
			if(buflen==bufsize-1){
				bufsize*=2;
				buf=realloc(buf,bufsize);
			}	
			buf[buflen++]=c;
			buf[buflen]='\0';
		}
		if(callback) callback(buf,c);
	}
}

void editorMoveCursor(int key){
	erow* row = (E.cy>=E.numrows) ? NULL : &E.row[E.cy];
	// E.cy and E.cx are all allowed one past last of file.

	switch(key){
		case ARROW_LEFT: 
			if(E.cx!=0) {
				E.cx--; 
			}
			else if(E.cy>0){ // <- moves to end of prvious line
				E.cy--;
				E.cx = E.row[E.cy].size; 
			}
			break;
		case ARROW_RIGHT: 
			if(row && E.cx < row-> size){
				E.cx++; 
			}
			else if(row && E.cx == row->size){
				// -> moves to beginning of next line
				E.cy++;
				E.cx =0;
			}
			break;
		case ARROW_UP: 
			if(E.cy!=0){
				E.cy--;
			}
			break;
		case ARROW_DOWN: 
			if(E.cy < E.numrows){
				E.cy++;
			}
			break;
	}

	row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
	int rowlen = row? row->size : 0;
	if(E.cx > rowlen){ // prevents cursor from moving past the end of a line 
		E.cx = rowlen;
	}
}

void editorProcessKeypress() {
	static int quit_times = KILO_QUIT_TIMES;

	int c = editorReadKey();

	switch (c) {
		case '\r':
			// enter key
			editorInsertNewline();
			break;

		case CTRL_KEY('B'):
			{	//if(E.row[E.cy].chars[E.cx] == '{' || E.row[E.cy].chars[E.cx] == '}')	FindBracket();
			for(int i=0;i<2;i++) for(int j=0;j<2;j++) bracket_pair[i][j]=-1;
			FindBracket();
			break;
			}

		case CTRL_KEY('p'):
			{
				WINDOW* win = newwin(10,10, 30,30);
				box(win,0,0);
				wrefresh(win);
				wgetch(win);
				break;
			}
	
		case CTRL_KEY('q'):
			if (E.dirty && quit_times > 0){
				editorSetStatusMessage("WARNING!!! File has unsaved changes. Press Ctrl-Q %d more times to quit.", quit_times);
				quit_times--;
				return;
			}
			write(STDOUT_FILENO, "\x1b[2J", 4);
			write(STDOUT_FILENO, "\x1b[H", 3);
			EndRecommend();
			exit(0); break;
	
		case CTRL_KEY('s'):
			editorSave();
			break;
			
	    case HOME_KEY:
	    	E.cx = 0;
	    	break;
	    case END_KEY:
			if(E.cy<E.numrows) E.cx = E.row[E.cy].size;
			break;

		case CTRL_KEY('f'):
			{
				for(int i=0;i<2;i++) fail[i]=(char*)malloc(sizeof(char));
				editorFind();
				for(int i=0;i<2;i++) free(fail[i]);
				break;
			}
	
		case BACKSPACE:
		case CTRL_KEY('h'):
			// control code 8: backspace ASCII code but dif in modern comp
		case DEL_KEY:
			if(c == DEL_KEY) editorMoveCursor(ARROW_RIGHT);
			editorDelChar();
			break;
	
		case PAGE_UP:
		case PAGE_DOWN:
			{ // {} needed to declare variable
				if(c==PAGE_UP){
					E.cy=E.rowoff;
				}
				else if (c == PAGE_DOWN){
					E.cy = E.rowoff+E.screenrows-1;
					if(E.cy>E.numrows) E.cy=E.numrows;
				}
	
				int times=E.screenrows;
				while(times--) {
					editorMoveCursor(c==PAGE_UP ? ARROW_UP:ARROW_DOWN); 
					// pageup = up arrow till top of screen
				}
			}
			break;
	
	
		case ARROW_UP:
		case ARROW_DOWN:
		case ARROW_LEFT:
		case ARROW_RIGHT: editorMoveCursor(c); break;
	
		case CTRL_KEY('l'):
			//traditionally used to refresh screen in terminal
		case '\x1b':
			// ignered bec key esc seqs such as F1~F12 not handled
			// editorReadKey() reads them as escape
			break;
	
		default:
			editorInsertChar(c);
			break;
	}
	
	quit_times = KILO_QUIT_TIMES; // when pressed other key, reset quit_times
}

/*** init ***/

void initEditor() {
	E.cx=0;// bec c uses indexes from 0
	E.cy=0;
	E.rx=0;
	E.rowoff=0;
	E.coloff=0;
	E.numrows=0;
	E.row=NULL;
	E.dirty=0;
	E.filename=NULL;
	E.statusmsg[0]='\0';
	E.statusmsg_time=0;
	E.syntax = NULL;
	for(int i=0;i<2;i++) for(int j=0;j<2;j++) bracket_pair[i][j]=-1;

	if (getWindowSize(&E.screenrows, &E.screencols) == -1) {
		die("getWindowSize");
	}
	E.screenrows -= 2;
}
