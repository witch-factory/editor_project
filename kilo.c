/* includes */
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <fcntl.h>

/* defines */

#define KILO_VERSION "0.0.1"
#define KILO_TAB_STOP 8
#define KILO_QUIT_TIMES 3

#define CTRL_KEY(k) ((k)&0x1f)

enum editorKey
{
  //방향키로 커서를 이동할 수 있게 하기 위하여
  //그리고 다른 아스키코드와 겹치지 않기 위해 char값보다 큰 값을 넣어줌
  BACKSPACE=127,
  ARROW_UP = 1000,
  ARROW_DOWN,
  ARROW_LEFT,
  ARROW_RIGHT,
  DEL_KEY,
  HOME_KEY,
  END_KEY,
  PAGE_UP,
  PAGE_DOWN
};

enum editorHighlight {
  HL_NORMAL = 0,
  HL_COMMENT,
  HL_KEYWORD1,
  HL_KEYWORD2,
  HL_STRING,
  HL_NUMBER,
  HL_MATCH
};

#define HL_HIGHLIGHT_NUMBERS (1 << 0)
#define HL_HIGHLIGHT_STRINGS (1<<1)

/*** data ***/

typedef struct {
  char *filetype;
  char **filematch;
  //각 filetype에 맞는 pattern을 담음
  char **keywords;
  char *singleline_comment_start;
  int flags;
}editorSyntax;

typedef struct
{
  //동적 할당된 배열에 텍스트 라인을 저장
  int size;
  int rsize;
  char *chars;
  char *render;
  unsigned char* hl;
  //to indicate that each character in render should be highlighted to some color or not
} erow;

typedef struct
{
  //에디터의 현 상태를 나타내주는 구조체 정의
  int cx, cy; //에디터 내의 커서 위치
  int rx;
  int rowoff;
  int coloff;
  int screenrows;
  int screencols;
  int numrows;
  erow *row;
  int dirty;
  //저장되지 않은 수정사항이 있는지를 뜻하는 변수. 수정사항 있으면 dirty!=0
  char *filename;
  char statusmsg[80];
  time_t statusmsg_time;
  editorSyntax *syntax;
  struct termios orig_termios;
} editorConfig;

editorConfig E;

/* file types */

char *C_HL_extensions[] = {".c", ".h", ".cpp", NULL};

char *C_HL_keywords[] = {"switch",    "if",      "while",   "for",    "break",
                         "continue",  "return",  "else",    "struct", "union",
                         "typedef",   "static",  "enum",    "class",  "case",
                         "int|",      "long|",   "double|", "float|", "char|",
                         "unsigned|", "signed|", "void|",   NULL};

editorSyntax HLDB[] = {
    {// highlight database
     "c", C_HL_extensions, C_HL_keywords, "//", HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS
     },
};


#define HLDB_ENTRIES (sizeof(HLDB)/sizeof(HLDB[0]))

/* prototypes */

void editorSetStatusMessage(const char *fmt, ...);
void editorRefreshScreen();
char *editorPrompt(char *prompt, void (*callback)(char *, int));

/* terminal */

void die(const char *s)
{
  //에러 발생시 스크린을 지우고 커서를 맨 앞으로 옮긴 후 에러 메시지 출력
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[1;1H", 3);

  perror(s);
  exit(1);
}

void disableRawMode()
{
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
  {
    die("tcsetattr");
  }
}

void enableRawMode()
{
  // rawmode를 실행시키고, ctrl+알파벳키로 실행되는 옵션들을 꺼준다
  struct termios raw = E.orig_termios;
  if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
  {
    die("tcsetattr");
  }
  atexit(disableRawMode);

  raw.c_iflag &= ~(ICRNL | IXON | BRKINT | INPCK | ISTRIP);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag |= (CS8);
  raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);

  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
  {
    die("tcsetattr");
  }
}

int editorReadKey()
{
  //입력되는 키를 읽음
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1)
  {
    if (nread == -1 && errno != EAGAIN)
    {
      die("read");
    }
  }

  if (c == '\x1b')
  {
    char seq[3];

    if (read(STDIN_FILENO, &seq[0], 1) != 1)
    {
      return '\x1b';
    }
    if (read(STDIN_FILENO, &seq[1], 1) != 1)
    {
      return '\x1b';
    }

    if (seq[0] == '[')
    {
      if (seq[1] >= '0' && seq[1] <= '9')
      {
        if (read(STDIN_FILENO, &seq[2], 1) != 1)
        {
          return '\x1b';
        }
        if (seq[2] == '~')
        {
          switch (seq[1])
          {
          case '1':
            return HOME_KEY;
          case '3':
            return DEL_KEY;
          case '4':
            return END_KEY;
          case '5':
            return PAGE_UP;
          case '6':
            return PAGE_DOWN;
          case '7':
            return HOME_KEY;
          case '8':
            return END_KEY;
          }
        }
      }
      else
      {
        switch (seq[1])
        {
        case 'A':
          return ARROW_UP;
        case 'B':
          return ARROW_DOWN;
        case 'C':
          return ARROW_RIGHT;
        case 'D':
          return ARROW_LEFT;
        case 'H':
          return HOME_KEY;
        case 'F':
          return END_KEY;
        }
      } //HOME, end 키에 반응하도록
    }
    else if (seq[0] == 'O')
    {
      switch (seq[1])
      {
      case 'H':
        return HOME_KEY;
      case 'F':
        return END_KEY;
      }
    }

    return '\x1b';
  }
  else
  {
    return c;
  }
}

int getCursorPosition(int *rows, int *cols)
{
  //커서의 위치 읽어서 출력해줌
  char buf[32];
  unsigned int i = 0;
  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
  {
    return -1;
  }

  while (i < sizeof(buf) - 1)
  {
    if (read(STDIN_FILENO, &buf[i], 1) != 1)
    {
      break;
    }
    if (buf[i] == 'R')
    {
      break;
    }
    i++;
  }
  buf[i] = '\0';

  if (buf[0] != '\x1b' || buf[1] != '[')
  {
    return -1;
  }
  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2)
  {
    //커서의 위치를 받아서 rows, cols에 넣어줌. 2개를 입력 못받으면 실패(scanf 리턴값 이용)
    return -1;
  }

  return 0;
}

int getWindowSize(int *rows, int *cols)
{
  //현재 터미널 윈도우 사이즈를 구해주고, 만약 에러 발생시 -1을 리턴
  struct winsize ws;

  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
  {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
    {
      //터미널 사이즈를 알기 위해서 커서를 터미널 맨 오른쪽 아래로 이동
      return -1;
    }
    return getCursorPosition(rows, cols);
  }
  else
  {
    *rows = ws.ws_row;
    *cols = ws.ws_col;
    return 0;
  }
}

/* syntax highlighting */

int is_separator(int c){
  return (isspace(c) || c=='\0' || strchr(",.()+-/*=~%<>[];", c)!=NULL);
}

void editorUpdateSyntax(erow *row){
  //row를 받아서 적절하게 highlight 숫자 할당해줌
  row->hl=realloc(row->hl, row->rsize);
  memset(row->hl, HL_NORMAL, row->rsize);

  if (E.syntax == NULL) {
    return;
  }

  char **keywords = E.syntax->keywords;
  
  char *scs = E.syntax->singleline_comment_start;
  int scs_len = scs ? strlen(scs) : 0;

  int prev_sep = 1;
  int in_string = 0;


  int i=0;
  while (i < (row->rsize)) {
    char c=row->render[i];
    unsigned char prev_hl = (i > 0) ? row->hl[i - 1] : HL_NORMAL;

    if (scs_len && !in_string) {
      if (!strncmp(&row->render[i], scs, scs_len)) {
        memset(&row->hl[i], HL_COMMENT, (row->rsize) - i);
        break;
      }
    }

    if (E.syntax->flags & HL_HIGHLIGHT_STRINGS) {
      if (in_string) {
        //string 내부에 있는지 아닌지를 in_string 변수로 판별
        row->hl[i] = HL_STRING;
        if (c == '\\' && i + 1 < row->rsize) {
          row->hl[i + 1] = HL_STRING;
          i += 2;
          continue;
        }
        if (c == in_string) {
          in_string = 0;
        }
        i++;
        prev_sep = 1;
        continue;
      } else {
        if (c == '"' || c == '\'') {
          in_string = c;
          row->hl[i] = HL_STRING;
          i++;
          continue;
        }
      }
    }
    

    if(E.syntax->flags & HL_HIGHLIGHT_NUMBERS){
      if ((isdigit(c) && (prev_sep || prev_hl == HL_NUMBER)) ||
          (c == '.' && prev_hl == HL_NUMBER)) {
        //.을 처리함으로써 소수도 처리 가능
        row->hl[i]=HL_NUMBER;
        i++;
        prev_sep=0;
        continue;
      }
    }

    if (prev_sep) {
      int j;
      for (j = 0; keywords[j]; j++) {
        int klen = strlen(keywords[j]);
        int kw2 = keywords[j][klen - 1] == '|';
        if (kw2) {
          klen--;
        }

        if (!strncmp(&row->render[i], keywords[j], klen) &&
            is_separator(row->render[i + klen])) {
          memset(&row->hl[i], kw2 ? HL_KEYWORD2 : HL_KEYWORD1, klen);
          i += klen;
          break;
        }
      }
      if (keywords[j] != NULL) {
        prev_sep = 0;
        continue;
      }
    }

    prev_sep=is_separator(c);
    i++;
  }
}

int editorSyntaxToColor(int hl){
  switch (hl) {
  case HL_COMMENT:
    return 36;
  case HL_KEYWORD1:
    return 33;
  case HL_KEYWORD2:
    return 32;
  case HL_STRING:
    return 35;
  case HL_NUMBER:
    return 31;
    //foreground red
  case HL_MATCH:
    return 34;
    //foreground blue
  default:
    return 37;
    //foreground white(default)
  }
}

void editorSelectSyntaxHighlight() {
  E.syntax = NULL;
  if (E.filename == NULL) {
    return;
  }

  char *ext = strrchr(E.filename, '.');

  for (unsigned int j = 0; j < HLDB_ENTRIES; j++) {
    editorSyntax *s = &HLDB[j];
    unsigned int i = 0;
    while (s->filematch[i]) {
      int is_ext = (s->filematch[i][0] == '.');
      if ((is_ext && ext && !strcmp(ext, s->filematch[i])) ||
          (!is_ext && strstr(E.filename, s->filematch[i]))) {
        E.syntax = s;

        int filerow;
        for (filerow = 0; filerow < E.numrows; filerow++) {
          editorUpdateSyntax(&E.row[filerow]);
          //모든 행 돌면서 다시 highlight syntax
        }
        return;
      }
      i++;
    }
  }
}

/* row operations */

int editorRowCxToRx(erow *row, int cx)
{
  int rx = 0, j;
  for (j = 0; j < cx; j++)
  {
    if (row->chars[j] == '\t')
    {
      rx += (KILO_TAB_STOP - 1) - (rx % KILO_TAB_STOP);
    }
    //cx는 탭의 끝으로부터 몇 글자 있는지, rx는 행의 시작으로부터 몇 글자 있는지
    rx++;
  }
  return rx;
}

int editorRowRxToCx(erow *row, int rx){
  int cur_rx=0;
  int cx;

  for(cx=0;cx<row->size;cx++){
    if(row->chars[cx]=='\t'){
      cur_rx+=(KILO_TAB_STOP-1)-(cur_rx % KILO_TAB_STOP);
    }
    cur_rx++;

    if(cur_rx>rx){return cx;}
  }
  return cx;
}

void editorUpdateRow(erow *row)
{
  //그 행에서 렌더링할 문자들로 업데이트
  int tabs = 0, j;
  for (j = 0; j < row->size; j++)
  {
    if (row->chars[j] == '\t')
    {
      tabs++;
    }
  }
  free(row->render);
  row->render = malloc(row->size + tabs * (KILO_TAB_STOP - 1) + 1);

  int idx = 0;
  for (j = 0; j < row->size; j++)
  {
    //tab을 스페이스 8개로 변경
    if (row->chars[j] == '\t')
    {
      row->render[idx++] = ' ';
      while (idx % KILO_TAB_STOP != 0)
      {
        row->render[idx++] = ' ';
      }
    }
    else
    {
      row->render[idx++] = row->chars[j];
    }
  }
  row->render[idx] = '\0';
  row->rsize = idx;

  editorUpdateSyntax(row);
}

void editorInsertRow(int at, char *s, size_t len)
{
  // 끝에 len 사이즈의 줄을 더해준다
  if(at<0 || at>E.numrows){return;}
  //validate at
  E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));
  memmove(&E.row[at+1], &E.row[at], sizeof(erow)*(E.numrows-at));

  //numrow개의 줄 중에서 어디에 있는가?
  E.row[at].size = len;
  E.row[at].chars = malloc(len + 1);
  memcpy(E.row[at].chars, s, len);
  E.row[at].chars[len] = '\0';

  E.row[at].rsize = 0;
  E.row[at].render = NULL;
  E.row[at].hl=NULL;
  editorUpdateRow(&E.row[at]);

  E.numrows++;
  E.dirty++;
}

void editorFreeRow(erow *row){
  free(row->render);
  free(row->chars);
  free(row->hl);
}

void editorDelRow(int at){
  if(at<0 || at>=E.numrows){return;}
  editorFreeRow(&E.row[at]);

  memmove(&E.row[at], &E.row[at+1], sizeof(erow)*(E.numrows-at-1));
  E.numrows--;
  E.dirty++;
}


void editorRowInsertChar(erow *row, int at, int c)
{
  //row의 at지점에 문자 c를 삽입
  if (at < 0 || at > row->size)
  {
    at = row->size;
  }
  row->chars = realloc(row->chars, row->size + 2);
  memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
  row->size++;
  row->chars[at] = c;
  editorUpdateRow(row);
  E.dirty++;
}

void editorRowAppendString(erow *row, char *s, size_t len){
  //row에 s문자열을 len 길이만큼 추가
  row->chars=realloc(row->chars, row->size+len+1);
  memcpy(&row->chars[row->size], s, len);
  row->size+=len;
  row->chars[row->size]='\0';
  editorUpdateRow(row);
  E.dirty++;
}

void editorRowDelChar(erow *row, int at){
  if(at<0 || at>=row->size){return;}
  //at가 row size의 범위를 벗어나면 안됨
  memmove(&row->chars[at], &row->chars[at+1], row->size-at);
  (row->size)--;
  editorUpdateRow(row);
  E.dirty++;
}

/* editor operations */

void editorInsertChar(int c)
{
  //커서의 위치에 새로운 문자 c 추가
  if (E.cy == E.numrows)
  //커서가 맨 끝 줄에 있으면 새로운 줄 추가
  {
    editorInsertRow(E.numrows, "", 0);
  }
  editorRowInsertChar(&E.row[E.cy], E.cx, c);
  E.cx++;
}

void editorInsertNewLine(){
  if(E.cx==0){
    editorInsertRow(E.cy, "", 0);
  }
  else{
    //행의 첫 부분이 아닌 곳에서 엔터를 치면 행의 내용이 둘로 나눠짐
    erow *row=&E.row[E.cy];
    editorInsertRow(E.cy+1, &row->chars[E.cx], row->size-E.cx);
    row=&E.row[E.cy];
    row->size=E.cx;
    row->chars[row->size]='\0';
    editorUpdateRow(row);
  }
  E.cy++;
  E.cx=0;
}

void editorDelChar(){
  if(E.cy==E.numrows){return;}
  if(E.cx==0 && E.cy==0){return;}
  //만약 커서가 (0,0)에 있으면 아무것도 못 지움

  erow *row=&E.row[E.cy];
  if(E.cx>0){
    editorRowDelChar(row, E.cx-1);
    //현재 커서 위치 바로 앞에 있는 글자를 지운다. 즉 백스페이스 기능
    E.cx--;
  }
  else{
    //만약 커서가 한 줄의 맨 첫 위치에 있는데 백스페이스 되면 바로 앞줄에 내용 붙임
    E.cx=E.row[E.cy-1].size;
    editorRowAppendString(&E.row[E.cy-1], row->chars, row->size);
    editorDelRow(E.cy);
    E.cy--;
  }
}

/* file i/o */

char *editorRowsToString(int *buflen){
  //에디터에 있는 문자열들을 모두 동적 배열에 저장한다
  int totlen=0, j;
  for(j=0;j<E.numrows;j++){
    totlen+=E.row[j].size+1;
  }
  *buflen=totlen;

  char *buf=malloc(totlen);
  char *p=buf;
  for(j=0;j<E.numrows;j++){
    memcpy(p, E.row[j].chars, E.row[j].size);
    p+=E.row[j].size;
    *p='\n';
    p++;
  }

  return buf;
}

void editorOpen(char *filename)
{
  //파일 이름을 받아서 그걸 읽고 에디터에 정보로 넣어준다
  free(E.filename);
  E.filename = strdup(filename);

  editorSelectSyntaxHighlight();

  FILE *fp = fopen(filename, "r");
  if (!fp)
  {
    die("fopen");
  }

  char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen;
  while ((linelen = getline(&line, &linecap, fp)) != -1)
  {
    while (linelen > 0 &&
           (line[linelen - 1] == '\n' || line[linelen - 1] == '\r'))
    {
      //끝의 개행이나 캐리지 리턴은 strip 해준다
      linelen--;
    }

    editorInsertRow(E.numrows, line, linelen);
  }
  free(line);
  fclose(fp);
  E.dirty = 0;
}


void editorSave(){
  //파일에 저장
  if(E.filename==NULL){
    E.filename=editorPrompt("Save as: %s (ESC to cancel)", NULL);
    if(E.filename==NULL){
      editorSetStatusMessage("Empty file name. Save aborted.");
      return;
    }
    editorSelectSyntaxHighlight();
  }

  int len;
  char *buf=editorRowsToString(&len);
  
  int fd=open(E.filename, O_RDWR | O_CREAT, 0644);
  if(fd!=-1){ //error handling
    if(ftruncate(fd,len)!=-1){
      if(write(fd,buf,len)==len){
        close(fd);
        free(buf);
        E.dirty=0;
        editorSetStatusMessage("%d bytes written to disk", len);
        //저장 성공
        return;
      }
    }
    close(fd);
  }
  free(buf);
  editorSetStatusMessage("Failed to Save! I/O error : %s", strerror(errno));
}

/* find */

void editorFindCallback(char *query, int key){
  static int last_match=-1;
  static int direction=1;

  static int saved_hl_line;
  static char *saved_hl=NULL;
  //탐색이 종료되면 highlighted된 match result들을 다시 되돌리기 위해서
  //탐색 전의 highlight 된 상태를 저장해놓음

  if(saved_hl){
    memcpy(E.row[saved_hl_line].hl, saved_hl, E.row[saved_hl_line].rsize);
    free(saved_hl);
    saved_hl=NULL;
  }

  if(key=='\r' || key=='\x1b'){
    //엔터나 esc누르면 검색을 종료. 따라서 last_match와 dir을 초기화
    last_match=-1;
    direction=1;
    return;
  }
  else if(key==ARROW_RIGHT || key==ARROW_DOWN){
    direction=1;
  }
  else if(key==ARROW_LEFT || key==ARROW_UP){
    direction=-1;
  }
  else{
    last_match=-1;
    direction=1;
  }

  if(last_match==-1){direction=1;}
  int current=last_match; //우리가 지금 탐색중인 인덱스
  int i;
  for(i=0;i<E.numrows;i++){
    current+=direction;
    if(current==-1){current=E.numrows-1;}
    else if(current==E.numrows){current=0;}
    erow *row=&E.row[current];
    char *match=strstr(row->render, query);
    if(match){
      last_match=current;
      E.cy=current;
      E.cx=editorRowRxToCx(row, match-(row->render));
      E.rowoff=E.numrows;

      saved_hl_line=current;
      saved_hl=malloc(row->rsize);
      memcpy(saved_hl, row->hl, row->rsize);
      memset(&row->hl[match-(row->render)], HL_MATCH, strlen(query));
      break;
    }
  }
}

void editorFind(){
  int saved_cx=E.cx;
  int saved_cy=E.cy;
  int saved_coloff=E.coloff;
  int saved_rowoff=E.rowoff;

  char *query=editorPrompt("Search : %s (Use ESC/Arrows/Enter)", editorFindCallback);
  //만약 쿼리가 Enter나 Esc가 아니라면 그 쿼리에 대해서 콜백함수로 문자열탐색진행
  if(query){
    free(query);
  }
  else{
    //query가 널, 즉 Esc 입력을 받으면 커서를 이전 위치로 되돌린다
    E.cx=saved_cx;
    E.cy=saved_cy;
    E.coloff=saved_coloff;
    E.rowoff=saved_rowoff;
  }
}

/* append buffer */

typedef struct
{
  char *b;
  int len;
} abuf;

#define ABUF_INIT \
  {               \
    NULL, 0       \
  }

void abAppend(abuf *ab, const char *s, int len)
{
  char *new = realloc(ab->b, ab->len + len);
  //len만큼 추가 할당

  if (new == NULL)
  {
    return;
  }
  memcpy(&new[ab->len], s, len);
  ab->b = new;
  ab->len += len;
}

void abFree(abuf *ab)
{
  free(ab->b);
}

/* output */

void editorScroll()
{
  //만약 커서가 창을 벗어나 있으면 창 내부로 옮겨줌
  E.rx = 0;
  if (E.cy < E.numrows)
  {
    E.rx = editorRowCxToRx(&E.row[E.cy], E.cx);
  }

  if (E.cy < E.rowoff)
  {
    E.rowoff = E.cy;
  }
  if (E.cy >= E.rowoff + E.screenrows)
  {
    E.rowoff = E.cy - E.screenrows + 1;
  }
  if (E.rx < E.coloff)
  {
    E.coloff = E.rx;
  }
  if (E.rx >= E.coloff + E.screencols)
  {
    E.coloff = E.rx - E.screencols + 1;
  }
}

void editorDrawRows(abuf *ab)
{
  //각 줄의 맨 앞에 ~ 표시를 그려줌
  int y;
  for (y = 0; y < E.screenrows; y++)
  {
    int filerow = y + E.rowoff;
    if (filerow >= E.numrows)
    {
      if (E.numrows == 0 && y == E.screenrows / 3)
      {
        //welcome message는 파일 연 게 없을 때만 출력
        char welcome[80];
        // welcome message 출력
        int welcomelen = snprintf(welcome, sizeof(welcome),
                                  "Kilo editor -- version %s", KILO_VERSION);
        if (welcomelen > E.screencols)
        {
          welcomelen = E.screencols;
        }

        int padding = (E.screencols - welcomelen) / 2;
        //center the welcome message
        if (padding)
        {
          abAppend(ab, "~", 1);
          padding--;
        }
        while (padding--)
        {
          abAppend(ab, " ", 1);
        }

        abAppend(ab, welcome, welcomelen);
      }
      else
      {
        abAppend(ab, "~", 1);
      }
    }
    else
    {
      int len = E.row[filerow].rsize - E.coloff;
      if (len < 0)
      {
        len = 0;
      }
      if (len > E.screencols)
      {
        len = E.screencols;
      }

      char *c=&E.row[filerow].render[E.coloff];
      unsigned char* hl=&E.row[filerow].hl[E.coloff];
      int current_color=-1;
      int j;
      for(j=0;j<len;j++){
        if(hl[j]==HL_NORMAL){
          if(current_color!=-1){
            //색이 흰색으로 바뀌면 배열에 표시해준다. 흰색일땐 current_color = -1
            abAppend(ab, "\x1b[39m", 5);
            current_color=-1;
          }
          abAppend(ab, &c[j], 1);
        }
        else{
          int color=editorSyntaxToColor(hl[j]);
          if(color!=current_color){
            current_color=color;
            char buf[16];
            int clen=snprintf(buf, sizeof(buf), "\x1b[%dm", color);
            abAppend(ab, buf, clen);
          }
          
          abAppend(ab, &c[j], 1);
        }
      }
      abAppend(ab, "\x1b[39m", 5);
    }

    abAppend(ab, "\x1b[K", 3);
    abAppend(ab, "\r\n", 2);
  }
}

void editorDrawStatusBar(abuf *ab)
{
  //상태 바는 파일명(20글자까지)과, 현재 몇 번째 줄에 있는지를 표시
  abAppend(ab, "\x1b[7m", 4);
  char status[80], rstatus[80];
  int len = snprintf(status, sizeof(status), "%.20s - %d lines %s",
                     E.filename ? E.filename : "[No Name]", E.numrows, E.dirty? "(modified)":"");
  int rlen = snprintf(rstatus, sizeof(rstatus), "%s | %d/%d",
                      E.syntax ? E.syntax->filetype : "no filetype", E.cy + 1,
                      E.numrows);
  if (len > E.screencols)
  {
    len = E.screencols;
  }
  abAppend(ab, status, len);
  while (len < E.screencols)
  {
    if (E.screencols - len == rlen)
    {
      abAppend(ab, rstatus, rlen);
      break;
    }
    else
    {
      abAppend(ab, " ", 1);
      len++;
    }
  }
  abAppend(ab, "\x1b[m", 3);
  abAppend(ab, "\r\n", 2);
}

void editorDrawMessageBar(abuf *ab)
{
  abAppend(ab, "\x1b[K", 3);
  int msglen = strlen(E.statusmsg);
  if (msglen > E.screencols)
  {
    msglen = E.screencols;
  }
  if (msglen && time(NULL) - E.statusmsg_time < 5)
  {
    abAppend(ab, E.statusmsg, msglen);
  }
}

void editorRefreshScreen()
{
  //스크린 싹 밀고 커서를 맨 위로 옮겨줌
  editorScroll();

  abuf ab = ABUF_INIT;

  abAppend(&ab, "\x1b[?25l", 6);
  //abAppend(&ab, "\x1b[2J", 4);
  abAppend(&ab, "\x1b[H", 3);

  editorDrawRows(&ab);
  editorDrawStatusBar(&ab);
  editorDrawMessageBar(&ab);

  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1, (E.rx - E.coloff) + 1);
  abAppend(&ab, buf, strlen(buf));

  abAppend(&ab, "\x1b[?25h", 6);

  write(STDOUT_FILENO, ab.b, ab.len);
  abFree(&ab);
}

void editorSetStatusMessage(const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
  va_end(ap);
  E.statusmsg_time = time(NULL);
}

/* input */

char *editorPrompt(char *prompt, void (*callback)(char*, int)){
  size_t bufsize=128;
  char *buf=malloc(bufsize);

  size_t buflen=0;
  buf[0]='\0';

  while(1){
    editorSetStatusMessage(prompt, buf);
    editorRefreshScreen();

    int c=editorReadKey();
    if(c==DEL_KEY || c==CTRL_KEY('h') || c==BACKSPACE){
      if(buflen!=0){buf[--buflen]='\0';}
    }
    else if(c=='\x1b'){
      editorSetStatusMessage("");
      if(callback){callback(buf, c);}
      free(buf);
      return NULL;
    }
    else if(c=='\r'){
      if(buflen!=0){
        editorSetStatusMessage("");
        if(callback){callback(buf, c);}
        return buf;
      }
    }
    else if(!iscntrl(c) && c<128){
      if(buflen==bufsize-1){
        bufsize*=2;
        buf=realloc(buf, bufsize);
      }
      buf[buflen++]=c;
      buf[buflen]='\0';
    }
    if(callback){callback(buf, c);}
  }
}

void editorMoveCursor(int key)
{
  erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];

  switch (key)
  {
  case ARROW_UP:
    if (E.cy != 0)
    {
      E.cy--;
    }
    break;
  case ARROW_DOWN:
    if (E.cy < E.numrows)
    {
      E.cy++;
    }
    break;
  case ARROW_LEFT:
    if (E.cx != 0)
    {
      E.cx--;
    }
    else if (E.cy > 0)
    {
      E.cy--;
      E.cx = E.row[E.cy].size;
    }
    break;
  case ARROW_RIGHT:
    if (row && E.cx < row->size)
    {
      //줄의 끝에 도달해 있으면 더 이상 못 나아간다
      E.cx++;
    }
    else if (row && E.cx == row->size)
    {
      //끝에서 오른쪽으로 한번 가면 다음 줄의 첫 글자로 커서 이동
      E.cy++;
      E.cx = 0;
    }
    break;
  }

  row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
  //커서가 줄의 끝을 넘어가 있으면 그걸 줄의 끝으로 돌려줌
  int rowlen = row ? row->size : 0;
  if (E.cx > rowlen)
  {
    E.cx = rowlen;
  }
}

void editorProcessKeypress()
{
  static int quit_times=KILO_QUIT_TIMES;

  //계속 키 입력 받고, 만약 ctrl+q 눌리면 exit
  int c = editorReadKey();

  switch (c)
  {

  case '\r':
    editorInsertNewLine();
    break;

  case CTRL_KEY('q'):
    if(E.dirty && quit_times>0){
      editorSetStatusMessage("WARNING!!! File has unsaved changes." "Press Ctrl+Q %d more times to quit anyway.", quit_times);
      quit_times--;
      return;
    }
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[1;1H", 3);
    exit(0);
    break;

  case CTRL_KEY('s'):
    editorSave();
    break;

  case HOME_KEY:
    E.cx = 0;
    break;
  case END_KEY:
    if (E.cy < E.numrows)
    {
      E.cx = E.row[E.cy].size;
    }
    break;

  case CTRL_KEY('f'):
    editorFind();
    break;

  case BACKSPACE:
  case CTRL_KEY('h'):
  case DEL_KEY:
    if(c==DEL_KEY){editorMoveCursor(ARROW_RIGHT);}
    //만약 delete키면 커서 오른쪽에 있는 문자를 지워야 하므로.
    editorDelChar();
    break;

  case PAGE_UP:
  case PAGE_DOWN:
  {
    // page up/down 만들기
    if (c == PAGE_UP)
    {
      E.cy = E.rowoff;
    }
    else if (c == PAGE_DOWN)
    {
      E.cy = E.rowoff + E.screenrows - 1;
      if (E.cy > E.numrows)
      {
        E.cy = E.numrows;
      }
    }

    int times = E.screenrows;
    while (times--)
    {
      editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
    }
  }
  break;

  case ARROW_UP:
  case ARROW_DOWN:
  case ARROW_LEFT:
  case ARROW_RIGHT:
    editorMoveCursor(c);
    break;

  case CTRL_KEY('l'):
  case '\x1b':
  //우리가 설정하지 않은 이스케이프 시퀀스 입력은 무시한다
    break;

  default:
    editorInsertChar(c);
    //만약 Ctrl과 함께 눌리는 연산에 포함되지 않으면, 그 문자를 추가한다
    break;
  }

  quit_times=KILO_QUIT_TIMES;
}

/* init */

void initEditor()
{
  E.cx = 0;
  E.cy = 0;
  E.rx = 0;
  E.rowoff = 0;
  E.coloff = 0;
  E.numrows = 0;
  E.row = NULL;
  E.dirty = 0;
  E.filename = NULL;
  E.statusmsg[0] = '\0';
  E.statusmsg_time = 0;
  E.syntax = NULL;

  if (getWindowSize(&E.screenrows, &E.screencols) == -1)
  {
    die("getWindowSize");
  }
  E.screenrows -= 2;
}

int main(int argc, char *argv[])
{
  enableRawMode();
  initEditor();
  if (argc >= 2)
  {
    editorOpen(argv[1]);
  }

  editorSetStatusMessage(
    "HELP: Ctrl+S = save | Ctrl+Q = quit | Ctrl+F = find");

  while (1)
  {
    editorRefreshScreen();
    editorProcessKeypress();
  }

  return 0;
}