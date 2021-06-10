#include "kilo.h"
#include <ncurses.h>

/* 자동완성을 위한 노드 구조체*/
typedef struct word_node{
	char* word;
	struct word_node* next;
} word_node;

word_node* list;

/* 스택을 위한 노드 구조체 */
typedef struct char_node{
	char data;
	struct char_node* next;
} char_node;

/*** 자동완성 
	kilo.h : editorUpdateSyntax() : line 406
***/

/* save.txt에 저장된 단어들로 트리를 만드는 함수
	현재 사용 안함 */
void InitWordNode(FILE* fp){
	// incomplete: read save.txt and make heap tree out of words
	char* input;
	while(!fscanf(fp, "%s ", input)){
			word_node* new = (word_node*)malloc(sizeof(word_node));
		strcpy(new->word, input);
	}

}

/* 연결리스트에 맨 앞에 새로운 단어 삽입 */
void InsertList(char* start, int len){
	word_node* new = (word_node*)malloc(sizeof(word_node));
	new->word = (char*)malloc(sizeof(char)*(len+1));
	strncpy(new->word, start, len);
	new->word[len] = '\0';
	if(list){
		new->next = list->next; 
		list->next = new;
	}
	else{
		list=new;
		list->next=NULL;
	}
}

/* 연결 리스트 삭제 및 메모리 해제 */
void EraseList(){
	word_node* del;
	while(list){
		del=list;
		list=list->next;
		free(del);
	}
}

/* 텍스트 에디터 프로그램 종료 직전에 save.txt에 자동완성용 연결리스트에 저장된 단어들 저장 */
void EndRecommend(){
	FILE* fp = fopen("save.txt","w");
	if(fp){
		word_node* cur = list;
		while(cur){
			fprintf(fp, "%s\n",cur->word);
			cur=cur->next;
		}
		fclose(fp);
	}
	else{
		fprintf(stderr, "cannot open file\n");
	}
	EraseList();
}

/*** 스택 push/pop ***/

/* 스택 push */
void InsertCharList(char_node** clist, char c) {
	char_node* new = (char_node*)malloc(sizeof(char_node));
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

/* 스택 pop */
void DeleteCharList(char_node** clist) {
	if (!(*clist)) return;
	char_node* del = (*clist);
	(*clist) = (*clist)->next;
	free(del);
}

/*** FindKMP : 단어 검색
	kilo.h 에 editorFindCallback()에서 kmp 사용 ***/

void FailFunc(char* pat){
	int i,k;
	int n=strlen(pat);
	for(int i=0;i<2;i++) fail[i]=(char*)realloc(fail[i], sizeof(char)*n);

	/* --> : 오른쪽에 있는 단어 탐색할 때 사용할 실패함수*/
	fail[0][0] = -1;
	for(k=1; k<n; k++){
		i = fail[0][k-1];
		while(pat[k] != pat[i+1] && i>=0 ) i=fail[0][i];
		if(pat[k] == pat[i+1]) fail[0][k] = i+1;
		else fail[0][k] = -1;
	}

	/* <-- : 거꾸로 탐색할 때 사용할 실패함수 (왼쪽에 있는 단어 탐색 */
	fail[1][n-1] = n;
	for(k=n-2; k>=0; k--){
		i = fail[1][k+1];
		while(pat[k] != pat[i-1] && i<n) i = fail[1][i];
		if(pat[k] == pat[i-1]) fail[1][k] = i-1;
		else fail[1][k] = n;
	}
}

char* Pmatch(char* string, char* pat, int* i, int dir){
	// string의 (*i)번째 인덱스에서 탐색 시작. 
	int k;
	int lens=strlen(string);
	int lenp=strlen(pat);
	if(dir == 1){ // 오른쪽 방향으로 탐색
		k=0;
		while((*i)<lens && k<lenp){
			if(string[*i] == pat[k]){
				(*i)++; k++;
			}
			else if( k==0) (*i)++;
			else k = fail[0][k-1]+1;
		}
		return (k==lenp)? (string + (*i)-lenp) : NULL;
	}
	else{ // 왼쪽 방향으로 탐색
		k=lenp-1;
		while((*i) >=0 && k >= 0){
			if(string[*i] == pat[k]){
				(*i)--; k--;
			}
			else if ( k == lenp-1 ) (*i)--;
			else k = fail[1][k+1]-1;
		}
		return (k == -1)? (string + (*i) + 1) : NULL;
	}
}

/*** FindBracket: 괄호 쌍 찾기 ***/
/*
	kilo.h : bracket_pair[2][2]
	하이라이트 하기 위해 괄호쌍 위치 일단 저장. 현재 사용 안함.
	bracket_pair[0]에 쌍을 찾을 괄호 위치 저장
	bracket_pair[1] = {-1,-1} 이면 현재 괄호에 쌍 없음.
*/

/*
	현재 위치에서 파일 끝까지 완전 탐색하면서 
	현재 괄호와 동일한 괄호가 있으면 push,
	현재 괄호와 다른 괄호가 있으면 pop.
	스택이 비면 괄호 쌍을 찾음.
	파일 끝까지 다 탐색했는데 스택이 안 비었으면 괄호 쌍이 없음 -> 현재 아무 일도 안 일어남 (커서 위치 안 바뀜)
*/
void FindBracket(){
	int r = E.cy;
	int c = E.cx;
	char_node* stack = NULL;
	short int found = 0;
	if(E.row[r].chars[c] == '{' && E.row[r].hl[c] == HL_NORMAL){ // 커서 위치에 string/comment가 아닌 '{'가 있을 때
		InsertCharList(&stack, '{');
		bracket_pair[0][0]=E.cy; 
		bracket_pair[0][1]=E.cx;
		c++;
		/* 파일 끝까지 완전 탐색*/
		for(c; c<E.row[r].rsize; c++){
			if(E.row[r].chars[c] == '{' && E.row[r].hl[c] == HL_NORMAL){
				InsertCharList(&stack, '{');
			}
			else if (E.row[r].chars[c] == '}'&& E.row[r].hl[c] == HL_NORMAL){
				DeleteCharList(&stack);
				if(!stack) { // empty stack
					bracket_pair[1][0]=r;
					bracket_pair[1][1]=c;
					E.cx = c;
					return;
				}
			}
		}
		r++;
		for(r ; r< E.numrows; r++){
			for(c=0; c<E.row[r].rsize; c++){
				if(E.row[r].chars[c] == '{'&& E.row[r].hl[c] == HL_NORMAL){
					InsertCharList(&stack, '{');
				}
				else if (E.row[r].chars[c] == '}'&& E.row[r].hl[c] == HL_NORMAL){
					DeleteCharList(&stack);
				// if stack is empty: found pair
				// hightlight -> goto endbracket -> break
					if(!stack){
						bracket_pair[1][0]=r;
						bracket_pair[1][1]=c;
						E.cx = c;
						E.cy = r;
						return;
					}
				}
			}
		}

	}
	else if (E.row[r].chars[c] == '}'&& E.row[r].hl[c] == HL_NORMAL){ // 커서 위치에 string/comment가 아닌 '}'가 있을 때
		InsertCharList(&stack, '}');
		bracket_pair[0][0]=E.cy;
		bracket_pair[0][1]=E.cx;
		c--;
		/* 파일 끝까지 완전 탐색*/
		for(c; c >=0; c--){
			if(E.row[r].chars[c] == '{'&& E.row[r].hl[c] == HL_NORMAL){
				DeleteCharList(&stack);
				if(!stack) { // empty stack
					bracket_pair[1][0]=r;
					bracket_pair[1][1]=c;
					E.cx = c;
					return;
				}
			}
			else if (E.row[r].chars[c] == '}'&& E.row[r].hl[c] == HL_NORMAL){
				InsertCharList(&stack, '}');
			}
		}
		r--;
		for(r ; r >= 0; r--){
			for(c=E.row[r].rsize - 1; c >=0; c--){
				if(E.row[r].chars[c] == '}'&& E.row[r].hl[c] == HL_NORMAL){
					InsertCharList(&stack, '}');
				}
				else if (E.row[r].chars[c] == '{'&& E.row[r].hl[c] == HL_NORMAL){
					DeleteCharList(&stack);
				// if stack is empty: found pair
				// hightlight -> goto endbracket -> break
					if(!stack){
						bracket_pair[1][0]=r;
						bracket_pair[1][1]=c;
						E.cx = c;
						E.cy = r;
						return;
					}
				}
			}
		}
	}

}


int main(int argc, char* argv[]) {
	//initscr();

	enableRawMode();
	initEditor();
	if(argc>=2){
		editorOpen(argv[1]);
	}

//	FILE* fp = fopen("save.txt","r");
//	if(!fp){
//		// error message & quit
//	}

	editorSetStatusMessage("HELP: Ctrl-S = save | Ctrl-Q = quit | Ctrl-F =  find");

	while (1) {
		editorRefreshScreen();
		editorProcessKeypress();
		//editorSetStatusMessage("(%d %d)", bracket_pair[0][0], bracket_pair[1][0]); // dbg
	}
	
	//endwin();
	return 0;
}
