#include <stdio.h>
#include <stdlib.h>
#include <math.h>


typedef struct node{
	struct node* parent;
	struct node* left_child;
	struct node* right_child;
	//int* selected_attr_nums;
	/* 이미 선택되어서, 더 이상 생각할 필요 없는 attr들의 인덱스*/
	float entropy;
	/* 그 노드까지 왔을 때의 엔트로피를 담아놓고 있어야 한다 */
	int split_point;
	//int* data;
	//int attr;
	int is_leaf;
	/* 리프노드면 data_class를 갖고 있어야 한다 */
	int val;
	int data_class;
}node;

node* root;

void add_new_node(node* parent, int left, float entropy) {
	node* child = malloc(sizeof(node));

	child->parent = parent;
	child->left_child = NULL;
	child->right_child = NULL;

	child->entropy = entropy;
	
	if (left == 1) { parent->left_child = child; }
	else { parent->right_child = child; }
}


float probability_info(float p) {
	if (p <= 0 || p >= 1) { return 0;}
	return (-1.0) * p * (log(p)) / log(2);
}

int find_index(int* arr, int arr_size, int value) {
	int idx;
	for (idx = 0; idx < arr_size; idx++) {
		if (arr[idx] == value) {
			return idx;
		}
	}
	return -1;
}

float column_entropy(int* data_column, int data_length, int class_num) {
	/* 특정 데이터 column을 받아서 그 엔트로피 계산 */
	/* 데이터의 숫자인 data_length와 선택지가 몇 개로 나눠지는지인 class_num */
	float* probability;
	int i;
	float column_entropy;
	probability = malloc(sizeof(float) * class_num);

	for (i = 0; i < split_num; i++) {
		probability[i] = 0;
	}

	for (i = 0; i < data_length; i++) {
		probability[data_column[i]]+=1;
	}

	/*for (i = 0; i < split_num; i++) {
		printf("%d\n", probability[i]);
	}*/

	for (i = 0; i < class_num; i++) {
		/* 각 경우의 확률 */
		probability[i] = (float)probability[i] / data_length;
	}

	column_entropy = 0;
	for (i = 0; i < class_num; i++) {
		column_entropy += probability_info(probability[i]);
	}

	return column_entropy;
}


/*float class_entropy(int* class_column, int data_num, int class_num) {
	float* probability;
	int i;
	float column_entropy;

	probability = malloc(sizeof(float) * class_num);
	for (i = 0; i < class_num; i++) {
		probability[i] = 0;
	}

	for (i = 0; i < data_num; i++) {
		probability[class_column[i]]++;
	}

	for (i = 0; i < class_num; i++) {
		probability[i] = (float)probability[i] / data_num;
	}

	column_entropy = 0;
	for (i = 0; i < class_num; i++) {
		column_entropy += probability_info(probability[i]);
	}

	return column_entropy;

}*/

void modify_by_given_info(int** train_data_feature, int* train_data_class, int*** modified_data_feature, int** modified_data_class, int given_info_index, int given_info_label, int data_num, int attr_num, int* modified_data_num) {
	/* train data feature에서 given_info_index의 내용이 given_info_label에 맞는 data 만 뽑아서 modified_data_feature랑 modified_data_class 에 복사한 배열을 리턴해줌 */
	//int modified_data_num;
	int i, j;
	int modify_row_num;

	*modified_data_num = 0;
	for (i = 0; i < data_num; i++) {
		if (train_data_feature[i][given_info_index] == given_info_label) { (*modified_data_num)++; }
		/* 주어진 어트리뷰트 인덱스가 적절한 라벨일 때 데이터를 세기*/
	}


	*modified_data_feature = malloc(sizeof(int*) * (*modified_data_num));
	*modified_data_class = malloc(sizeof(int) * (*modified_data_num));
	for (i = 0; i < *modified_data_num; i++) {
		(*modified_data_feature)[i] = malloc(sizeof(int) * attr_num);
	}

	modify_row_num = 0;
	for (i = 0; i < data_num; i++) {
		if (train_data_feature[i][given_info_index] == given_info_label) {
			for (j = 0; j < attr_num; j++) {
				(*modified_data_feature)[modify_row_num][j] = train_data_feature[i][j];
			}
			(*modified_data_class)[modify_row_num] = train_data_class[i];
			modify_row_num++;
		}
	}
}


int read_train_file(FILE* train_file, int* data_num, int* numeric_attr_num, int* categoric_attr_num, int*** train_data_feature, int** train_data_class) {
	int attr_num;
	int i, j;
	fscanf(train_file, "%d %d %d", data_num, numeric_attr_num, categoric_attr_num);
	attr_num = (*numeric_attr_num) + (*categoric_attr_num);

	printf("%d %d %d\n", *data_num, *numeric_attr_num, *categoric_attr_num);
	//printf("%d\n", attr_num);

	*train_data_feature= malloc((*data_num) * sizeof(int*));
	for (i = 0; i < *data_num; i++) {
		(*train_data_feature)[i] = malloc((attr_num) * sizeof(int));
	}
	*train_data_class = malloc((*data_num) * sizeof(int));

	for (i = 0; i < *data_num; i++) {
		for (j = 0; j < attr_num; j++) {
			fscanf(train_file, "%d", &(*train_data_feature)[i][j]);
		}
		fscanf(train_file, "%d", &(*train_data_class)[i]);
	}
}

	train_file = fopen("train.txt", "r");

	read_train_file(train_file, &data_num, &numeric_attr_num, &categoric_attr_num, &train_data_feature, &train_data_class);

	/* train data 잘 입력되었나 출력해보기 */
	attr_num = numeric_attr_num + categoric_attr_num;

	for (i = 0; i < data_num; i++) {
		for (j = 0; j < attr_num; j++) {
			printf("%d ", train_data_feature[i][j]);
		}
		printf("%d\n", train_data_class[i]);
	}

	train_data_to_categoric(train_data_feature, data_num, numeric_attr_num);

	/* split된 데이터 출력해보기 */

	for (i = 0; i < data_num; i++) {
		for (j = 0; j < attr_num; j++) {
			printf("%d ", train_data_feature[i][j]);
		}
		printf("%d\n", train_data_class[i]);
	}

	modify_by_given_info(train_data_feature, train_data_class, &test_data_feature, &test_data_class, 0, 0, data_num, attr_num);
	


	return 0;
}
