#include <stdio.h>
#include <ctype.h>

int main(){
        printf("%d", isspace(' '));
}

enum separator{
	NORMAL_CHAR=0,
	SEP_SPACE=1,
	SEP_NULL=2,
	SEP_OTHER=3,
	SEP_NAMEEND=4
}

WINDOW* win;
		if((Editor.cy-Editor.rowoff)+SHOWCNT+2>Editor.screenrows){
			/* 창이 스크린보다 내려가야 할 때 */
			if(Editor.rx+WORDMAX+2>Editor.coloff+Editor.screencols){
				win=newwin(SHOWCNT+2, WORDMAX+2, Editor.cy-Editor.rowoff-(SHOWCNT+2), Editor.rx-Editor.coloff-(WORDMAX+2));
			}
			else{
				win=newwin(SHOWCNT+2, WORDMAX+2, Editor.cy-Editor.rowoff-(SHOWCNT+2), Editor.rx);
			}
		}
		else{
			/* 창이 순방향으로 출력될 수 있다 */
			if(Editor.rx+WORDMAX+2>Editor.coloff+Editor.screencols){
				win=newwin(SHOWCNT+2, WORDMAX+2, Editor.cy-Editor.rowoff+1, Editor.rx-Editor.coloff-(WORDMAX+2));
			}
			else{
				win=newwin(SHOWCNT+2, WORDMAX+2, Editor.cy-Editor.rowoff+1, Editor.rx-Editor.coloff);
			}
		}
