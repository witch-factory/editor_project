/* Sogang University 2021-1 Data Structure(Prof. JiHoon Yang) - term project : Simple Text Editor
 * Used Stack, Trie, KMP String Search
 * Team member
 * SungHyun Kim(https://github.com/witch-factory)
 * JiEun Kwon(https://github.com/lectura7942)
*/

/*
Copyright (c) 2016, Salvatore Sanfilippo <antirez at gmail dot com>

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice,
  this list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

/* includes */
// dbg:  복원 못한 주석/* 수정:
/* 수정: ( 'modf' 로 표시 ) 
	자동완성 - 에디터에서 변수 / 함수명 선언할 때 마지막 단계만 trie에 들어가도록 변경
		1) 뒤에 ';([' 오면 넣는다
		2) 뒤에 ',' 오면 넣고, 바로 뒤에 오는 변수/함수명을 넣을 준비를 한다.
		3) 뒤에 공백,'\t' 오면 넣는다
		4) 뒤에 '\0' 이 오면 줄 바꿈할 때까지 보류하고 넣는다.
	debug.txt 관련 코드 제거
*/

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

enum separator {
	NORMAL_CHAR = 0,
	SEP_SPACE,
	SEP_NULL,
	SEP_OTHER,
	SEP_NAMEEND,
	SEP_COM
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
	int rsize; // tab takes multiple space
	int msize; /* dynamic memory size */
	char *chars;
	char *render; // tab takes multiple space
	char *hl;
	/* highlight */
	int hl_open_comment;
}editor_row;

typedef struct {
	int cx;
	int cy; /* 현재 커서의 위치 */
	int py; // row idx before present key was pressed. Used to read in name_keyword
	int rx; /* render field index. 만약 탭이 있으면 탭이 차지하는 공간 때문에 rx가 cx보다 더 커짐*/
	int rowoff; // 스크린 밖 위에 있는 행의 개수
	int coloff; // 스크린 밖 왼쪽에 있는 열의 개수
	int screenrows;
	int screencols;
	int numrows;
	char *filename;
	char status_msg[105];
	time_t status_msg_time;
	/* 상태 메시지와 그 타임스탬프 */
	editor_syntax *syntax;
	editor_row* row;
	int dirty; /* 수정중 플래그 */
    trie* auto_complete; // 트라이
}editor_config;

editor_config Editor;

/*** bracket check ***/

int bracket_pair[2][2]; // saves coordinates of bracket pairs

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

int WORDMAX = 30; // max length of word -> subwindow width is WORDMAX+2
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

char* word_input;


/*** prototypes ***/

void editor_set_status_message(const char *fmt, ...);
void editor_refresh_screen();
char *editor_prompt(char *prompt, void(*callback)(char*, int));

char* fail[2] = { NULL, }; /* 실패함수. 정방향/역방향 둘 다 있음 */
void fail_func(char*);

void find_bracket();

void insert_list(char* start, int len);
void erase_list();
char* word_recommend(WINDOW*);

/*** terminal ***/

void die(const char *s) {
	wclear(stdscr);
	perror(s);
	exit(1);
}


void disable_raw_mode() {
	noraw();
	wclear(stdscr);
	endwin();
}


void enable_raw_mode() {
	raw();
	atexit(disable_raw_mode);
	//raw 다음에 atexit가 와야 제대로 raw 탈출된다
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
		//못 찾음

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
			//자식이 있다
			(*cur)->is_leaf = 0;
			return 0;
		}
	}
	return 0;
}


void free_trie(trie** node) {
	/* used to free the dynamically allocated trie nodes. recursive */
	int i;

	if ((*node)->is_leaf == 0) {
		for (i = 0; i < CHAR_SIZE; i++) {
			if ((*node)->ch[i] != NULL) {
				free_trie(&((*node)->ch[i]));
			}
		}
	}

	if ((*node) != NULL) {
		free((*node));
	}
}


/*** autocomplete functions ***/

void suggestion_by_prefix(trie* root, char* prefix) {
	//find the words starts with prefix string, with length len
	int i;

	if (root->is_leaf) {
        insert_list(prefix, strlen(prefix));
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

		//이 prefix로 시작하는 게 없다
		if (!crawl->ch[index]) {
			return 0;
		}

		crawl = crawl->ch[index];

	}

	is_word = crawl->is_leaf == 1 ? 1 : 0;
	is_last = (has_children(crawl) == 0) ? 1 : 0;

	/* 이 노드가 끝이고, 그 뒤에 이어지는 노드가 없을 때 인쇄 */
	if (is_word && is_last) {
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

int is_separator(int c) { // modf
	if (c == '\0') { return SEP_NULL; }
	if (c == ',') { return SEP_COM; }
	if (strchr("?:'\".+-/%<>*!|&)]{}~@#$^`\\", c) != NULL) { return SEP_OTHER; }
	if (strchr("(=[;", c) != NULL) { return SEP_NAMEEND; }
	if (isspace(c) || c == '\t') { return SEP_SPACE; /*this is SEP_NAMEEND too */ }
	return NORMAL_CHAR;
}

/* 행의 하이라이팅 버퍼 업데이트 */
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
	/*문자열 안에 있는지를 나타냄 */
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
			/* 여러 줄 주석에 해당되는 부분을 감지한다 */
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
					/* 이스케이프 문자 무시 */
					row->hl[i + 1] = HL_STRING;
					i += 2;
					continue;
				}

				if (c == in_string) { in_string = 0; }
				/* 닫는 큰/작은 따옴표 감지되면 문자열이 끝난 것이다 */
				i++;
				prev_sep = 1;
				continue;
			}
			else {
				if (c == '"' || c == '\'') {
					/* 여는 따옴표 감지되면 그걸 저장해 둔다 */
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
				/* 현재 문자를 강조표시할 것인가 */
				/* 소수점도 포함해서 강조 표시를 해준다 */
				row->hl[i] = HL_NUMBER;
				i++;
				prev_sep = 0;
				continue;
			}
		}


		if (prev_sep) { /* keywords highlight */
			if (name_keyword) {
				switch (is_separator(c)) {
				case SEP_SPACE:
					break;
				case NORMAL_CHAR:
				{
					// separator가 아닌 문자
					int a;
					// 함수명/변수명 길이 찾기
					for (a = 0; !is_separator(row->render[i + a]); a++); // modf: last char of line is always SEP_NULL
					if (a > WORDMAX) {
						WORDMAX = a;
						word_input = (char*)realloc(word_input, sizeof(char)*(WORDMAX + 1));
					}
					strncpy(word_input, &row->render[i], a);
					word_input[a] = '\0';
					name_keyword = 0;
					/* modf: insert it to trie ONLY IF there is SEP_NAMEEND or SEP_SPACE at the end
						if there isn't, wait until row change to get the name word at final stage.
						Word at final stage is inserted to trie in editor_process_key_press()
					*/ 
					switch (is_separator(row->render[i + a])) {
					case SEP_NAMEEND:
					case SEP_SPACE:
						trie_insert_string(Editor.auto_complete, word_input);
						word_input[0] = '\0';
						break;
					case SEP_COM: // for multiple name_keywords in same line separated by ','
						trie_insert_string(Editor.auto_complete, word_input);
						word_input[0] = '\0';
						name_keyword = 1;
						break;
					case SEP_NULL: break;
					default: 
						word_input[0] = '\0'; 
						break;
					}
					prev_sep = 0;
					i += a;
					continue;
				}
				default: // 공백 아닌 separator.
					//  keyword2가 변수/함수 선언이 아닌 다른 용도로 사용될 때 (eg) sizeof(char)
					name_keyword = 0;
					break;
				}
			}
			else {
				for (j = 0; keywords[j]; j++) {
					kwlen = strlen(keywords[j]);
					kw2 = keywords[j][kwlen - 1] == '|';
					if (kw2) { kwlen--; }

					if (!strncmp(&row->render[i], keywords[j], kwlen) && is_separator(row->render[i + kwlen])) {
						/* 만약 키워드가 있고 키워드 뒤에 separator가 있으면 */
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
	/* 여러 줄 주석이 끝났는지 아닌지 */
	row->hl_open_comment = in_comment;
	if (changed&&row->idx + 1 < Editor.numrows) {
		editor_update_syntax(&Editor.row[row->idx + 1]);
	}

}


/* 파일 이름을 매칭해서 신택스 하이라이팅 부분을 불러온다 */
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
	/* 탭 처리하기 */
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
	/* idx는 row->render에 복사된 문자의 수. 마지막에 널 삽입 */
	row->rsize = idx;

	editor_update_syntax(row);
}


void editor_insert_row(int at, char *s, int len) {
	/* at번째 줄에 에 길이 len 인 문자열 s를 붙여 준다 */
	int j;
	int chars_len;
	if (at<0 || at>Editor.numrows) { return; }

	Editor.row = realloc(Editor.row, sizeof(editor_row)*(Editor.numrows + 1));
	memmove(&Editor.row[at + 1], &Editor.row[at], sizeof(editor_row)*(Editor.numrows - at));

	for (j = at + 1; j <= Editor.numrows; j++) {
		Editor.row[j].idx++;
	}
	/* 그 뒤 줄들의 줄번호를 1씩 증가시켜줌 */

	Editor.row[at].idx = at;
	/* 줄 삽입시 초기화 */

	Editor.row[at].size = len;

	chars_len = Editor.screencols;
	while (chars_len < len) {
		chars_len *= 2;
	}
	Editor.row[at].chars = malloc(sizeof(char)*(chars_len + 1));
	Editor.row[at].msize = chars_len + 1;
	/* 처음에 스크린 크기만큼 할당. */
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
	/* 줄을 앞으로 하나 옮겨오기 */

	for (j = at; j < Editor.numrows - 1; j++) { Editor.row[j].idx--; }
	/* 줄번호 하나씩 당겨주기 */
	Editor.numrows--;
	Editor.dirty++;
}


void editor_row_insert_char(editor_row *row, int at, int c) {
	if (at<0 || at>row->size) { at = row->size; }
	if ((row->size + 2) >= row->msize) {
		/* 만약에 메모리가 부족할 경우 2배로 늘려서 재할당해준다 */
		row->chars = realloc(row->chars, (row->msize) * 2);
		row->msize *= 2;
	}
	memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
	/* 중간에 삽입할 수도 있으므로, at 뒤의 데이터를 한칸 밀어준다 */
	row->size++;
	row->chars[at] = c;
	editor_update_row(row);
	Editor.dirty++;
}


void editor_row_append_string(editor_row* row, char* s, size_t len) {
	/* 줄 끝에 길이 len인 문자열 s를 붙인다 */
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
	/* 커서 위치에 문자 하나 추가 */
	if (Editor.cy == Editor.numrows) {
		/* 마지막 줄에 줄 하나 추가하는 경우 */
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
	/* 문서 저장을 위해 에디터의 문자열을 하나의 문자열 배열에 담기 */
	int i, total_len = 0;
	char* buf;
	char* p;
	for (i = 0; i < Editor.numrows; i++) {
		total_len += Editor.row[i].size + 1;
		/* 1은 각 행의 개행 문자를 위해서 추가 할당 */
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

	fp = fopen(filename, "r");
	if (!fp) { die("fopen"); }

	char *line = NULL;
	size_t linecap = 0;
	int linelen;
	int count = 0;

	while ((linelen = getline(&line, &linecap, fp)) != -1) {
		while (linelen > 0 && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r')) {
			linelen--;
		}
		count++;
		editor_insert_row(Editor.numrows, line, linelen);
	}

	editor_select_syntax_highlight();
	/* 파일 열고 나서 문자열 복사 후 신택스 하이라이팅 */
	free(line);
	fclose(fp); 

	Editor.dirty = 0;

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
	} /* 열려 있는 파일 없다 */


	buf = editor_rows_to_string(&len);
	/* 에디터에 담긴 내용을 문자열 배열 하나에 다 담았다. */

	fd = open(Editor.filename, O_RDWR | O_CREAT, 0644);

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

	/* 왼쪽-> 오른쪽 방향의 실패함수 */
	fail[0][0] = -1;
	for (k = 1; k < n; k++) {
		i = fail[0][k - 1];
		while (pat[k] != pat[i + 1] && i >= 0) {
			i = fail[0][i];
		}
		if (pat[k] == pat[i + 1]) { fail[0][k] = i + 1; }
		else { fail[0][k] = -1; }
	}

	/* 오른쪽 -> 왼쪽 방향의 실패함수 */
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
	/* string의 (*i)번째 인덱스에서 탐색 시작 */
	int k;
	int lens = strlen(string);
	int lenp = strlen(pat);

	if (direction == 1) {
		/* 오른쪽 방향 탐색 */
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
		/* 왼쪽 방향 탐색 */
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
	/* 특정 쿼리 찾는 콜백함수 */
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
		/* 검색 취소하는 경우 */
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
		Editor.py = saved_cy;
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
	/* buffer 에 길이 len인 문자열 s추가 */
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
	/* 하이라이팅 문자열 추가 */
	int i;
	char *new_hl = realloc(hlb->b, hlb->len + len);

	if (new_hl == NULL) { return; }
	if (hl == NULL) {
		/* 복사할 하이라이팅 배열로 널 들어오면 len만큼을 NORMAL 프린트로 */
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

	/* 한줄씩 위/아래로 가게 적절히 조절 */
	if (Editor.cy < Editor.rowoff) {
		Editor.rowoff = Editor.cy;
		/* 커서가 우리 화면보다 위에 있을 때 */
	}
	if (Editor.cy >= Editor.rowoff + Editor.screenrows) {
		/* 커서가 우리 화면보다 아래 있을 때 */
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
	/* kilo의 draw_rows를 시작 화면에서만 줄 그리게 바꿈 */
	/* 코드 에디터 시작 화면 */
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
}


void editor_print_rows(buffer *ab, hl_buffer *hlb) {
	int y, len, filerow, j;
	char *c;
	char *hl;
	char sym;

	for (y = 0; y < Editor.screenrows; y++) {
		filerow = y + Editor.rowoff;
		/* rowoff부터 시작해서 screenrow 개수만큼 출력하기*/
		if (filerow < Editor.numrows) {
			len = Editor.row[filerow].rsize - Editor.coloff;
			if (len < 0) { len = 0; }
			if (len > Editor.screencols) { len = Editor.screencols - 1; }

			c = &Editor.row[filerow].render[Editor.coloff];
			hl = &Editor.row[filerow].hl[Editor.coloff];

			for (j = 0; j < len; j++) {
				if (!isascii(c[j])) {
					/* 제어 문자는 ?를 넣어줌 */
					sym = '?';
					buffer_append(ab, &sym, 1);
				}
				else {
					buffer_append(ab, &c[j], 1);
				}
			}

			hl_buffer_append(hlb, hl, len);
		}

		buffer_append(ab, "\n", 1);
		hl_buffer_append(hlb, NULL, 1);
	}
}


void print_buffer_on_screen(buffer *ab, hl_buffer *hlb) {
	/* 버퍼를 화면에 한글자씩 출력해줌. 신택스 하이라이팅을 위하여. */
	int i;
	char cur;
	char current_color = HL_NORMAL;
	wmove(stdscr, 0, 0);
	/* 프린트하기 전에 먼저 0,0위치로 옮겨준다 */
	for (i = 0; i < ab->len; i++) {
		cur = ab->b[i];
		if (current_color != hlb->b[i]) {
			/* 색이 바뀔 때만 출력 */
			attroff(COLOR_PAIR(current_color));
			current_color = hlb->b[i];
			attron(COLOR_PAIR(current_color));
		}
		wprintw(stdscr, "%c", cur);
	}

}



void editor_draw_status_bar() {
	/* 하단 상태바 출력 */
	int len, rlen;
	char status[80], rstatus[80];
	len = snprintf(status, sizeof(status), "%.20s - %d lines %s", (Editor.filename ? Editor.filename : "[No Name]"),
		Editor.numrows, (Editor.dirty ? "(modifying)" : ""));
	/* 포맷 문자열 출력 */
	rlen = snprintf(rstatus, sizeof(rstatus), "%s | %d line / %d line", Editor.syntax ? Editor.syntax->file_type : "no filetype", Editor.cy, Editor.numrows);
	if (len > Editor.screencols) { len = Editor.screencols; }

	attron(A_REVERSE);
	mvwprintw(stdscr, Editor.screenrows, 0, "%s", status);
	/* 즉 다른 문장들은 Editor.screenrows-1 번째 줄까지 출력되어야 한다 */
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
	}

	for (i = 0; i < Editor.screencols; i++) {
		wprintw(stdscr, " ");
	}
	attroff(A_REVERSE);
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
	/* 가변 인자 함수. 상태 메시지를 설정한다. */
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(Editor.status_msg, sizeof(Editor.status_msg), fmt, ap);
	va_end(ap);
	Editor.status_msg_time = time(NULL);
}


/*** input ***/

char *editor_prompt(char *prompt, void(*callback)(char*, int)) {
	/* 검색어를 입력할 때마다 파일이 검색되게 하기 위해 콜백 함수 사용 */
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
			/* 입력은 char 형으로 제한 */
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
	/* 끝줄에서의 오른쪽키 처리 다시 확인할 것 */
	editor_row *row = (Editor.cy >= Editor.numrows ? NULL : &Editor.row[Editor.cy]);

	switch (key) {
	case KEY_UP:
		if (Editor.cy > 0) {
			Editor.cy--;
			if (Editor.cx > Editor.row[Editor.cy].size) {
				/* 이동한 줄이 더 사이즈가 작을 수도 있다. 그때의 처리 */
				Editor.cx = Editor.row[Editor.cy].size;
			}
		}
		break;

	case KEY_DOWN:
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
			/* 일정 사이즈 이상으로 갈 수 없게 통제 */
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
		free(word_input);
		free_trie(&Editor.auto_complete);
		exit(0);
		break;

	case CTRL_KEY('s'):
		editor_save();
		break;


		/* WordRecommend */
	case CTRL_KEY('p'): 
	{
		if (Editor.cx == 0) break; 
		Editor.rx=editor_row_cx_to_rx(&Editor.row[Editor.cy], Editor.cx); 
		WINDOW* win;
		if (Editor.cy + SHOWCNT + 2 > Editor.screenrows + Editor.rowoff) {
			if (Editor.rx + WORDMAX + 2 > Editor.screencols + Editor.coloff) win = newwin(SHOWCNT + 2, WORDMAX + 2, Editor.cy - (SHOWCNT + 2) -  Editor.rowoff, Editor.rx - (WORDMAX + 2)-Editor.coloff);
			else win = newwin(SHOWCNT + 2, WORDMAX + 2, Editor.cy - (SHOWCNT + 2) - Editor.rowoff, Editor.rx-Editor.coloff);
		}
		else {
			if (Editor.rx + WORDMAX + 2 > Editor.screencols + Editor.coloff) win = newwin(SHOWCNT + 2, WORDMAX + 2, Editor.cy-Editor.rowoff + 1, Editor.rx - (WORDMAX + 2)-Editor.coloff);
			else win = newwin(SHOWCNT + 2, WORDMAX + 2, Editor.cy-Editor.rowoff + 1, Editor.rx-Editor.coloff);
		}
        keypad(win, TRUE);
        //to get special character
		wborder(win, '|', '|', '-', '-', '+', '+', '+', '+'); 
		int start; // 현재 row에서 prefix의 첫 글자 인덱스를 저장할 변수
		for (start = Editor.cx; start>0 && !is_separator(Editor.row[Editor.cy].chars[start - 1]); start--);
		// trie에서 검색할 prefix: 현재 row에서 인덱스가 [start, Editor.cx]인 substring
        if(Editor.cx-start+2>=WORDMAX){break;}
        else{
            prefix_word=strncpy(prefix_word, &Editor.row[Editor.cy].chars[start], Editor.cx-start);
            //prefix길이는 Editor.cx-start
			prefix_word[Editor.cx-start]='\0';
        }
        auto_complete_suggestion(Editor.auto_complete, prefix_word);
		char* word = word_recommend(win);
		delwin(win);
		int word_len;
		if (word!=NULL) { // insert word if word exists
            word_len=strlen(word);
			for (int i = 0; i < word_len; i++) {
				if (word[i] == Editor.row[Editor.cy].chars[start + i]) continue; 
				// word 중 스크린에 없는 부분만 입력한다.
				editor_insert_char(word[i]);
			}
		}
		erase_list();
		break;
	}

	/* FindBracket */
	case CTRL_KEY('B'):
	{
		find_bracket();
		if (bracket_pair[0][0] != -1) {
			if (bracket_pair[1][0] == -1) { // pair not found
				Editor.row[bracket_pair[0][0]].hl[bracket_pair[0][1]] = HL_NOTPAIR;
			}
			else { // pair found
				Editor.row[bracket_pair[0][0]].hl[bracket_pair[0][1]] = HL_PAIR;
				Editor.row[bracket_pair[1][0]].hl[bracket_pair[1][1]] = HL_PAIR;
			}
		}
		break;
	}


	case CTRL_KEY('f'):
		Editor.py = Editor.cy;
		editor_find();
		break;

	case KEY_HOME:
		Editor.py = Editor.cy;
		Editor.cx = 0;
		break;

	case KEY_END:
		Editor.py = Editor.cy;
		if (Editor.cy < Editor.numrows) {
			Editor.cx = Editor.row[Editor.cy].size;
		}
		break;

	case KEY_BACKSPACE:
	case CTRL_KEY('h'):
	case KEY_DC:
		Editor.py = Editor.cy;
		if (c == KEY_DC) {
			editor_move_cursor(KEY_RIGHT);
		}
		editor_delete_char();
		break;

	case KEY_UP:
	case KEY_DOWN:
	case KEY_LEFT:
	case KEY_RIGHT:
		Editor.py = Editor.cy;
		editor_move_cursor(c);
		break;

	case KEY_PPAGE:
	case KEY_NPAGE:
		/* 페이지 단위로 이동하기. PAGEUP, PAGEDOWN */
	{
		Editor.py = Editor.cy;
		if (c == KEY_PPAGE) {
			Editor.cy = Editor.rowoff;
		}
		else if (c == KEY_NPAGE) {
			Editor.cy = Editor.rowoff + Editor.screenrows - 1;
			if (Editor.cy > Editor.numrows) { 
				Editor.cy = Editor.numrows; 
			}
		}

		int times = Editor.numrows;
		while (times--) {
			editor_move_cursor(c == KEY_PPAGE ? KEY_UP : KEY_DOWN);
		}
		Editor.cx = 0;
		/* 행의 맨 첫 위치로 */
	}
	break;

	case CTRL_KEY('l'):
	case KEY_ESC:
		break;

	case KEY_ENTER:
		Editor.py = Editor.cy;
		editor_insert_newline();
		break;

	default:
		Editor.py = Editor.cy;
		editor_insert_char(c);
		break;

	}

	/* modf: name_keyword 뒤 문자가 SEP_NULL일 때 단어 전체를 입력받을 때까지 보류한 것을 지금 트라이에 넣음 */
	if (word_input[0] != '\0' && Editor.cy != Editor.py) {
		trie_insert_string(Editor.auto_complete, word_input);
		word_input[0] = '\0';
	}

	quit_times = QUIT_TIMES;
}


/*** init ***/

void init_editor() {
	Editor.cx = 0;
	Editor.cy = 0;
	Editor.py = 0;
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
	/* 상태바를 하단에 만들기 위한 여유 공간 */
	word_input = (char*)malloc(sizeof(char)*(WORDMAX + 1));
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


/*** FindBracket: 괄호쌍 검사 기능 ***/
/*
bracket_pair[2][2]
하이라이트 하기 위해 괄호쌍 위치 일단 저장. 현재 사용 안함.
bracket_pair[0]에 쌍을 찾을 괄호 위치 저장
bracket_pair[1] = {-1,-1} 이면 현재 괄호에 쌍 없음.
*/
void find_bracket() {
	int r = Editor.cy;
	int c = editor_row_cx_to_rx(&Editor.row[r], Editor.cx); 
	char_node* stack = NULL;
	short int found = 0;
	if (Editor.row[r].render[c] == '{' && Editor.row[r].hl[c] == HL_NORMAL) { // 커서 위치에 string/comment가 아닌 '{'가 있을 때
		insert_char_list(&stack, '{');
		bracket_pair[0][0] = r;
		bracket_pair[0][1] = c;
		c++;
		/* 파일 끝까지 완전 탐색*/
		for (; c < Editor.row[r].rsize; c++) {
			if (Editor.row[r].render[c] == '{' && Editor.row[r].hl[c] == HL_NORMAL) {
				insert_char_list(&stack, '{');
			}
			else if (Editor.row[r].render[c] == '}'&& Editor.row[r].hl[c] == HL_NORMAL) {
				delete_char_list(&stack);
				if (!stack) { // empty stack -> bracket pair found
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
	else if (Editor.row[r].render[c] == '}'&& Editor.row[r].hl[c] == HL_NORMAL) { // 커서 위치에 string/comment가 아닌 '}'가 있을 때
		insert_char_list(&stack, '}');
		bracket_pair[0][0] = r;
		bracket_pair[0][1] = c;
		c--;
		/* 파일 끝까지 완전 탐색*/
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
		/* 리스트에 삽입 */
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
	/* erast the whole list. 전역변수 list가 존재 */
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
	int key, top = 1; // top_node has the (top)th word of list
	word_node* top_node = list;
	int x = 1, y = 1, i;

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
			if (list_cnt == 0) return NULL; 
			cur = top_node;
			for (i = 1; i < y; i++) cur = cur->next;
			return cur->word; 
			break;

		case KEY_ESC:
			return NULL;
			break;
            
		case KEY_RIGHT:
		case KEY_DOWN: 
		case 's':
		case 'd':
			if (list_cnt == 0) break;
			y++;
			if (y > list_cnt) y = 1; 

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
			wrefresh(win);
			wmove(win, y, x);

			break;

		case KEY_LEFT:
		case KEY_UP:
		case 'w':
		case 'a':
			if (list_cnt == 0) break;
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
			wrefresh(win);
			wmove(win, y, x);
			break;
		}
	}
}


/*** main ***/

int main(int argc, char* argv[]) {
	int c;
	buffer temp_buffer;

	setlocale(LC_ALL, "");
	//유니코드 가능하게 만들어줌
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

	editor_set_status_message("HELP : Ctrl-S = save | Ctrl-Q = quit | Ctrl-F = find | Ctrl-B = bracket check | Ctrl-P = autocomplete");

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
}
