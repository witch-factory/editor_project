/* includes */
// modf: modified parts
// dbg: ?붾쾭源낆슜 

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <locale.h>

/*** defines ***/

#define CTRL_KEY(x) ((x)&0x1f)
#define KEY_ENTER 10
#define KEY_ESC 27

#define TAB_STOP 4
#define QUIT_TIMES 2

enum editor_highlight {
	HL_NORMAL = 0,
	HL_COMMENT,
	HL_MLCOMMENT,
	HL_KEYWORD1,
	HL_KEYWORD2,
	HL_STRING,
	HL_NUMBER,
	HL_MATCH,
	HL_PAIR,
	HL_NOTPAIR
};


#define HL_HIGHLIGHT_NUMBERS (1<<0)
#define HL_HIGHLIGHT_STRINGS (1<<1)


/*** color define ***/
#define COLOR_BLACK 0
#define COLOR_RED 1
#define COLOR_GREEN 2
#define COLOR_YELLOW 3
#define COLOR_BLUE 4
#define COLOR_MAGENTA 5
#define COLOR_CYAN 6
#define COLOR_WHITE 7
#define COLOR_GRAY 8
#define COLOR_CHERRY 9
#define COLOR_LIGHTGREEN 10
#define COLOR_BEIGE 11
#define COLOR_LIGHTBLUE 12
#define COLOR_PINK 13
#define COLOR_SKYBLUE 14
#define COLOR_IVORY 15


/* trie structure for autocomplete */

#define CHAR_SIZE 96
/* 32~127 ASCII code is used. */
#define CHAR_TO_INDEX(c) ((int)c - (int)' ')

typedef struct trie {
	int is_leaf;
	int branch;
	int words;
	struct trie* ch[CHAR_SIZE];
}trie;

/*** data ***/

typedef struct {
	char *file_type;
	char **file_match;
	char **keywords;
	char* single_line_comment_start;
	char* multi_line_comment_start;
	char* multi_line_comment_end;
	int flags;
}editor_syntax;

typedef struct {
	int idx;
	int size;
	int rsize;
	int msize; /* dynamic memory size */
	char *chars;
	char *render;
	char *hl;
	/* highlight */
	int hl_open_comment;
}editor_row;

typedef struct {
	int cx;
	int cy; /* ?꾩옱 而ㅼ꽌???꾩튂 */
	int rx; /* render field index. 留뚯빟 ??씠 ?덉쑝硫???씠 李⑥??섎뒗 怨듦컙 ?뚮Ц??rx媛 cx蹂대떎 ??而ㅼ쭚*/
	int rowoff;
	int coloff;
	int screenrows;
	int screencols;
	int numrows;
	char *filename;
	char status_msg[80];
	time_t status_msg_time;
	/* ?곹깭 硫붿떆吏? 洹???꾩뒪?ы봽 */
	editor_syntax *syntax;
	editor_row* row;
	int dirty;
    trie* auto_complete;
	/* ?섏젙以??뚮옒洹?*/
}editor_config;

editor_config Editor;

/*** bracket check ***/

int bracket_pair[2][2];

/*** file types ***/

char *C_highlight_extensions[] = { ".c", ".h", ".cpp", NULL };

char *C_highlight_keywords[] = { 
	/* C keywords */
	"auto","break","case","continue","default","do","else","enum",
	"extern","for","goto","if","register","return","sizeof","static",
	"struct","switch","typedef","union|","volatile","while", "define", "NULL",

	/* C++ Keywords */
	"alignas","alignof","and","and_eq","asm","bitand","bitor","class",
	"compl","constexpr","const_cast","deltype","delete","dynamic_cast",
	"explicit","export","false","friend","inline","mutable","namespace",
	"new","noexcept","not","not_eq","nullptr","operator","or","or_eq",
	"private","protected","public","reinterpret_cast","static_assert",
	"static_cast","template","this","thread_local","throw","true","try",
	"typeid","typename","virtual","xor","xor_eq",

	/* types */
	"int|", "long|", "double|", "float|", "char|", "unsigned|", "signed|",
	"void|", "short", "auto|", "const|", "bool|" ,"#include|","FILE|", NULL
};

editor_syntax HLDB[] = {
	{
		"c",
		C_highlight_extensions,
		C_highlight_keywords,
		"//",
		"/*", "*/",
		HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS
	},
};

#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))


/*** stack ***/

#define WORDMAX 30 // max length of word -> subwindow width is WORDMAX+2
#define SHOWCNT 5 // max number of words shown -> subwindow height is SHOWCNT+2

typedef struct char_node {
	char data;
	struct char_node* next;
}char_node;

typedef struct word_node {
	char* word;
	struct word_node* next;
	struct word_node* prev;
}word_node;

word_node* list = NULL;
int list_cnt=0;

int wordcnt = 0; // dbg
char word[WORDMAX]; 
FILE* fp_save;


/*** prototypes ***/

void editor_set_status_message(const char *fmt, ...);
void editor_refresh_screen();
char *editor_prompt(char *prompt, void(*callback)(char*, int));

char* fail[2] = { NULL, }; /* ?ㅽ뙣?⑥닔. ?뺣갑????갑???????덉쓬 */
void fail_func(char*);

void find_bracket();

void insert_list(char* start, int len);
void end_recommend(); // 由ъ뒪?몃? save.txt????? 遺덊븘??
void erase_list();
char* word_recommend(WINDOW*);

/*** terminal ***/

void die(const char *s) {
	wclear(stdscr);
	perror(s);
	//end_recommend();
	fprintf(fp_save, "%d", wordcnt); // dbg

	fclose(fp_save); // dbg
	exit(1);
}


void disable_raw_mode() {
	noraw();
	wclear(stdscr);
	//echo();
	endwin();
}


void enable_raw_mode() {
	raw();
	atexit(disable_raw_mode);
	//raw ?ㅼ쓬??atexit媛 ????쒕?濡?raw ?덉텧?쒕떎
	keypad(stdscr, 1);
	noecho();
}


int editor_read_key() {
	int c = getch();
	if (c == EOF) { die("read"); }
	return c;
}


int get_cursor_position(int *rows, int *cols) {
	getyx(stdscr, *rows, *cols);
	if (*rows == -1 && *cols == -1) { return -1; }
	return 0;
}


int get_window_size(int *rows, int *cols) {
	getmaxyx(stdscr, *rows, *cols);
	if (*rows == -1 && *cols == -1) { return -1; }
	return 0;
}


/*** C++ string operation imitate ***/

char* string_append(char* str, char part) {
    //append part character to string
	int i, len;
	char* temp;

	len = strlen(str);

	temp = malloc(sizeof(char) * (len + 2));
	if (temp) {
		for (i = 0; i < len; i++) {
			temp[i] = str[i];
		}
		temp[i] = part;
		temp[i + 1] = '\0';
	}

	//printf("%d\n", len);
	return temp;
}

char* string_pop_back(char* str) {
	int i, len;
	char* temp;

	len = strlen(str);

	temp = malloc(sizeof(char) * len);
	if (temp) {
		for (i = 0; i < len - 1; i++) {
			temp[i] = str[i];
		}
		temp[len - 1] = '\0';
	}
	return temp;
}


char* string_copy(char* original) {
	//return the pointer of the copy of the original string 
	int i;
	int len = strlen(original);
	char* temp;

	temp = malloc(sizeof(char) * (len + 1));

	for (i = 0; i < len; i++) {
		temp[i] = original[i];
	}
	temp[i] = '\0';
	return temp;
}



/*** trie functions ***/

trie* get_new_trie_node() {
	int i;
	trie* new_node = (trie*)malloc(sizeof(trie));

	new_node->is_leaf = 0;
	new_node->branch = 0;
	new_node->words = 0;
	for (i = 0; i < CHAR_SIZE; i++) {
		new_node->ch[i] = NULL;
	}
	return new_node;
}


void trie_insert_string(trie* head, char* str) {
	/* insert string to the head trie */
	trie* cur;

	cur = head;

	while (*str) {
		if (cur->ch[CHAR_TO_INDEX(*str)] == NULL) {
			cur->ch[CHAR_TO_INDEX(*str)] = get_new_trie_node();
			cur->branch++;
		}
		cur->words++;
		cur = cur->ch[CHAR_TO_INDEX(*str)];
		//go to next node

		str++;
	}
	cur->branch++;
	cur->is_leaf = 1;

	/* indicate the end of the word(is_leaf) */
}


int trie_search(trie* head, char* str) {
	/* if string is found in the trie, return 1 otherwise 0 */
	trie* cur;

	if (head == NULL) {
		return 0;
	}

	cur = head;
	while (*str) {
		cur = cur->ch[CHAR_TO_INDEX(*str)];

		if (cur == NULL) { return 0; }
		//紐?李얠쓬

		str++;
	}

	return ((cur != NULL && cur->is_leaf) ? 1 : 0);
}


int has_children(trie* cur) {
	/* return 1 if there exists children, otherwise return 0 */
	int i;
	for (i = 0; i < CHAR_SIZE; i++) {
		if (cur->ch[i]) {
			return 1;
		}
	}
	return 0;
}


int trie_deletion(trie** cur, char* str) {
	/* delete str in cur trie */
	/* WARNING : need to give the pointer of trie in delete function */
	if (*cur == NULL) {
		return 0;
	}

	if (*str) {
		if (*cur != NULL && (*cur)->ch[CHAR_TO_INDEX(*str)] != NULL && trie_deletion(&((*cur)->ch[CHAR_TO_INDEX(*str)]), str + 1) && (*cur)->is_leaf == 0) {
			if (!has_children(*cur)) {
				free(*cur);
				*cur = NULL;
				return 1;
			}
			else {
				return 0;
			}
		}
	}

	if (*str == '\0' && (*cur)->is_leaf) {
		if (!has_children(*cur)) {
			free(*cur);
			*cur = NULL;
			return 1;
		}
		else {
			//?먯떇???덈떎
			(*cur)->is_leaf = 0;
			return 0;
		}
	}
	return 0;
}


void free_trie(trie* node) {
	/* used to free the dynamically allocated trie nodes. recursive */
	int i;

	if (node->is_leaf == 0) {
		for (i = 0; i < CHAR_SIZE; i++) {
			if (node->ch[i] != NULL) {
				free_trie(node->ch[i]);
			}
		}
	}

	if (node != NULL) {
		free(node);
	}
}


/*** autocomplete functions ***/

void suggestion_by_prefix(trie* root, char* prefix) {
	//find the words starts with prefix string, with length len
	int i;
	//int len;
	//len = strlen(prefix);

	if (root->is_leaf) {
        insert_list(prefix, strlen(prefix));
        //trie_insert_string(Editor.auto_complete, prefix);
	}

	if (has_children(root) == 0) {
		//no child. That is, no word in trie starts with this prefix
		return;
	}

	for (i = 0; i < CHAR_SIZE; i++) {
		if (root->ch[i]) {
			prefix = string_append(prefix, ' ' + i);
			suggestion_by_prefix(root->ch[i], prefix);
			prefix = string_pop_back(prefix);
		}
	}
}


int auto_complete_suggestion(trie* root, char* query) {
	/* used to autocomplete */
	trie* crawl = root;

	int level;
	int len = strlen(query);
	int index;
	int is_word, is_last;
	char* prefix=NULL;

	for (level = 0; level < len; level++) {
		index = CHAR_TO_INDEX(query[level]);

		//??prefix濡??쒖옉?섎뒗 寃??녿떎
		if (!crawl->ch[index]) {
			return 0;
		}

		crawl = crawl->ch[index];

	}

	is_word = crawl->is_leaf == 1 ? 1 : 0;
	is_last = (has_children(crawl) == 0) ? 1 : 0;

	/* ???몃뱶媛 ?앹씠怨? 洹??ㅼ뿉 ?댁뼱吏???몃뱶媛 ?놁쓣 ???몄뇙 */
	if (is_word && is_last) {
        //insert_list(query, strlen(query));
		//printf("%s", query);
		return -1;
	}

	if (is_last == 0) {
		prefix = string_copy(query);
		suggestion_by_prefix(crawl, prefix);
		return 1;
	}

	return 0;
}



/*** syntax highlighting ***/

int is_separator(int c) {
	if (isspace(c)) { return 1; }
	if (c == '\0') { return 2; }
	if (strchr(",.()+-/=~%<>[];", c) != NULL) { return 2; }
	if (strchr("*&", c) != NULL) { return 3; } /*modf*/
	return 0;
}

/* ?됱쓽 ?섏씠?쇱씠??踰꾪띁 ?낅뜲?댄듃 */
void editor_update_syntax(editor_row* row) {
	int i, j, idx, prev_sep, name_keyword; 
	int in_string;
	int in_comment;
	char c;
	char prev_hl;
	char* single_line_comment;
	int single_line_comment_len;

	char* multi_line_comment_start;
	char* multi_line_comment_end;
	int multi_line_comment_start_len;
	int multi_line_comment_end_len;

	char** keywords;
	int kwlen, kw2;

	int changed;

	row->hl = realloc(row->hl, row->rsize);
	memset(row->hl, HL_NORMAL, row->rsize);

	if (Editor.syntax == NULL) { return; }

	keywords = Editor.syntax->keywords;

	single_line_comment = Editor.syntax->single_line_comment_start;
	single_line_comment_len = single_line_comment ? strlen(single_line_comment) : 0;

	multi_line_comment_start = Editor.syntax->multi_line_comment_start;
	multi_line_comment_end = Editor.syntax->multi_line_comment_end;

	multi_line_comment_start_len = multi_line_comment_start ? strlen(multi_line_comment_start) : 0;
	multi_line_comment_end_len = multi_line_comment_end ? strlen(multi_line_comment_end) : 0;


	prev_sep = 1;
	in_string = 0;
	/* 臾몄옄???덉뿉 ?덈뒗吏瑜??섑???*/
	in_comment = (row->idx > 0 && Editor.row[row->idx - 1].hl_open_comment);
	name_keyword = 0;

	i = 0;
	while (i < row->rsize) {
		c = row->render[i];

		prev_hl = (i > 0) ? row->hl[i - 1] : HL_NORMAL;


		if (single_line_comment_len && !in_string && !in_comment) {
			if (!strncmp(&row->render[i], single_line_comment, single_line_comment_len)) {
				for (j = 0; j < row->rsize - i; j++) {
					row->hl[i + j] = HL_COMMENT;
				}
				break;
			}
		}


		if (multi_line_comment_start_len && multi_line_comment_end_len && !in_string) {
			/* ?щ윭 以?二쇱꽍???대떦?섎뒗 遺遺꾩쓣 媛먯??쒕떎 */
			if (in_comment) {
				row->hl[i] = HL_MLCOMMENT;
				if (!strncmp(&row->render[i], multi_line_comment_end, multi_line_comment_end_len)) {
					for (j = 0; j < multi_line_comment_end_len; j++) {
						row->hl[i + j] = HL_MLCOMMENT;
					}
					i += multi_line_comment_end_len;
					in_comment = 0;
					prev_sep = 1;
					continue;
				}
				else {
					i++;
					continue;
				}
			}
			else if (!strncmp(&row->render[i], multi_line_comment_start, multi_line_comment_start_len)) {
				for (j = 0; j < multi_line_comment_start_len; j++) {
					row->hl[i + j] = HL_MLCOMMENT;
				}
				i += multi_line_comment_start_len;
				in_comment = 1;
				continue;
			}
		}


		if (Editor.syntax->flags & HL_HIGHLIGHT_STRINGS) {
			if (in_string) {
				row->hl[i] = HL_STRING;

				if (c == '\\' && i + 1 < row->rsize) {
					/* ?댁뒪耳?댄봽 臾몄옄 臾댁떆 */
					row->hl[i + 1] = HL_STRING;
					i += 2;
					continue;
				}

				if (c == in_string) { in_string = 0; }
				/* ?ル뒗 ???묒? ?곗샂??媛먯??섎㈃ 臾몄옄?댁씠 ?앸궃 寃껋씠??*/
				i++;
				prev_sep = 1;
				continue;
			}
			else {
				if (c == '"' || c == '\'') {
					/* ?щ뒗 ?곗샂??媛먯??섎㈃ 洹멸구 ??ν빐 ?붾떎 */
					in_string = c;
					row->hl[i] = HL_STRING;
					i++;
					continue;
				}
			}
		}


		if (Editor.syntax->flags & HL_HIGHLIGHT_NUMBERS) {
			if ((isdigit(c) && (prev_sep || prev_hl == HL_NUMBER))
				|| (c == '.' && prev_hl == HL_NUMBER)) {
				/* ?꾩옱 臾몄옄瑜?媛뺤“?쒖떆??寃껋씤媛 */
				/* ?뚯닔?먮룄 ?ы븿?댁꽌 媛뺤“ ?쒖떆瑜??댁???*/
				row->hl[i] = HL_NUMBER;
				i++;
				prev_sep = 0;
				continue;
			}
		}


		if (prev_sep) { /* keywords highlight */
			if (name_keyword) {
				switch (is_separator(c)) { // is_separator ?섏젙: return 0: separator ?꾨떂, return 1 -> 怨듬갚, return 2 -> ?섎㉧吏 separator
				case 2:
					name_keyword = 0;
					break;  // keyword2媛 蹂???⑥닔 ?좎뼵???꾨땶 ?ㅻⅨ ?⑸룄濡??ъ슜????(eg) sizeof(char)
				case 0:
				{
					int a;
					// ?⑥닔紐?蹂?섎챸 湲몄씠 李얘린
					for (a = 0; i + a < row->rsize && !is_separator(row->render[i + a]); a++);
					strncpy(word, &row->render[i], a);
					word[a] = '\0';
					wordcnt++;
					fprintf(fp_save, "[%s]\n", word);
					// insert it to trie
                    trie_insert_string(Editor.auto_complete, word);
                    //?뚯떛???⑥닔紐낆쓣 ?몃씪?댁뿉 ?쎌엯
					//insert_list(&row->render[i], a); // ?⑥닔紐?蹂?섎챸 由ъ뒪?몄뿉 ?쎌엯
					name_keyword = 0; prev_sep = 0;
					i += a;
					continue;
				}
				default: break;
				}
			}
			else {
				for (j = 0; keywords[j]; j++) {
					kwlen = strlen(keywords[j]);
					kw2 = keywords[j][kwlen - 1] == '|';
					if (kw2) { kwlen--; }

					if (!strncmp(&row->render[i], keywords[j], kwlen) && is_separator(row->render[i + kwlen])) {
						/* 留뚯빟 ?ㅼ썙?쒓? ?덇퀬 ?ㅼ썙???ㅼ뿉 ?꾩뼱?곌린媛 ?덉쑝硫?*/
						for (idx = 0; idx < kwlen; idx++) {
							row->hl[i + idx] = kw2 ? HL_KEYWORD2 : HL_KEYWORD1;
						}
						i += kwlen;
						if (kw2) { name_keyword = 1; }
						break;
					}
				}
			}

			if (keywords[j] != NULL) {
				prev_sep = 0;
				continue;
			}
		}

		prev_sep = is_separator(c);
		i++;
	}

	changed = (row->hl_open_comment != in_comment);
	/* ?щ윭 以?二쇱꽍???앸궗?붿? ?꾨땶吏 */
	row->hl_open_comment = in_comment;
	if (changed&&row->idx + 1 < Editor.numrows) {
		editor_update_syntax(&Editor.row[row->idx + 1]);
	}

}

/* 踰꾨젮吏??⑥닔 */
int editor_syntax_to_color(int hl) {
	switch (hl) {
	case HL_NUMBER:return 31;
	default:return 37;
	}
}

/* ?뚯씪 ?대쫫??留ㅼ묶?댁꽌 ?좏깮???섏씠?쇱씠??遺遺꾩쓣 遺덈윭?⑤떎 */
void editor_select_syntax_highlight() {
	char *ext;
	int i, is_ext, filerow;
	unsigned int j;

	Editor.syntax = NULL;
	if (Editor.filename == NULL) { return; }
	/* no filename */

	ext = strrchr(Editor.filename, '.');

	for (j = 0; j < HLDB_ENTRIES; j++) {
		editor_syntax *s = &HLDB[j];
		i = 0;
		while (s->file_match[i]) {
			is_ext = (s->file_match[i][0] == '.');
			if ((is_ext && ext && !strcmp(ext, s->file_match[i])) ||
				(!is_ext && strstr(Editor.filename, s->file_match[i]))) {
				Editor.syntax = s;

				for (filerow = 0; filerow < Editor.numrows; filerow++) {
					editor_update_syntax(&Editor.row[filerow]);
				}
				return;
			}
			i++;
		}
	}
}


/*** row operations ***/

int editor_row_cx_to_rx(editor_row *row, int cx) {
	int j, rx = 0;
	for (j = 0; j < cx; j++) {
		if (row->chars[j] == '\t') {
			rx += (TAB_STOP - 1) - (rx%TAB_STOP);
		}
		rx++;
	}
	return rx;
}


int editor_row_rx_to_cx(editor_row *row, int rx) {
	int cur_rx = 0, cx;
	for (cx = 0; cx < row->size; cx++) {
		if (row->chars[cx] == '\t') {
			cur_rx += (TAB_STOP - 1) - (cur_rx%TAB_STOP);
		}
		cur_rx++;
		if (cur_rx > rx) { return cx; }
	}
	return cx;
}


void editor_update_row(editor_row* row) {
	/* ??泥섎━?섍린 */
	int tabs = 0;
	int j, idx = 0;

	for (j = 0; j < row->size; j++) {
		if (row->chars[j] == '\t') { tabs++; }
	}

	free(row->render);
	row->render = malloc(row->size + tabs * (TAB_STOP - 1) + 1);

	for (j = 0; j < row->size; j++) {
		if (row->chars[j] == '\t') {
			row->render[idx++] = ' ';
			while (idx % (TAB_STOP) != 0) { row->render[idx++] = ' '; }
		}
		else {
			row->render[idx++] = row->chars[j];
		}
	}
	row->render[idx] = '\0';
	/* idx??row->render??蹂듭궗??臾몄옄???? 留덉?留됱뿉 ???쎌엯 */
	row->rsize = idx;

	editor_update_syntax(row);
}


void editor_insert_row(int at, char *s, int len) {
	/* at踰덉㎏ 以꾩뿉 ??湲몄씠 len ??臾몄옄??s瑜?遺숈뿬 以??*/
	/* ?먮옒??append_row ???*/
	int j;
	int chars_len;
	if (at<0 || at>Editor.numrows) { return; }

	Editor.row = realloc(Editor.row, sizeof(editor_row)*(Editor.numrows + 1));
	memmove(&Editor.row[at + 1], &Editor.row[at], sizeof(editor_row)*(Editor.numrows - at));

	for (j = at + 1; j <= Editor.numrows; j++) {
		Editor.row[j].idx++;
	}
	/* 洹???以꾨뱾??以꾨쾲?몃? 1??利앷??쒖폒以?*/

	Editor.row[at].idx = at;
	/* 以??쎌엯??珥덇린??*/

	//int at=Editor.numrows;
	Editor.row[at].size = len;

	chars_len = Editor.screencols;
	while (chars_len < len) {
		chars_len *= 2;
	}
	Editor.row[at].chars = malloc(sizeof(char)*(chars_len + 1));
	Editor.row[at].msize = chars_len + 1;
	/* 泥섏쓬???ㅽ겕由??ш린留뚰겮 ?좊떦. */
	memcpy(Editor.row[at].chars, s, len);
	Editor.row[at].chars[len] = '\0';

	Editor.row[at].rsize = 0;
	Editor.row[at].render = NULL;
	Editor.row[at].hl = NULL;
	Editor.row[at].hl_open_comment = 0;
	editor_update_row(&Editor.row[at]);

	Editor.numrows++;
	Editor.dirty++;
}


void editor_free_row(editor_row *row) {
	free(row->render);
	free(row->chars);
	free(row->hl);
}


void editor_delete_row(int at) {
	int j;
	if (at < 0 || at >= Editor.numrows) { return; }
	editor_free_row(&Editor.row[at]);

	memmove(&Editor.row[at], &Editor.row[at + 1], sizeof(editor_row)*(Editor.numrows - at - 1));
	/* 以꾩쓣 ?욎쑝濡??섎굹 ??꺼?ㅺ린 */

	for (j = at; j < Editor.numrows - 1; j++) { Editor.row[j].idx--; }
	/* 以꾨쾲???섎굹???밴꺼二쇨린 */
	Editor.numrows--;
	Editor.dirty++;
}


void editor_row_insert_char(editor_row *row, int at, int c) {
	if (at<0 || at>row->size) { at = row->size; }
	if ((row->size + 2) >= row->msize) {
		/* 筌롫뗀?덄뵳?? ??筌△뫀??2獄쏄퀡以?realloc. 筌롫뗀?덄뵳??온????μ몛???袁る퉸 ??륁젟??*/
		row->chars = realloc(row->chars, (row->msize) * 2);
		row->msize *= 2;
	}
	memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
	/* 以묎컙???쎌엯???섎룄 ?덉쑝誘濡? at ?ㅼ쓽 ?곗씠?곕? ?쒖뭏 諛?댁???*/
	row->size++;
	row->chars[at] = c;
	editor_update_row(row);
	Editor.dirty++;
}


void editor_row_append_string(editor_row* row, char* s, size_t len) {
	/* 以??앹뿉 湲몄씠 len??臾몄옄??s瑜?遺숈씤??*/
	row->chars = realloc(row->chars, row->size + len + 1);
	row->msize = row->size + len + 1;
	memcpy(&row->chars[row->size], s, len);
	row->size += len;
	row->chars[row->size] = '\0';
	editor_update_row(row);
	Editor.dirty++;
}


void editor_row_delete_char(editor_row *row, int at) {
	if (at < 0 || at >= row->size) { return; }
	memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
	row->size--;
	editor_update_row(row);
	Editor.dirty++;
}

/*** editor operations ***/

void editor_insert_char(int c) {
	/* 而ㅼ꽌 ?꾩튂??臾몄옄 ?섎굹 異붽? */
	if (Editor.cy == Editor.numrows) {
		/* 留덉?留?以꾩뿉 以??섎굹 異붽??섎뒗 寃쎌슦 */
		editor_insert_row(Editor.numrows, "", 0);
	}
	editor_row_insert_char(&Editor.row[Editor.cy], Editor.cx, c);
	Editor.cx++;
}


void editor_insert_newline() {
	editor_row* row;
	if (Editor.cx == 0) {
		editor_insert_row(Editor.cy, "", 0);
	}
	else {
		/* split line */
		row = &Editor.row[Editor.cy];
		editor_insert_row(Editor.cy + 1, &row->chars[Editor.cx], (row->size) - Editor.cx);
		row = &Editor.row[Editor.cy];
		row->size = Editor.cx;
		row->chars[row->size] = '\0';
		editor_update_row(row);
	}
	Editor.cy++;
	Editor.cx = 0;
}

void editor_delete_char() {
	if (Editor.numrows < Editor.cy) { return; }
	if (Editor.cx == 0 && Editor.cy == 0) { return; }

	editor_row *row = &Editor.row[Editor.cy];
	if (Editor.cx > 0) {
		editor_row_delete_char(row, Editor.cx - 1);
		Editor.cx--;
	}
	else {
		Editor.cx = Editor.row[Editor.cy - 1].size;
		editor_row_append_string(&Editor.row[Editor.cy - 1], row->chars, row->size);
		editor_delete_row(Editor.cy);
		Editor.cy--;
	}
}



/*** file I/O ***/

char *editor_rows_to_string(int *buflen) {
	/* 臾몄꽌 ??μ쓣 ?꾪빐 ?먮뵒?곗쓽 臾몄옄?댁쓣 ?섎굹??臾몄옄??諛곗뿴???닿린 */
	int i, total_len = 0;
	char* buf;
	char* p;
	for (i = 0; i < Editor.numrows; i++) {
		total_len += Editor.row[i].size + 1;
		/* 1? 媛??됱쓽 媛쒗뻾 臾몄옄瑜??꾪빐??異붽? ?좊떦 */
	}
	*buflen = total_len;

	buf = malloc(total_len);
	p = buf;
	for (i = 0; i < Editor.numrows; i++) {
		memcpy(p, Editor.row[i].chars, Editor.row[i].size);
		p += Editor.row[i].size;
		*p = '\n';
		p++;
	}

	return buf;
}


void editor_open(char *filename) {
	FILE* fp;
	free(Editor.filename);
	Editor.filename = strdup(filename);
	FILE* debug_file;

	//editor_select_syntax_highlight();

	fp = fopen(filename, "r");
	if (!fp) { die("fopen"); }

	debug_file = fopen("debug.txt", "w");

	char *line = NULL;
	size_t linecap = 0;
	int linelen;
	int count = 0;

	while ((linelen = getline(&line, &linecap, fp)) != -1) {
		while (linelen > 0 && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r')) {
			linelen--;
		}
		fprintf(debug_file, "%d\n", count);
		count++;
		editor_insert_row(Editor.numrows, line, linelen);
	}

	fprintf(debug_file, "number of rows : %d\n", Editor.numrows);
	editor_select_syntax_highlight();
	/* ?뚯씪 ?닿퀬 ?섏꽌 臾몄옄??蹂듭궗 ???좏깮???섏씠?쇱씠??*/
	free(line);
	fclose(fp); 

	Editor.dirty = 0;

	fclose(debug_file);
}


void editor_save() {
	int len;
	char* buf;
	int fd;
	if (Editor.filename == NULL) {
		Editor.filename = editor_prompt("Save as : %s (ESC to cancel)", NULL);
		if (Editor.filename == NULL) {
			editor_set_status_message("Save aborted");
			return;
		}
		editor_select_syntax_highlight();
	} /* ?대젮 ?덈뒗 ?뚯씪 ?녿떎 */


	buf = editor_rows_to_string(&len);
	/* ?먮뵒?곗뿉 ?닿릿 ?댁슜??臾몄옄??諛곗뿴 ?섎굹?????댁븯?? */

	//save_file=fopen(Editor.filename, "w");

	fd = open(Editor.filename, O_RDWR | O_CREAT, 0644);
	/* 異붽? : ??ν븷 ??fnctl媛숈? 蹂???⑥닔 ?덉벐怨??????덇쾶 ?대낯?? fprintf 媛숈? 嫄몃줈 ?섎젮?? step 106 李멸퀬 */

	if (fd != -1) {
		if (ftruncate(fd, len) != -1) {
			if (write(fd, buf, len) == len) {
				close(fd);
				free(buf);
				Editor.dirty = 0;
				editor_set_status_message("%d bytes written to disk", len);
				return;
			}
		}
		close(fd);
	}

	free(buf);
	editor_set_status_message("I/O error : %s", strerror(errno));
}

/*** find KMP optimization***/

void fail_func(char* pat) {
	int i, k;
	int n = strlen(pat);
	for (i = 0; i < 2; i++) {
		fail[i] = realloc(fail[i], sizeof(char)*n);
	}

	/* ?ㅻⅨ履쎌뿉 ?덈뒗 ?⑥뼱 ?먯깋?섎뒗 ?ㅽ뙣?⑥닔 */
	fail[0][0] = -1;
	for (k = 1; k < n; k++) {
		i = fail[0][k - 1];
		while (pat[k] != pat[i + 1] && i >= 0) {
			i = fail[0][i];
		}
		if (pat[k] == pat[i + 1]) { fail[0][k] = i + 1; }
		else { fail[0][k] = -1; }
	}

	fail[1][n - 1] = n;
	for (k = n - 2; k >= 0; k--) {
		i = fail[1][k + 1];
		while (pat[k] != pat[i - 1] && i < n) {
			i = fail[1][i];
		}
		if (pat[k] == pat[i - 1]) { fail[1][k] = i - 1; }
		else { fail[1][k] = n; }
	}
}


char* KMP_match(char* string, char* pat, int* i, int direction) {
	/* string??(*i)踰덉㎏ ?몃뜳?ㅼ뿉???먯깋 ?쒖옉 */
	int k;
	int lens = strlen(string);
	int lenp = strlen(pat);

	if (direction == 1) {
		/* ?ㅻⅨ履?諛⑺뼢 ?먯깋 */
		k = 0;
		while ((*i) < lens && k < lenp) {
			if (string[*i] == pat[k]) {
				(*i)++; k++;
			}
			else if (k == 0) {
				(*i)++;
			}
			else {
				k = fail[0][k - 1] + 1;
			}
		}

		return(k == lenp) ? (string + (*i) - lenp) : NULL;
	}

	else {
		/* ?쇱そ 諛⑺뼢 ?먯깋 */
		k = lenp - 1;
		while ((*i) >= 0 && k >= 0) {
			if (string[*i] == pat[k]) {
				(*i)--; k--;
			}
			else if (k == lenp - 1) {
				(*i)--;
			}
			else {
				k = fail[1][k + 1] - 1;
			}
		}
		return (k == -1) ? (string + (*i) + 1) : NULL;
	}
}


/*** find ***/

void editor_find_callback(char* query, int key) {
	/* ?뱀젙 荑쇰━ 李얜뒗 肄쒕갚?⑥닔 */
	if (query[0] == '\0') { return; } //check
	static int last_match = -1;
	static int direction = 1;

	static int saved_hl_line;
	static char *saved_hl = NULL;

	static int idx = -1;
	static int last_direction = 1;

	int i, j, current;
	int query_len = strlen(query);
	char *match;

	if (saved_hl) {
		memcpy(Editor.row[saved_hl_line].hl, saved_hl, Editor.row[saved_hl_line].rsize);
		free(saved_hl);
		saved_hl = NULL;
	}


	if (key == '\r' || key == KEY_ESC) {
		/* 寃??痍⑥냼?섎뒗 寃쎌슦 */
		last_match = -1;
		direction = 1;
		last_direction = 1;
		return;
	}
	else if (key == KEY_RIGHT || key == KEY_DOWN) {
		direction = 1;
	}
	else if (key == KEY_LEFT || key == KEY_UP) {
		direction = -1;
	}
	else {
		last_match = -1;
		direction = 1;
		last_direction = 1;
	}


	if (last_match == -1) { direction = 1; }
	current = last_match;

	fail_func(query);

	for (i = 0; i < Editor.numrows; i++) {
		/*if(idx<0 || idx>=Editor.row[current].rsize){ //?꾩뿉 ?먯깋??以꾩뿉 ?⑥뼱 ?녿떎
			current+=direction;
			if(current==-1){current=Editor.numrows-1;}
			else if(current==Editor.numrows){current=0;}
			if(direction==1){idx=0;}
			else idx=Editor.row[current].rsize-1;
		}*/
		current += direction;
		if (current == -1) { current = Editor.numrows - 1; }
		else if (current == Editor.numrows) { current = 0; }
		if (direction == 1) { idx = 0; }
		else idx = Editor.row[current].rsize - 1;

		editor_row *row = &Editor.row[current];

		if (last_direction != direction) {
			if (last_direction == 1) { idx -= (strlen(query) + 1); }
			else idx += (strlen(query) + 1);
		}

		match = KMP_match(row->render, query, &idx, direction);
		/* return the matching substring pointer */
		if (match) {
			last_direction = direction;
			last_match = current;
			Editor.cy = current;
			Editor.cx = editor_row_rx_to_cx(row, match - row->render);
			Editor.rowoff = Editor.numrows;
			//Editor.coloff=Editor.cx-Editor.screencols/2; //check
			if (Editor.coloff < 0) { Editor.coloff = 0; }

			saved_hl_line = current;
			saved_hl = malloc(row->rsize);
			memcpy(saved_hl, row->hl, row->rsize);

			for (j = 0; j < query_len; j++) {
				row->hl[match - row->render + j] = HL_MATCH;
			}

			break;
		}
		else {
			idx = -1;
			last_direction = direction;
		}
	}
}


void editor_find() {
	int saved_cx = Editor.cx;
	int saved_cy = Editor.cy;
	int saved_coloff = Editor.coloff;
	int saved_rowoff = Editor.rowoff;

	char *query = editor_prompt("Search: %s (Use ESC/Arrows/Enter)", editor_find_callback);

	if (query) {
		free(query);
	}
	else {
		Editor.cx = saved_cx;
		Editor.cy = saved_cy;
		Editor.rowoff = saved_rowoff;
		Editor.coloff = saved_coloff;
	}

}


/*** print buffer ***/

typedef struct {
	char *b;
	int len;
}buffer;


#define BUFFER_INIT {NULL,0}


void buffer_append(buffer *ab, const char *s, int len) {
	/* buffer ??湲몄씠 len??臾몄옄??s異붽? */
	char *new = realloc(ab->b, (ab->len + len));

	if (new == NULL) { return; }
	memcpy(&new[ab->len], s, len);
	ab->b = new;
	ab->len += len;
}


void buffer_free(buffer *ab) {
	free(ab->b);
}

/*** highlight buffer ***/

typedef struct {
	char *b;
	int len;
}hl_buffer;

#define HL_BUFFER_INIT {NULL,0}

void hl_buffer_append(hl_buffer *hlb, const char* hl, int len) {
	/* ?섏씠?쇱씠??臾몄옄??異붽? */
	int i;
	char *new_hl = realloc(hlb->b, hlb->len + len);

	if (new_hl == NULL) { return; }
	if (hl == NULL) {
		/* 蹂듭궗???섏씠?쇱씠??諛곗뿴濡????ㅼ뼱?ㅻ㈃ len留뚰겮??NORMAL ?꾨┛?몃줈 */
		for (i = 0; i < len; i++) {
			new_hl[hlb->len + i] = HL_NORMAL;
		}
		hlb->b = new_hl;
		hlb->len += len;
	}
	else {
		memcpy(&new_hl[hlb->len], hl, len);
		hlb->b = new_hl;
		hlb->len += len;
	}
}

void hl_buffer_free(hl_buffer *hlb) {
	free(hlb->b);
}


/*** output ***/

void editor_scroll() {
	Editor.rx = Editor.cx;

	if (Editor.cy < Editor.numrows) {
		Editor.rx = editor_row_cx_to_rx(&Editor.row[Editor.cy], Editor.cx);
	}


	/* ?쒖쨪?????꾨옒濡?媛寃??곸젅??議곗젅 */
	if (Editor.cy < Editor.rowoff) {
		Editor.rowoff = Editor.cy;
		/* 而ㅼ꽌媛 ?곕━ ?붾㈃蹂대떎 ?꾩뿉 ?덉쓣 ??*/
	}
	if (Editor.cy >= Editor.rowoff + Editor.screenrows) {
		/* 而ㅼ꽌媛 ?곕━ ?붾㈃蹂대떎 ?꾨옒 ?덉쓣 ??*/
		Editor.rowoff = Editor.cy - Editor.screenrows + 1;
	}
	if (Editor.rx < Editor.coloff) {
		Editor.coloff = Editor.rx;
	}
	if (Editor.rx >= Editor.coloff + Editor.screencols) {
		Editor.coloff = Editor.rx - Editor.screencols + 1;
	}
}


void editor_start_screen(buffer *ab) {
	/* kilo??draw_rows瑜??쒖옉 ?붾㈃?먯꽌留?以?洹몃━寃?諛붽퓞 */
	/* 肄붾뱶 ?먮뵒???쒖옉 ?붾㈃ */
	int y;
	for (y = 0; y < Editor.screenrows + 1; y++) {
		if (Editor.numrows == 0 && y == Editor.screenrows / 3) {
			char welcome[80] = "This is the text editor";
			char start_info[80] = "Press ENTER to start";
			int welcome_len = strlen(welcome);
			int start_info_len = strlen(start_info);
			if (welcome_len > Editor.screencols) { welcome_len = Editor.screencols; }
			if (start_info_len > Editor.screencols) { start_info_len = Editor.screencols; }
			mvwprintw(stdscr, y, (Editor.screencols - welcome_len) / 2, "%s", welcome);
			mvwprintw(stdscr, y + 1, (Editor.screencols - start_info_len) / 2, "%s", start_info);

		}
		mvwprintw(stdscr, y, 0, "~");
		mvwprintw(stdscr, y, Editor.screencols - 1, "~");
	}
	buffer_append(ab, "", 0);
	//hl_buffer_append(hlb, NULL, 0);
}


void editor_print_rows(buffer *ab, hl_buffer *hlb) {
	int y, len, filerow, j;
	char *c;
	char *hl;
	char sym;

	for (y = 0; y < Editor.screenrows; y++) {
		filerow = y + Editor.rowoff;
		/* rowoff遺???쒖옉?댁꽌 screenrow 媛쒖닔留뚰겮 異쒕젰?섍린*/
		if (filerow < Editor.numrows) {
			len = Editor.row[filerow].rsize - Editor.coloff;
			if (len < 0) { len = 0; }
			if (len > Editor.screencols) { len = Editor.screencols - 1; }

			c = &Editor.row[filerow].render[Editor.coloff];
			hl = &Editor.row[filerow].hl[Editor.coloff];

			for (j = 0; j < len; j++) {
				if (!isascii(c[j])) {
					/* ?쒖뼱 臾몄옄???瑜??ｌ뼱以?*/
					sym = '?';
					buffer_append(ab, &sym, 1);
				}
				else {
					buffer_append(ab, &c[j], 1);
				}
			}

			//buffer_append(ab, c, len);
			hl_buffer_append(hlb, hl, len);
		}

		buffer_append(ab, "\n", 1);
		hl_buffer_append(hlb, NULL, 1);
	}
}


void print_buffer_on_screen(buffer *ab, hl_buffer *hlb) {
	/* 踰꾪띁瑜??붾㈃???쒓??먯뵫 異쒕젰?댁쨲. ?좏깮???섏씠?쇱씠?낆쓣 ?꾪븯?? */
	int i;
	char cur;
	char current_color = HL_NORMAL;
	wmove(stdscr, 0, 0);
	/* ?꾨┛?명븯湲??꾩뿉 癒쇱? 0,0?꾩튂濡???꺼以??*/
	//wprintw(stdscr, "%s", ab->b);
	for (i = 0; i < ab->len; i++) {
		cur = ab->b[i];
		if (current_color != hlb->b[i]) {
			/* ?됱씠 諛붾??뚮쭔 異쒕젰 */
			attroff(COLOR_PAIR(current_color));
			current_color = hlb->b[i];
			attron(COLOR_PAIR(current_color));
		}
		wprintw(stdscr, "%c", cur);
	}

}



void editor_draw_status_bar() {
	/* 異붽? : ??諛섏쟾?댁꽌 異쒕젰?섍린. 諛곌꼍 ?곗깋/ 湲??寃?뺤쑝濡?異쒕젰?섍쾶 ?섎㈃ ?좊벏 */
	/* ?섎떒 ?곹깭諛?異쒕젰 */
	int len, rlen;
	char status[80], rstatus[80];
	//char negative=HL_NEGATIVE;
	len = snprintf(status, sizeof(status), "%.20s - %d lines %s", (Editor.filename ? Editor.filename : "[No Name]"),
		Editor.numrows, (Editor.dirty ? "(modifying)" : ""));
	/* ?щ㎎ 臾몄옄??異쒕젰 */
	rlen = snprintf(rstatus, sizeof(rstatus), "%s | %d line / %d line", Editor.syntax ? Editor.syntax->file_type : "no filtype", Editor.cy, Editor.numrows);
	if (len > Editor.screencols) { len = Editor.screencols; }

	attron(A_REVERSE);
	mvwprintw(stdscr, Editor.screenrows, 0, "%s", status);
	/* 利??ㅻⅨ 臾몄옣?ㅼ? Editor.screenrows-1 踰덉㎏ 以꾧퉴吏 異쒕젰?섏뼱???쒕떎 */
	while (len < Editor.screencols) {
		if (Editor.screencols - len == rlen) {
			mvwprintw(stdscr, Editor.screenrows, len, "%s", rstatus);
			break;
		}
		else {
			mvwprintw(stdscr, Editor.screenrows, len, " ");
			len++;
		}
	}

	/*buffer_append(ab, status, len);
	for(i=0;i<len;i++){
		hl_buffer_append(hlb, &negative, 1);
	}


	while(len<Editor.screencols){
		if(Editor.screencols-len==rlen){
			buffer_append(ab, rstatus, rlen);
			//hl_buffer_append(hlb, NULL, rlen);
			for(i=0;i<rlen;i++){
				hl_buffer_append(hlb, &negative, 1);
			}
			break;
		}
		else{
			buffer_append(ab, " ", 1);
			hl_buffer_append(hlb, &negative, 1);
			len++;
		}
	}*/
	//buffer_append(ab, "\n", 1);
	/* 異붽? : ?곹깭諛붽? ??諛섏쟾?쇰줈 異쒕젰?섎룄濡?肄붾뱶瑜?怨좎튌 寃?*/
}


void editor_draw_message_bar() {
	int i;
	int msg_len = strlen(Editor.status_msg);
	if (msg_len > Editor.screencols) { msg_len = Editor.screencols; }

	if (msg_len && time(NULL) - Editor.status_msg_time < 5) {
		attron(A_REVERSE);
		mvwprintw(stdscr, Editor.screenrows + 1, 0, "%s", Editor.status_msg);
		for (i = msg_len; i < Editor.screencols; i++) {
			wprintw(stdscr, " ");
		}
		//attroff(A_REVERSE);
	}

	for (i = 0; i < Editor.screencols; i++) {
		wprintw(stdscr, " ");
	}
	attroff(A_REVERSE);
	/* 異붽? : ?곹깭諛붽? ??諛섏쟾?쇰줈 異쒕젰?섎룄濡?肄붾뱶瑜?怨좎튌 寃?*/
}


void editor_refresh_screen() {
	editor_scroll();

	buffer ab = BUFFER_INIT;
	hl_buffer hlb = HL_BUFFER_INIT;

	wclear(stdscr);

	editor_print_rows(&ab, &hlb);
	editor_draw_status_bar();
	editor_draw_message_bar();

	print_buffer_on_screen(&ab, &hlb);
	wmove(stdscr, Editor.cy - Editor.rowoff, Editor.rx - Editor.coloff);

	buffer_free(&ab);
	hl_buffer_free(&hlb);
	refresh();
}


void editor_set_status_message(const char *fmt, ...) {
	/* 媛蹂 ?몄옄 ?⑥닔. ?곹깭 硫붿떆吏瑜??ㅼ젙?쒕떎. */
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(Editor.status_msg, sizeof(Editor.status_msg), fmt, ap);
	va_end(ap);
	Editor.status_msg_time = time(NULL);
}


/*** input ***/

char *editor_prompt(char *prompt, void(*callback)(char*, int)) {
	/* 寃?됱뼱瑜??낅젰???뚮쭏???뚯씪??寃?됰릺寃??섍린 ?꾪빐 肄쒕갚 ?⑥닔 ?ъ슜 */
	int c;
	size_t bufsize = 128;
	char *buf = malloc(bufsize);

	size_t buflen = 0;
	buf[0] = '\0';

	while (1) {
		editor_set_status_message(prompt, buf);
		editor_refresh_screen();

		c = editor_read_key();
		if (c == KEY_DC || c == CTRL_KEY('h') || c == KEY_BACKSPACE) {
			if (buflen != 0) { buf[--buflen] = '\0'; }
		}
		else if (c == KEY_ESC) {
			editor_set_status_message("");
			if (callback) { callback(buf, c); }
			free(buf);
			return NULL;
		}
		else if (c == KEY_ENTER) {
			if (buflen != 0) {
				editor_set_status_message("");
				if (callback) { callback(buf, c); }
				return buf;
			}
		}
		else if (!iscntrl(c) && c < 128) {
			/* ?낅젰? char ?뺤쑝濡??쒗븳 */
			if (buflen == bufsize - 1) {
				bufsize *= 2;
				buf = realloc(buf, bufsize);
			}
			buf[buflen++] = c;
			buf[buflen] = '\0';
		}
		if (callback) { callback(buf, c); }
	}
}


void editor_move_cursor(int key) {
	/* ?앹쨪?먯꽌???ㅻⅨ履쏀궎 泥섎━ ?ㅼ떆 ?뺤씤??寃?*/
	editor_row *row = (Editor.cy >= Editor.numrows ? NULL : &Editor.row[Editor.cy]);

	switch (key) {
	case KEY_UP:
		if (Editor.cy > 0) {
			Editor.cy--;
			if (Editor.cx > Editor.row[Editor.cy].size) {
				/* ?대룞??以꾩씠 ???ъ씠利덇? ?묒쓣 ?섎룄 ?덈떎. 洹몃븣??泥섎━ */
				Editor.cx = Editor.row[Editor.cy].size;
			}
		}
		break;

	case KEY_DOWN:
		/* 異붽? : ?좎? ?ㅽ겕濡??꾨옒濡??대젮媛덈븣 而ㅼ꽌媛 留??앹쑝濡?媛꾨떎 */
		if (Editor.cy < Editor.numrows - 1) {
			Editor.cy++;
			if (Editor.cx > Editor.row[Editor.cy].size) {
				Editor.cx = Editor.row[Editor.cy].size;
			}
		}
		break;

	case KEY_LEFT:
		if (Editor.cx != 0) {
			Editor.cx--;
		}
		else if (Editor.cy > 0) {
			Editor.cy--;
			Editor.cx = Editor.row[Editor.cy].size;
		}
		break;

	case KEY_RIGHT:
		if (row && Editor.cx < row->size) {
			/* ?쇱젙 ?ъ씠利??댁긽?쇰줈 媛????녾쾶 ?듭젣???먯옄 */
			Editor.cx++;
		}
		else if (row && Editor.cx == row->size && Editor.cy < Editor.numrows - 1) {
			Editor.cy++;
			Editor.cx = 0;
		}
		break;
	}

	row = (Editor.cy >= Editor.numrows) ? NULL : &Editor.row[Editor.cy];
	int rowlen = row ? row->size : 0;
	if (Editor.cx > rowlen) {
		Editor.cx = rowlen;
	}

}


void editor_process_key_press() {
	static int quit_times = QUIT_TIMES;
	int c = editor_read_key();
    char* prefix_word=malloc(sizeof(char)*WORDMAX);

	switch (c) {

	case CTRL_KEY('q'):
		if (Editor.dirty && quit_times > 0) {
			editor_set_status_message("File has unsaved changes. " "Press Ctrl-Q %d more time to quit", quit_times);
			quit_times--;
			return;
		}
		//end_recommend();
		fprintf(fp_save, "%d", wordcnt); // dbg

		fclose(fp_save); //dbg
		exit(0);
		break;

	case CTRL_KEY('s'):
		editor_save();
		break;


		/* WordRecommend */
	case CTRL_KEY('p'): // modf
	{
		WINDOW* win = newwin(SHOWCNT + 3, WORDMAX + 2, Editor.cy + 1, Editor.cx);// +3 -> +2
        keypad(win, TRUE);
        //to get special character
		// chk: border line
		// chk: subwin pos - at cursor OR at word's first letter
		box(win, 0,0);
		int start; // ?꾩옱 row?먯꽌 prefix??泥?湲???몃뜳?ㅻ? ??ν븷 蹂??
		for (start = Editor.cx; start>0 && !is_separator(Editor.row[Editor.cy].chars[start - 1]); start--);
        //for 臾몄쓽 遺?깊샇 諛⑺뼢 ?섏젙??
		// trie?먯꽌 寃?됲븷 prefix: ?꾩옱 row?먯꽌 ?몃뜳?ㅺ? [start, Editor.cx]??substring
        if(Editor.cx-start+2>=WORDMAX){break;}
        else{
            prefix_word=strncpy(prefix_word, &Editor.row[Editor.cy].chars[start], Editor.cx-start);
            //prefix??湲몄씠??Editor.cx-start ?대떎
			prefix_word[Editor.cx-start]='\0';
        }
        //fprintf(fp_save, "%s\n", prefix_word);
        auto_complete_suggestion(Editor.auto_complete, prefix_word);
        //char tmp=Editor.cx-start+'0';
        //insert_list(&tmp, 1);
        //insert_list(prefix_word, Editor.cx-start);
		char* word = word_recommend(win);
		delwin(win);
		//erase_list();
		int word_len;
		if (word!=NULL) { // insert word if word exists
            word_len=strlen(word);
			for (int i = 0; i < word_len; i++) {
				if (word[i] == Editor.row[Editor.cy].chars[start + i]) continue; 
				// word 以??ㅽ겕由곗뿉 ?녿뒗 遺遺꾨쭔 ?낅젰?쒕떎.
				editor_insert_char(word[i]);
			}
		}
		erase_list();
		break;
	}

	/* FindBracket */
	case CTRL_KEY('B'):
	{
		// modf: highlight bracket(s)
		find_bracket();
		if (bracket_pair[0][0] != -1) {
			if (bracket_pair[1][0] == -1) { // not pair
				Editor.row[bracket_pair[0][0]].hl[bracket_pair[0][1]] = HL_NOTPAIR;
			}
			else { // matched
				Editor.row[bracket_pair[0][0]].hl[bracket_pair[0][1]] = HL_PAIR;
				Editor.row[bracket_pair[1][0]].hl[bracket_pair[1][1]] = HL_PAIR;
			}
		}
		break;
	}


	case CTRL_KEY('f'):
		editor_find();
		break;

	case KEY_HOME:
		Editor.cx = 0;
		break;

	case KEY_END:
		if (Editor.cy < Editor.numrows) {
			Editor.cx = Editor.row[Editor.cy].size;
		}
		break;

	case KEY_BACKSPACE:
	case CTRL_KEY('h'):
	case KEY_DC:
		if (c == KEY_DC) {
			editor_move_cursor(KEY_RIGHT);
		}
		editor_delete_char();
		break;

	case KEY_UP:
	case KEY_DOWN:
	case KEY_LEFT:
	case KEY_RIGHT:
		editor_move_cursor(c);
		break;

	case KEY_PPAGE:
	case KEY_NPAGE:
		/* ?섏씠吏 ?⑥쐞濡??대룞?섍린. PAGEUP, PAGEDOWN */
	{
		if (c == KEY_PPAGE) {
			Editor.cy = Editor.rowoff;
		}
		else if (c == KEY_NPAGE) {
			Editor.cy = Editor.rowoff + Editor.screenrows - 1;
			if (Editor.cy > Editor.numrows) { Editor.cy = Editor.numrows; }
		}

		int times = Editor.numrows;
		while (times--) {
			editor_move_cursor(c == KEY_PPAGE ? KEY_UP : KEY_DOWN);
		}
		Editor.cx = 0;
		/* ?됱쓽 留?泥??꾩튂濡?*/
	}
	break;

	case CTRL_KEY('l'):
	case KEY_ESC:
		/* do nothing */
		break;

	case KEY_ENTER:
		editor_insert_newline();
		break;

	default:
		editor_insert_char(c);
		break;

	}

	quit_times = QUIT_TIMES;
}


/*** init ***/

void init_editor() {
	Editor.cx = 0;
	Editor.cy = 0;
	Editor.rx = 0;
	Editor.rowoff = 0;
	Editor.coloff = 0;
	Editor.numrows = 0;
	Editor.row = NULL;
	Editor.dirty = 0;
	Editor.filename = NULL;
	Editor.status_msg[0] = '\0';
	Editor.status_msg_time = 0;
	Editor.syntax = NULL;
    Editor.auto_complete=get_new_trie_node();

	get_window_size(&Editor.screenrows, &Editor.screencols);
	if (Editor.screenrows == -1 && Editor.screencols == -1) {
		die("Window Size");
	}

	Editor.screenrows -= 2;
	/* ?곹깭諛붾? ?섎떒??留뚮뱾湲??꾪븳 ?ъ쑀 怨듦컙 */
}


/*** stack functions ***/

/* stack push */

void insert_char_list(char_node** clist, char c) {
	char_node* new;
	new = (char_node*)malloc(sizeof(char_node));
	new->data = c;
	if (*clist) {
		new->next = (*clist)->next;
		(*clist)->next = new;
	}
	else {
		(*clist) = new;
		(*clist)->next = NULL;
	}
}

void delete_char_list(char_node** clist) {
	char_node* del;
	if (!(*clist)) { return; }
	del = (*clist);
	(*clist) = (*clist)->next;
	free(del);
}


/*** FindBracket: 愿꾪샇 ??李얘린 ***/
/*
bracket_pair[2][2]
?섏씠?쇱씠???섍린 ?꾪빐 愿꾪샇???꾩튂 ?쇰떒 ??? ?꾩옱 ?ъ슜 ?덊븿.
bracket_pair[0]???띿쓣 李얠쓣 愿꾪샇 ?꾩튂 ???
bracket_pair[1] = {-1,-1} ?대㈃ ?꾩옱 愿꾪샇?????놁쓬.
*/
void find_bracket() {
	int r = Editor.cy;
	int c = editor_row_cx_to_rx(&Editor.row[r], Editor.cx); 
	char_node* stack = NULL;
	short int found = 0;
	if (Editor.row[r].render[c] == '{' && Editor.row[r].hl[c] == HL_NORMAL) { // 而ㅼ꽌 ?꾩튂??string/comment媛 ?꾨땶 '{'媛 ?덉쓣 ??
		insert_char_list(&stack, '{');
		bracket_pair[0][0] = r;
		bracket_pair[0][1] = c;
		c++;
		/* ?뚯씪 ?앷퉴吏 ?꾩쟾 ?먯깋*/
		for (; c < Editor.row[r].rsize; c++) {
			if (Editor.row[r].render[c] == '{' && Editor.row[r].hl[c] == HL_NORMAL) {
				insert_char_list(&stack, '{');
			}
			else if (Editor.row[r].render[c] == '}'&& Editor.row[r].hl[c] == HL_NORMAL) {
				delete_char_list(&stack);
				if (!stack) { // empty stack
					bracket_pair[1][0] = r;
					bracket_pair[1][1] = c;
					Editor.cx = editor_row_rx_to_cx(&Editor.row[r], c);
					return;
				}
			}
		}
		r++;
		for (; r < Editor.numrows; r++) {
			for (c = 0; c < Editor.row[r].rsize; c++) {
				if (Editor.row[r].render[c] == '{'&& Editor.row[r].hl[c] == HL_NORMAL) {
					insert_char_list(&stack, '{');
				}
				else if (Editor.row[r].render[c] == '}'&& Editor.row[r].hl[c] == HL_NORMAL) {
					delete_char_list(&stack);
					// if stack is empty: found pair
					// hightlight -> goto endbracket -> break
					if (!stack) {
						bracket_pair[1][0] = r;
						bracket_pair[1][1] = c;
						Editor.cx = editor_row_rx_to_cx(&Editor.row[r], c);
						Editor.cy = r;
						return;
					}
				}
			}
		}

	}
	else if (Editor.row[r].render[c] == '}'&& Editor.row[r].hl[c] == HL_NORMAL) { // 而ㅼ꽌 ?꾩튂??string/comment媛 ?꾨땶 '}'媛 ?덉쓣 ??
		insert_char_list(&stack, '}');
		bracket_pair[0][0] = r;
		bracket_pair[0][1] = c;
		c--;
		/* ?뚯씪 ?앷퉴吏 ?꾩쟾 ?먯깋*/
		for (; c >= 0; c--) {
			if (Editor.row[r].render[c] == '{'&& Editor.row[r].hl[c] == HL_NORMAL) {
				delete_char_list(&stack);
				if (!stack) { // empty stack
					bracket_pair[1][0] = r;
					bracket_pair[1][1] = c;
					Editor.cx = editor_row_rx_to_cx(&Editor.row[r], c);
					return;
				}
			}
			else if (Editor.row[r].render[c] == '}'&& Editor.row[r].hl[c] == HL_NORMAL) {
				insert_char_list(&stack, '}');
			}
		}
		r--;
		for (; r >= 0; r--) {
			for (c = Editor.row[r].rsize - 1; c >= 0; c--) {
				if (Editor.row[r].render[c] == '}'&& Editor.row[r].hl[c] == HL_NORMAL) {
					insert_char_list(&stack, '}');
				}
				else if (Editor.row[r].render[c] == '{'&& Editor.row[r].hl[c] == HL_NORMAL) {
					delete_char_list(&stack);
					// if stack is empty: found pair
					// hightlight -> goto endbracket -> break
					if (!stack) {
						bracket_pair[1][0] = r;
						bracket_pair[1][1] = c;
						Editor.cx = editor_row_rx_to_cx(&Editor.row[r], c);
						Editor.cy = r;
						return;
					}
				}
			}
		}
	}

}



/*** linked list ***/
// list: circular list ( no head node)
void insert_list(char* start, int len) {
	word_node* new;
	new = (word_node*)malloc(sizeof(word_node));
	new->word = (char*)malloc(sizeof(char)*(len + 1));
	strncpy(new->word, start, len);
	new->word[len] = '\0';

	if (list) {
		/* 由ъ뒪?몄뿉 ?쎌엯 */
		new->prev = list->prev;
		list->prev->next = new;
		new->next = list;
		list->prev = new;
		list = new;
	}
	else {
		new->prev = new->next = new;
		list = new;
	}
	list_cnt++;
}

void erase_list() {
	/* erast the whole list. ?꾩뿭蹂??list媛 議댁옱 */
	int i;
	word_node* del;
	for(i=0;i<list_cnt;i++){
		del=list;
		list=list->next;
		free(del->word);
		free(del);
	}
	list=NULL;
	list_cnt=0;
}

/*** save keywords to save.txt ***/
void end_recommend() {
	FILE* fp = fopen("save.txt", "w");

	if (fp) {
		word_node* cur = list;
		while (cur) {
			fprintf(fp, "%s\n", cur->word);
			cur = cur->next;
		}
		fclose(fp);
	}

	else {
		fprintf(stderr, "cannot open file\n");
	}

	erase_list();
}


/* word_recommend */


char* word_recommend(WINDOW* win) { // return selected word from list
	int key, top = 0;
	word_node* top_node = list;
	int x = 1, y = 1, i;

	mvwprintw(win, SHOWCNT+1, 1, "< %d >", list_cnt);//dbg
	word_node* cur = list;
	for (i = 1; i <= SHOWCNT; i++) {
		if (i>list_cnt) break;
		mvwprintw(win, i, 1, cur->word);
		cur = cur->next;
	}
	wrefresh(win);
	wmove(win, y, x);

	while (1) {
		//get key
		key = wgetch(win);
		switch (key) {
		case KEY_ENTER:
			cur = top_node;
			for (i = 1; i < y; i++) cur = cur->next;
			return cur->word; 
			break;

		case KEY_ESC:
			return NULL;
			break;
            
		case KEY_RIGHT:
		case KEY_DOWN: // arrow two chars
		case 's':
		case 'd':
			y++;
			if (y > list_cnt) y = 1; // when list_cnt < SHOWCNT

			if (y > SHOWCNT) {
				top++;
				top_node = top_node->next;
				y = SHOWCNT;
				if (top > list_cnt) { // moved past the last of word in list
					top = 0;
					top_node = list->next;
				}
				cur = top_node;
				for (i = 1; i <= SHOWCNT; i++) {
					wmove(win, i, 1);
					wclrtoeol(win);
					mvwprintw(win, i, 1, cur->word);
					cur = cur->next;
				}
			}
			//wmove(win, SHOWCNT + 1, 1); //dbg
			//wclrtoeol(win); //dbg
			//mvwprintw(win, SHOWCNT + 1, 1, "%d", top + y); //dbg
			wrefresh(win);
			wmove(win, y, x);

			break;

		case KEY_LEFT:
		case KEY_UP:
		case 'w':
		case 'a':
			y--;
			if (y <= 0) {
				if (list_cnt < SHOWCNT) y = list_cnt;
				else {
					y = 1;
					top--;
					top_node = top_node->prev;
					if (top <= 0) {
						top = list_cnt;
						top_node = list->prev;
					}
					cur = top_node;
					for (i = 1; i <= SHOWCNT; i++) {
						wmove(win, i, 1);
						wclrtoeol(win);
						mvwprintw(win, i, 1, cur->word);
						cur = cur->next;
					}
				}
			}
			//wmove(win, SHOWCNT + 1, 1); //dbg
			//wclrtoeol(win); //dbg
			//mvwprintw(win, SHOWCNT + 1, 1, "%d", top + y); //dbg
			wrefresh(win);
			wmove(win, y, x);
			break;
		}
		//box(win,0,0);
	}
}


/*** main ***/

int main(int argc, char* argv[]) {
	int c;
	buffer temp_buffer;

	fp_save = fopen("save.txt", "w"); //dbg

	setlocale(LC_ALL, "");
	//?좊땲肄붾뱶 媛?ν븯寃?留뚮뱾?댁쨲
	initscr();
	enable_raw_mode();
	init_editor();
	if (argc >= 2) {
		editor_open(argv[1]);
	}
	else {
		while (1) {
			temp_buffer.b = NULL;
			temp_buffer.len = 0;

			editor_start_screen(&temp_buffer);
			wprintw(stdscr, "%s", temp_buffer.b);
			buffer_free(&temp_buffer);
			wrefresh(stdscr);
			c = getch();
			if (c == KEY_ENTER) { break; }
		}
	}
	wclear(stdscr);
	start_color();
	init_color(COLOR_BLUE, 500,500, 1000);
	init_color(COLOR_MAGENTA, 1000,400,1000);
	init_color(COLOR_RED, 1000,0,200);
	//rgb 색 조정
	init_pair(HL_NUMBER, COLOR_GREEN, COLOR_BLACK); //number highlighting
	init_pair(HL_MATCH, COLOR_WHITE, COLOR_MAGENTA); //search result highlighting
	init_pair(HL_STRING, COLOR_YELLOW, COLOR_BLACK);
	init_pair(HL_COMMENT, COLOR_BLUE, COLOR_BLACK);
	init_pair(HL_KEYWORD1, COLOR_CYAN, COLOR_BLACK);
	init_pair(HL_KEYWORD2, COLOR_MAGENTA, COLOR_BLACK);
	init_pair(HL_MLCOMMENT, COLOR_RED, COLOR_BLACK);
	init_pair(HL_PAIR, COLOR_WHITE, COLOR_GREEN);
	init_pair(HL_NOTPAIR, COLOR_WHITE, COLOR_RED);

	editor_set_status_message("HELP : Ctrl-S = save | Ctrl-Q = quit | Ctrl-F = find");

	for (int i = 0; i < 2; i++) { 
		bracket_pair[0][i] = -1;
		bracket_pair[1][i] = -1;
	}

	while (1) {
		editor_refresh_screen();
		if (bracket_pair[0][0] != -1) { 
			Editor.row[bracket_pair[0][0]].hl[bracket_pair[0][1]] = HL_NORMAL;
			if (bracket_pair[1][0] != -1) {
				Editor.row[bracket_pair[1][0]].hl[bracket_pair[1][1]] = HL_NORMAL;
			}
			for (int i = 0; i < 2; i++) {
				bracket_pair[0][i] = -1;
				bracket_pair[1][i] = -1;
			}
		}
		editor_process_key_press();
	}


	refresh();
	endwin();
	//end_recommend();
	fprintf(fp_save, "%d", wordcnt); // db

	fclose(fp_save); //dbg
}
