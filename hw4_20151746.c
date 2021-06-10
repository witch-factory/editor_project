#include <stdio.h>
#include <stdlib.h>

#define MAX_SIZE 100
typedef enum{head, entry}tagfield;
typedef struct matrix_node* matrix_pointer;

typedef struct {
	int row;
	int col;
	int value;
}entry_node;


typedef struct matrix_node{
	matrix_pointer down;
	matrix_pointer right;
	tagfield tag;
	union {
		matrix_pointer next;
		entry_node entry;
	}u;
}matrix_node;

matrix_pointer hdnode[MAX_SIZE];

matrix_pointer new_node();
matrix_pointer mread(FILE* matrix_file);
void mwrite(matrix_pointer node);
void merase(matrix_pointer* node);


int main(void) {
	matrix_pointer a=NULL, b=NULL;
	FILE* fp1, *fp2;

	fp1 = fopen("A.txt", "r");
	fp2 = fopen("B.txt", "r");

	a = mread(fp1);
	b = mread(fp2);
	mwrite(a);
	mwrite(b);

	merase(&a);
	merase(&b);
	mwrite(a);
	mwrite(b);


	return 0;
}

matrix_pointer new_node() {
	return (matrix_pointer)malloc(sizeof(matrix_node));
	/* matrix node allocation */
}

matrix_pointer mread(FILE* matrix_file) {
	int row_num, col_num, term_num, head_num;
	int row, col, value, current_row;
	int i, j;
	matrix_pointer temp, last, list_node;
	entry_node* node;

	node = (entry_node*)malloc(sizeof(entry_node) * MAX_SIZE);
	fscanf(matrix_file, "%d %d", &row_num, &col_num);
	node[0].row = row_num;
	node[0].col = col_num;
	node[0].value = 0;
	/* read matrix file and construct the array */
	for (i = 0; i < row_num; i++) {
		for (j = 0; j < col_num; j++) {
			fscanf(matrix_file, "%d", &value);

			if (value != 0) {
				node[0].value++;
				node[node[0].value].row = i;
				node[node[0].value].col = j;
				node[node[0].value].value = value;
			}
		}
	}
	term_num = node[0].value;
	head_num = (row_num > col_num) ? row_num : col_num;

	list_node = new_node();
	list_node->tag = entry;
	list_node->u.entry.row = row_num;
	list_node->u.entry.col = col_num;

	if (head_num == 0) { list_node->right = list_node; }
	/* empty matrix */
	else {
		/* head nodes initialization */
		for (i = 0; i < head_num; i++) {
			temp = new_node();
			hdnode[i] = temp;
			hdnode[i]->tag = head;
			hdnode[i]->right = temp;
			hdnode[i]->u.next = temp;
		}
		current_row = 0;
		last = hdnode[0];
		/* last node of the current row */

		for (i = 1; i <= term_num; i++) {
			row = node[i].row;
			col = node[i].col;
			value = node[i].value;

			if (row > current_row) {
				last->right = hdnode[current_row];
				current_row = row;
				last = hdnode[row];
			}

			temp = new_node();
			temp->tag = entry;
			temp->u.entry.row = row;
			temp->u.entry.col = col;
			temp->u.entry.value = value;
			last->right = temp;
			last = temp;
			hdnode[col]->u.next->down = temp;
			hdnode[col]->u.next = temp;
		}

		/* last row */
		last->right = hdnode[current_row];
		/* close all column list */
		for (i = 0; i < col_num; i++) {
			hdnode[i]->u.next->down = hdnode[i];
		}
		/* link all head nodes */
		for (i = 0; i < head_num - 1; i++) {
			hdnode[i]->u.next = hdnode[i + 1];
		}

		hdnode[head_num - 1]->u.next = list_node;
		list_node->right = hdnode[0];
	}
	return list_node;
}


void mwrite(matrix_pointer node) {
	if (node == NULL) {
		/* for empty matrix */
		printf("The given matrix is empty\n");
		return;
	}
	else {
		int i, j;
		int row_num = node->u.entry.row, col_num = node->u.entry.col;
		matrix_pointer temp, head = node->right;

		for (i = 0; i < row_num; i++) {
			temp = head->right;
			for (j = 0; j < col_num; j++) {
				if (temp->u.entry.row == i && temp->u.entry.col == j) {
					printf("%d", temp->u.entry.value);
					temp = temp->right;
				}
				else {
					printf("%d", 0);
				}
				if (j != col_num - 1) { printf(" "); }
			}
			printf("\n");
			head = head->u.next;
		}
	}
}


void merase(matrix_pointer* node) {
	if (*node==NULL){
		printf("The given matrix pointer is NULL\n");
	}
	else {
		int i;
		matrix_pointer x, y, head = (*node)->right;

		for (i = 0; i < (*node)->u.entry.row; i++) {
			/* free the nodes one by one */
			y = head->right;
			while (y != head) {
				x = y;
				y = y->right;
				free(x);
			}
			x = head;
			head = head->u.next;
			free(x);
		}

		/* free remaining head node */
		y = head;
		while (y != *node) {
			x = y;
			y = y->u.next;
			free(x);
		}
		free(*node);
		*node = NULL;
	}
}