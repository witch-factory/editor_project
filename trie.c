#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHAR_SIZE 96
/* 32~127 ASCII code is used. */
#define CHAR_TO_INDEX(c) ((int)c - (int)' ')

typedef struct trie {
	int is_leaf;
	int branch;
	int words;
	struct trie* ch[CHAR_SIZE];
}trie;


trie* get_new_trie_node();
void trie_insert_string(trie* head, char* str);
int search(trie* head, char* str);
int has_children(trie* cur);
int trie_deletion(trie** cur, char* str);
void free_trie(trie* node);

/* C++ string function imitation*/
char* string_append(char* str, char part);
char* string_pop_back(char* str);
char* string_copy(char* original);

/* autocomplete functions */
void suggestion(trie* root, char* prefix);
int print_auto_suggestion(trie* root, char* query);

/* deprecated functions */
int consistent(trie* head);
long long count_key(trie* head, int isroot);


int main(void) {

	int comp, t = -1;
	trie* root = get_new_trie_node();

	trie_insert_string(root, "hello");
	trie_insert_string(root, "dog");
	trie_insert_string(root, "hell");
	trie_insert_string(root, "cat");
	trie_insert_string(root, "a");
	trie_insert_string(root, "hel");
	trie_insert_string(root, "help");
	trie_insert_string(root, "helps");
	trie_insert_string(root, "helping");
	t=trie_deletion(&root, "help");
	/* delete 시에는 root의 주소를 전달해야 하는 것을 주의 */

	//printf("%d\n", t);
	comp = print_auto_suggestion(root, "hel");

	if (comp == -1)
		printf("No other strings found with this prefix\n");

	else if (comp == 0)
		printf("No string found with this prefix\n");


	return 0;
}


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

void suggestion(trie* root, char* prefix) {
	//find the words starts with prefix string
	int i, len;
	len = strlen(prefix);

	if (root->is_leaf) {
		printf("%s\n", prefix);
	}

	if (has_children(root) == 0) {
		//no child. That is, no word in trie starts with this prefix
		return;
	}

	for (i = 0; i < CHAR_SIZE; i++) {
		if (root->ch[i]) {
			prefix = string_append(prefix, ' ' + i);
			suggestion(root->ch[i], prefix);
			prefix = string_pop_back(prefix);
		}
	}
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

int print_auto_suggestion(trie* root, char* query) {
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
		printf("%s", query);
		return -1;
	}

	if (is_last == 0) {
		prefix = string_copy(query);
		suggestion(crawl, prefix);
		return 1;
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


int consistent(trie* head) {
	/* used to solve BOj. deprecated */
	int i;
	if (head->is_leaf && has_children(head)) { return 0; }
	for (i = 0; i < CHAR_SIZE; i++) {
		if (head->ch[i] && !(consistent(head->ch[i]))) { return 0; }
	}
	return 1;
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


long long count_key(trie* head, int isroot) {
	//used to solve BOJ. deprecated 
	int i;
	long long res = 0;
	if (isroot == 1 || (head->branch) > 1) { res = head->words; }
	for (i = 0; i < CHAR_SIZE; i++) {
		if (head->ch[i]) { res += count_key(head->ch[i], 0); }
	}
	return res;

}
