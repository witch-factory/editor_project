/* includes */

#include "editor_func.h"
/* 어느 정도 만들어진 것 같은 함수들은 헤더로 이동 */


/*** stack functions ***/

/* stack push */

void insert_char_list(char_node** clist, char c){
	char_node* new;
	new=(char_node*)malloc(sizeof(char_node));
	new->data=c;
	if(*clist){
		new->next=(*clist)->next;
		(*clist)->next=new;
	}
	else{
		(*clist) = new;
		(*clist)->next = NULL;
	}
}

void delete_char_list(char_node** clist){
	char_node* del;
	if(!(*clist)){return;}
	del=(*clist);
	(*clist)=(*clist)->next;
	free(del);
}


/*** FindBracket: 괄호 쌍 찾기 ***/
/*
bracket_pair[2][2]
하이라이트 하기 위해 괄호쌍 위치 일단 저장. 현재 사용 안함.
bracket_pair[0]에 쌍을 찾을 괄호 위치 저장
bracket_pair[1] = {-1,-1} 이면 현재 괄호에 쌍 없음.
*/
void find_bracket() {
	int r = Editor.cy;
	int c = Editor.cx;
	char_node* stack = NULL;
	short int found = 0;
	if (Editor.row[r].chars[c] == '{' && Editor.row[r].hl[c] == HL_NORMAL) { // 커서 위치에 string/comment가 아닌 '{'가 있을 때
		insert_char_list(&stack, '{');
		bracket_pair[0][0] = Editor.cy;
		bracket_pair[0][1] = Editor.cx;
		c++;
		/* 파일 끝까지 완전 탐색*/
		for (; c<Editor.row[r].size; c++) {
			if (Editor.row[r].chars[c] == '{' && Editor.row[r].hl[c] == HL_NORMAL) {
				insert_char_list(&stack, '{');
			}
			else if (Editor.row[r].chars[c] == '}'&& Editor.row[r].hl[c] == HL_NORMAL) {
				delete_char_list(&stack);
				if (!stack) { // empty stack
					bracket_pair[1][0] = r;
					bracket_pair[1][1] = c;
					Editor.cx = c;
					return;
				}
			}
		}
		r++;
		for (; r< Editor.numrows; r++) {
			for (c = 0; c<Editor.row[r].rsize; c++) {
				if (Editor.row[r].chars[c] == '{'&& Editor.row[r].hl[c] == HL_NORMAL) {
					insert_char_list(&stack, '{');
				}
				else if (Editor.row[r].chars[c] == '}'&& Editor.row[r].hl[c] == HL_NORMAL) {
					delete_char_list(&stack);
					// if stack is empty: found pair
					// hightlight -> goto endbracket -> break
					if (!stack) {
						bracket_pair[1][0] = r;
						bracket_pair[1][1] = c;
						Editor.cx = c;
						Editor.cy = r;
						return;
					}
				}
			}
		}

	}
	else if (Editor.row[r].chars[c] == '}'&& Editor.row[r].hl[c] == HL_NORMAL) { // 커서 위치에 string/comment가 아닌 '}'가 있을 때
		insert_char_list(&stack, '}');
		bracket_pair[0][0] = Editor.cy;
		bracket_pair[0][1] = Editor.cx;
		c--;
		/* 파일 끝까지 완전 탐색*/
		for (; c >= 0; c--) {
			if (Editor.row[r].chars[c] == '{'&& Editor.row[r].hl[c] == HL_NORMAL) {
				delete_char_list(&stack);
				if (!stack) { // empty stack
					bracket_pair[1][0] = r;
					bracket_pair[1][1] = c;
					Editor.cx = c;
					return;
				}
			}
			else if (Editor.row[r].chars[c] == '}'&& Editor.row[r].hl[c] == HL_NORMAL) {
				insert_char_list(&stack, '}');
			}
		}
		r--;
		for (; r >= 0; r--) {
			for (c = Editor.row[r].rsize - 1; c >= 0; c--) {
				if (Editor.row[r].chars[c] == '}'&& Editor.row[r].hl[c] == HL_NORMAL) {
					insert_char_list(&stack, '}');
				}
				else if (Editor.row[r].chars[c] == '{'&& Editor.row[r].hl[c] == HL_NORMAL) {
					delete_char_list(&stack);
					// if stack is empty: found pair
					// hightlight -> goto endbracket -> break
					if (!stack) {
						bracket_pair[1][0] = r;
						bracket_pair[1][1] = c;
						Editor.cx = c;
						Editor.cy = r;
						return;
					}
				}
			}
		}
	}

}



/*** linked list ***/

void insert_list(char* start, int len){
	word_node* new;
	new=(word_node*)malloc(sizeof(word_node));
	new->word=(char*)malloc(sizeof(char)*(len+1));
	strncpy(new->word, start, len);
	new->word[len]='\0';

	if(list){
		/* 리스트에 삽입 */
		new->prev=NULL;
		new->next=list;
		list->prev=new;
	}
	else{
		new->prev=NULL;
		new->next=NULL;
		list=new;
	}
	wordcnt++;

}

void erase_list(){
	/* erast the whole list. 전역변수 list가 존재 */
	word_node* del;
	while(list){
		del=list;
		list=list->next;
		free(del);
	}
}

/*** save keywords to save.txt ***/
void end_recommend(){
	FILE* fp=fopen("save.txt", "w");

	if(fp){
		word_node* cur=list;
		while(cur){
			fprintf(fp, "%s\n", cur->word);
			cur=cur->next;
		}
		fclose(fp);
	}

	else{
		fprintf(stderr, "cannot open file\n");
	}

	erase_list();
}


/* word_recommend */


char* word_recommend(WINDOW* win) { // return selected word from list
	int key, top = 0;
	word_node* top_node = list;
	int x = 1, y = 1, i;
	char* word = (char*)malloc(sizeof(char)*WORDMAX);

	word_node* cur = list;
	for (i = 1; i <= SHOWCNT; i++) {
		if (!cur) break;
		mvwprintw(win, i, 1, cur->word);
		cur = cur->next;
	}
	//mvwprintw(win, i, 1, "%d", top + y - 1); //dbg
	wrefresh(win);
	wmove(win, y, x);

	while (1) {
		//get key
		key = wgetch(win);
		switch (key) {
		case KEY_ENTER:
			cur = top_node;
			for (i = 1; i < y; i++) cur = cur->next;
			return cur->word; // chk : out of bounds
			break;

		case KEY_ESC:
			return NULL;
			break;

		case KEY_RIGHT:
		case KEY_DOWN: // arrow two chars
		case 's':
		case 'd':
			y++;
			if (y > SHOWCNT) {
				top++; // chk
				top_node = top_node->next;
				y = SHOWCNT;
				if (top + y >= wordcnt) {
					top = 0;
					top_node = list;
					y = 1;
				}
				for (i = 1; i <= SHOWCNT; i++) {
					wmove(win, i, 1);
					wclrtoeol(win);
					cur = top_node;
					if (cur) {
						mvwprintw(win, i, 1, cur->word);
						cur = cur->next;
					}
				}
			}
			wmove(win, SHOWCNT + 1, 1); //dbg
			wclrtoeol(win); //dbg
			mvwprintw(win, SHOWCNT + 1, 1, "%d", top + y - 1); //dbg
			wrefresh(win);
			wmove(win, y, x);

			break;

		case KEY_LEFT:
		case KEY_UP:
		case 'w':
		case 'a':
			y--;
			if (y == 0) {
				y = 1;
				top--;
				top_node = top_node->prev;
				if (!top_node) {
					top = wordcnt - SHOWCNT;
					top_node = list->prev;
					for (i = 1; i <= 4; i++){
						top_node = top_node->prev;
					}
					y = SHOWCNT;
				}
				cur = top_node;
				for (i = 1; i <= SHOWCNT; i++) {
					wmove(win, i, 1);
					wclrtoeol(win);
					if (cur) {
						mvwprintw(win, i, 1, cur->word);
						cur = cur->next;
					}
				}
			}
			wmove(win, SHOWCNT + 1, 1); //dbg
			wclrtoeol(win); //dbg
			mvwprintw(win, SHOWCNT + 1, 1, "%d", top + y - 1); //dbg
			wrefresh(win);
			wmove(win, y, x);
			break;
		}
	}
}


/*** main ***/

int main(int argc, char* argv[]){
	int c;
	buffer temp_buffer;

	setlocale(LC_ALL, "");
	//유니코드 가능하게 만들어줌
	initscr();
	enable_raw_mode();
	init_editor();
	if(argc>=2){
		editor_open(argv[1]);
	}
	else{
		while(1){
			temp_buffer.b=NULL;
			temp_buffer.len=0;

			editor_start_screen(&temp_buffer);
			wprintw(stdscr, "%s", temp_buffer.b);
			buffer_free(&temp_buffer);
			wrefresh(stdscr);
			c=getch();
			if(c==KEY_ENTER){break;}
		}
	}
	wclear(stdscr);
	start_color();
	init_pair(HL_NUMBER, COLOR_GREEN, COLOR_BLACK); //초록색 글씨. 숫자 하이라이팅*/
	init_pair(HL_MATCH, COLOR_WHITE, COLOR_MAGENTA); // 검색 결과 하이라이팅
	init_pair(HL_STRING, COLOR_YELLOW, COLOR_BLACK);
	init_pair(HL_COMMENT, COLOR_PINK, COLOR_BLACK);
	init_pair(HL_KEYWORD1, COLOR_CYAN, COLOR_BLACK);
	init_pair(HL_KEYWORD2, COLOR_SKYBLUE, COLOR_BLACK);
	init_pair(HL_MLCOMMENT, COLOR_CHERRY, COLOR_BLACK);


	editor_set_status_message("HELP : Ctrl-S = save | Ctrl-Q = quit | Ctrl-F = find");

	while(1){
		editor_refresh_screen();
		editor_process_key_press();
	}


	refresh();
	endwin();
	end_recommend();
}