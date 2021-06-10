#include <ncurses.h>    
#include <locale.h>                                                     
                                                                             
int main()                                                                   
{       setlocale(LC_ALL, "");                                                       
        initscr();                      /* Start curses mode              */ 
        printw("%s", "Hello World !!! 가나다 ");      /* Print Hello World              */ 
        refresh();                      /* Print it on to the real screen for the blank for the blank for the blank for the blank for the blank for the blank*/ 
        getch();                        /* Wait for user input */            
        endwin();                       /* End curses mode                */ 

        int a=0;
        int b;
        return 0;                                                            
}