/* includes */

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

/* define macros */

#define CTRL_KEY(x) ((x)&0x1f)
#define KEY_ENTER 10

enum editor_highlight{
	HL_NORMAL=0,
	HL_NUMBER
};

/* data */

typedef struct {
	int size;
	int memory_size;
	char *chars;
	unsigned char *hl;
	/* highlight */
}editor_row;

typedef struct {
	int cx;
	int cy; /* 현재 커서의 위치 */
	int rowoff;
	int coloff;
	int screenrows;
	int screencols;
	int numrows;
	editor_row* row;
}editor_config;

editor_config Editor;

/* numeric */

int min(int a, int b){
	return a<b?a:b;
}

int max(int a, int b){
	return a>b?a:b;
}

/* terminal */

void die(const char* errormsg){
	perror(errormsg);
	exit(1);
}


void disable_raw_mode(){
	noraw();
	//echo();
	endwin();
}

void enable_raw_mode(){
	raw();
	atexit(disable_raw_mode);
	//raw 다음에 atexit가 와야 제대로 raw 탈출된다
	keypad(stdscr, 1);
	noecho();
}

int editor_read_key(){
	int c;
	c=getch();
	return c;
}

void get_cursor_postion(int* rows, int* cols){
	getyx(stdscr,*rows,*cols);
}

void get_window_size(int *rows, int *cols){
	getmaxyx(stdscr, *rows, *cols);
}


/* row operations */

void editor_append_row(char *s, int len){
	/* 텍스트에 길이 len 인 문자열 s를 붙여 준다 */
	Editor.row=realloc(Editor.row, sizeof(editor_row)*(Editor.numrows+1));

	int at=Editor.numrows;
	Editor.row[at].size=len;
	Editor.row[at].chars=malloc(sizeof(char)*Editor.screencols);
	/* 처음엔 스크린 크기만큼 할당. */
	Editor.row[at].memory_size=sizeof(char)*Editor.screencols;
	memcpy(Editor.row[at].chars, s, len);
	Editor.row[at].chars[len]='\0';
	Editor.row[at].hl=NULL;
	Editor.numrows++;
}

void editor_row_insert_char(editor_row *row, int at, int c){
	if(at<0 || at>row->size){at=row->size;}
	row->size++;
	if((row->size)-1==row->memory_size){
		/* 만약 문자 삽입시 메모리 사이즈보다 커지는 경우 2배로 메모리 증가시킴*/
		/* 다음 줄로 가는 기능 or 스크롤을 추가하자 */
		row->chars=realloc(row->chars, sizeof(char)*(row->memory_size)*2);
		row->memory_size*=2;
	}
	memmove(&row->chars[at+1], &row->chars[at], row->size-at+1);
	/* 중간에 삽입할 수도 있으므로, at 뒤의 데이터를 한칸 밀어준다 */
	row->chars[at]=c;
}

void editor_insert_char(int c){
	if(Editor.cy==Editor.numrows){
		editor_append_row("",0);
	}
	editor_row_insert_char(&Editor.row[Editor.cy], Editor.cx, c);
	Editor.cx++;
}

void editor_row_delete_char(editor_row *row, int at){
	if(at<0 || at>=row->size){return;}
	memmove(&row->chars[at], &row->chars[at+1], row->size-at);
	row->size--;
}

void editor_delete_char(){
	if(Editor.numrows<Editor.cy){return;}
	editor_row *row=&Editor.row[Editor.cy];
	if(Editor.cx>0){
		editor_row_delete_char(row, Editor.cx-1);
		Editor.cx--;
	}
}

/* file i/o */

void editor_open(char *filename){
	FILE* fp=fopen(filename, "r");
	if(!fp){die("fopen");}

	char *line=NULL;
	size_t linecap=0;
	int linelen;
	//linelen=getline(&line, &linecap, fp);
	while((linelen=getline(&line, &linecap, fp))!=-1){
		while(linelen>0 && (line[linelen-1]=='\n' || line[linelen-1]=='\r')){
			linelen--;
		}
		editor_append_row(line, linelen);
	}
	free(line);
	fclose(fp);
}

/* append buffer */

typedef struct abuf{
	char *b;
	int len;
}abuf;

#define ABUF_INIT {NULL,0}

void ab_append(abuf *ab, const char *s, int len){
	/* abuf에 길이 len인 문자열 s추가 */
	char *new=realloc(ab->b, ab->len+len);
	if(new==NULL){return;}
	memcpy(&new[ab->len], s, len);
	ab->b=new;
	ab->len+=len;
}

void ab_free(abuf *ab){
	free(ab->b);
}


/* output */

void editor_scroll(){
	if(Editor.cy<Editor.rowoff){
		Editor.rowoff=Editor.cy;
		/* 커서가 우리 화면보다 위에 있을 때 */
	}
	if(Editor.cy>=Editor.rowoff+Editor.screenrows){
		/* 커서가 우리 화면보다 아래 있을 때 */
		Editor.rowoff=Editor.cy-Editor.screenrows+1;
	}
	if(Editor.cx<Editor.coloff){
		Editor.coloff=Editor.cx;
	}
	if(Editor.cx>=Editor.coloff+Editor.screencols){
		Editor.coloff=Editor.cx-Editor.screencols+1;
	}
}

void editor_draw_rows(){
	/* 시작 화면 인쇄 */
	int y;
	for(y=0;y<Editor.screenrows;y++){
		if(Editor.numrows==0 && y==Editor.screenrows/3){
			char welcome[80]="This is the text editor";
			char start_info[80]="Press ENTER to start";
			int welcome_len=strlen(welcome);
			int start_info_len=strlen(start_info);
			if(welcome_len>Editor.screencols){welcome_len=Editor.screencols;}
			if(start_info_len>Editor.screencols){start_info_len=Editor.screencols;}
			mvwprintw(stdscr, y, (Editor.screencols-welcome_len)/2, "%s", welcome);
			mvwprintw(stdscr, y+1, (Editor.screencols-start_info_len)/2, "%s", start_info);
		
		}
		mvwprintw(stdscr,y,0,"~");
		mvwprintw(stdscr,y,Editor.screencols-1,"~");
	}
}

void editor_print_rows(){
	int r, c, cur;
	int max_row=min(Editor.numrows,Editor.screenrows);
	//(Editor.numrows>Editor.screenrows?Editor.screenrows:Editor.numrows);
	int max_col;
	/* 인쇄할 수 있는 최대의 줄 */
	for(r=Editor.rowoff;r<Editor.rowoff+max_row;r++){
		/* rowoff 부터 시작해서 max_row개의 줄을 인쇄한다 */
		max_col=min(Editor.row[r].size, Editor.screencols);
		for(c=Editor.coloff;c<Editor.coloff+max_col;c++){
			cur=Editor.row[r].chars[c];
				if(isdigit(cur)){attron(COLOR_PAIR(HL_NUMBER));}
				mvwprintw(stdscr, r-Editor.rowoff, c-Editor.coloff, "%c", cur);
				if(isdigit(cur)){attroff(COLOR_PAIR(HL_NUMBER));}
		}
	}
	mvwprintw(stdscr, 10,10,"%d",Editor.numrows);
}

void editor_refresh_screen(){
	//editor_draw_rows();
	wclear(stdscr);
	//editor_scroll();
	editor_print_rows();
	wmove(stdscr, Editor.cy-Editor.rowoff, Editor.cx-Editor.coloff);
	/* 커서를 적절한 위치로 이동 */
}

/* input */

void editor_move_cursor(int key){
	/* 끝줄에서의 오른쪽키 처리 다시 확인할 것 */
	editor_row *row=(Editor.cy>=Editor.numrows?NULL:&Editor.row[Editor.cy]);

	switch(key){

		case KEY_UP:
		if(Editor.cy>0){
			Editor.cy--;
			if(Editor.cx>Editor.row[Editor.cy].size){
				/* 이동한 줄이 더 사이즈가 작을 수도 있다. 그때의 처리 */
				Editor.cx=Editor.row[Editor.cy].size;
			}
		}
		break;

		case KEY_DOWN:
		if(Editor.cy<Editor.numrows-1){
			Editor.cy++;
			if(Editor.cx>Editor.row[Editor.cy].size){
				Editor.cx=Editor.row[Editor.cy].size;
			}
		}
		break;

		case KEY_LEFT:
		if(Editor.cx!=0){
			Editor.cx--;
		}
		else if(Editor.cy>0){
			Editor.cy--;
			Editor.cx=Editor.row[Editor.cy].size;
		}
		break;

		case KEY_RIGHT:
		if(row && Editor.cx<row->size){
			/* 일정 사이즈 이상으로 갈 수 없게 통제해 두자 */
			Editor.cx++;
		}
		else if(row && Editor.cx==row->size && Editor.cy<Editor.numrows-1){
			Editor.cy++;
			Editor.cx=0;
		}
		break;
	}

	row=(Editor.cy>=Editor.numrows)?NULL:&Editor.row[Editor.cy];
	int rowlen=row?row->size:0;
	if(Editor.cx>rowlen){
		Editor.cx=rowlen;
	}

}


void editor_process_key_press(){
	int c=editor_read_key();
	switch(c){
		case '\r':
		/* TODO */
		break;

		case CTRL_KEY('q'):
		//disable_raw_mode();
		//endwin();
		exit(0);
		break;

		case KEY_UP:
		case KEY_DOWN:
		case KEY_LEFT:
		case KEY_RIGHT:
		editor_move_cursor(c);
		break;

		case KEY_PPAGE:
		case KEY_NPAGE:
		{
			int times=Editor.screenrows;
			while(times--){
				editor_move_cursor(c==KEY_PPAGE?KEY_UP:KEY_DOWN);
			}
		}
		break;

		case KEY_HOME:
		Editor.cx=0;
		break;

		case KEY_END:
		Editor.cx=Editor.screencols-1;
		break;

		case KEY_BACKSPACE:
		case CTRL_KEY('h'):
		case KEY_DC:
		if(c==KEY_DC){wmove(stdscr, Editor.cy, Editor.cx+1);}
		editor_delete_char();
		break;

		case CTRL_KEY('l'):
		/* TODO */
		break;

		case KEY_ENTER:
		/* 줄 중간에서 엔터 치는 것 추가해 줘야 함. 엔터처럼 동작하게..*/
		editor_append_row("",0);
		Editor.cy++;
		Editor.cx=0;
		break;

		default:
		editor_insert_char(c);
		break;
	}


	/*if(iscntrl(c)){
		printw("%d\n",c);
	}
	else{
		printw("%d ('%c')\n",c,c);
	}
	if(c==CTRL_KEY('z')){
		exit(0);
	}*/
}



/* init */

void init_editor(){
	Editor.cx=0;
	Editor.cy=0;
	Editor.rowoff=0;
	Editor.coloff=0;
	Editor.numrows=0;
	Editor.row=NULL;

	//scrollok(stdscr, 1);
	get_window_size(&Editor.screenrows, &Editor.screencols);
	//getmaxyx(stdscr, Editor.screenrows, Editor.screencols);
	if(Editor.screenrows==-1 && Editor.screencols==-1){
		die("Window Size");
	}
	/*printw("%d %d\n", Editor.screenrows, Editor.screencols);*/
}

int main(int argc, char *argv[]){
	int c;

	initscr();
	enable_raw_mode();
	init_editor();

	if(argc>=2){
		editor_open(argv[1]);
	}
	else {
		/* 만약 열린 파일 없으면 시작 화면 띄워주기 */
		while(1){
			editor_draw_rows();
			wrefresh(stdscr);
			c=getch();
			if(c==KEY_ENTER){break;}
			/* 엔터 누르면 화면 청소후 시작 */
		}
	}
	wclear(stdscr);
	//wprintw(stdscr, "hello");
	start_color();
	init_pair(HL_NUMBER, COLOR_GREEN, COLOR_BLACK); //cyan색 글씨. 숫자 하이라이팅
	while(1){
		editor_refresh_screen();
		editor_process_key_press();
	}

	refresh();
	//noraw();
	//getch();
	endwin();

	return 0;
}

