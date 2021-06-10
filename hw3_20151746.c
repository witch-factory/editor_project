#include <stdio.h>
#include <stdlib.h>
#define MAX_EXPR_SIZE 100


typedef enum{lparen, rparen, plus, minus, times, divide, mod, eos, operand} precedence;

/* 링크드 리스트를 구성하는 노드 구조체 */
typedef struct node{
    precedence data;
    struct node* next;
}node;

/* 링크드 리스트를 이용한 스택의 head */
node* stack;

/* 수식 저장하는 배열*/
char expr[MAX_EXPR_SIZE];
/* 연산자의 우선순위를 나타내기 위한 숫자들을 담은 배열*/
int isp[]={0,19,12,12,13,13,13,0};
int icp[]={20,19,12,12,13,13,13,0};

void readExpr(FILE* expr_file);
int isEmpty(void);
void push(precedence token);
precedence top(void);
precedence pop(void);
precedence getToken(char* symbol, int* n);
void printToken(precedence token);
void postfix(void);



int main(void){
    FILE* expr_file;

    expr_file = fopen("expr.txt", "r");
    if (expr_file != NULL) {
        readExpr(expr_file);
        fclose(expr_file);
        postfix();
    }
    else {
        printf("Failed to open expr.txt\n");
    }

    return 0;
}

void readExpr(FILE* expr_file){
    char token;
    int idx=0;

    /* 파일 미리 연 다음 인자로 전달해 줘야 함 */
    while(fscanf(expr_file, "%c", &token)!=EOF){
        if(token==' '){continue;} /* 공백 넘어감 */
        else{
            expr[idx]=token; 
            idx++;
        }
    }
    expr[idx]=' '; /* 마지막 eos */
}

int isEmpty(void){
    if(stack->data==eos) {return 1;}
    else {return 0;}
}

void push(precedence token){
    node* newNode=(node*)malloc(sizeof(node));

    /* NULL 체크 후 할당 */
    if(newNode!=NULL){
        newNode->data=token;
        newNode->next=NULL; /* 마지막 노드의 다음 노드는 NULL로 할당 */
        if(stack!=NULL){newNode->next=stack;} /* head에 newNode 삽입*/
        stack=newNode;
    }
}

precedence top(void){
    if(isEmpty()==1){return eos;}
    else{return stack->data;}
}

precedence pop(void){
    /* top 노드를 삭제하고 거기 들어있던 데이터를 리턴*/
    node *temp;
    precedence data;

    if(isEmpty()==1){return eos;}

    data=stack->data;
    temp=stack;
    stack=stack->next;
    free(temp);

    return data;
}

precedence getToken(char *symbol, int *n){
    *symbol=expr[(*n)++];
    switch (*symbol){
        case '(':return lparen;
        case ')':return rparen;
        case '+':return plus;
        case '-':return minus;
        case '*':return times;
        case '/':return divide;
        case '%':return mod;
        case ' ':return eos;
        default:return operand;
    }
}

void printToken(precedence token){
    switch(token){
        case lparen:
        printf("( ");
        break;
        case rparen:
        printf(") ");
        break;
        case plus:
        printf("+ ");
        break;
        case minus:
        printf("- ");
        break;
        case times:
        printf("* ");
        break;
        case divide:
        printf("/ ");
        break;
        case mod:
        printf("%% ");
        break;
    }
}

void postfix(void){
    char symbol;
    precedence token;
    int n=0;
    push(eos);

    for(token=getToken(&symbol, &n); token!=eos; token=getToken(&symbol, &n)){
        if(token==operand){
            printf("%c ", symbol);
        }
        else if(token==rparen){
            /* 왼쪽 괄호가 나올 때까지 스택에 남은 것들 출력 */
            while(top()!=lparen){
                printToken(pop());
            }
            pop();
        }
        else{
            while(isp[top()]>=icp[token]){
                printToken(pop());
            }
            push(token);
        }
        
    }

    while((token=pop())!=eos){
        printToken(token);
    }
    printf("\n");
}