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


float info_gain(int** train_data_feature, int* train_data_class, int* selected_info_index, int given_info_index, int data_num, int attr_num, int class_num) {
	int** modified_data_feature;
	int* modified_data_class;
	int modified_data_num;
	int i;
	float prior_entropy;
	float info_given_entropy;

	prior_entropy = column_entropy(train_data_class, data_num, class_num);

	info_given_entropy = 0;
	for (i = 0; i < class_num; i++) {
		modify_by_given_info(train_data_feature, train_data_class, &modified_data_feature, &modified_data_class, given_info_index, i, data_num, attr_num, &modified_data_num);
		info_given_entropy += column_entropy(modified_data_class, modified_data_num, class_num);
		/* 어차피 클래스는 2가지로 통일 */
	}

}

void numeric_to_categoric(int* numeric_data_column, int data_length) {
	/* 추가 : 나중에 테스트에서도 numeric 데이터 받아야 하므로 split point도 어딘가 저장해 놔야함 */
	int* temp_data_column;
	int temp_split_point;
	int split_point;
	int split_idx;
	int i, j;
	float temp_entropy;
	float lowest_entropy;


	temp_data_column = malloc(sizeof(int) * data_length);
	split_idx = 1;
	split_point = (numeric_data_column[split_idx] + numeric_data_column[split_idx - 1]) / 2;
	for (i = 0; i < data_length; i++) {
		//printf("%d\n", numeric_data_column[i]);
		if (numeric_data_column[i] > split_point) {
			/* 특정 split point를 기준으로 크면 1, 작으면 0을 배정해서 binary split */
			temp_data_column[i] = 1;
		}
		else {
			temp_data_column[i] = 0;
		}
	}
	lowest_entropy = column_entropy(temp_data_column, data_length, 2);

	for (split_idx = 1; split_idx < data_length; split_idx++) {
		temp_split_point = (numeric_data_column[split_idx] + numeric_data_column[split_idx - 1]) / 2;
		for (i = 0; i < data_length; i++) {
			if (numeric_data_column[i] > temp_split_point) {
				/* 특정 split point를 기준으로 크면 1, 작으면 0을 배정해서 binary split */
				temp_data_column[i] = 1;
			}
			else {
				temp_data_column[i] = 0;
			}
		}
		temp_entropy= column_entropy(temp_data_column, data_length, 2);
		if (temp_entropy < lowest_entropy) {
			lowest_entropy = temp_entropy;
			split_point = temp_split_point;
		}
		
	}

	for (i = 0; i < data_length; i++) {
		if (numeric_data_column[i] > split_point) {
			numeric_data_column[i] = 1;
		}
		else {
			numeric_data_column[i] = 0;
		}
	}

	//printf("%f\n", lowest_entropy);
	/* 가장 작은 엔트로피 출력 디버깅 */

	/*for (i = 0; i < data_length; i++) {
		printf("%d ", numeric_data_column[i]);
	}printf("\n");*/

}

void train_data_to_categoric(int** train_data_feature, int data_num, int numeric_attr_num) {
	int* temp_numeric_attr;
	int i, j;

	temp_numeric_attr = malloc(sizeof(int) * data_num);

	for (i = 0; i < numeric_attr_num; i++) {
		for (j = 0; j < data_num; j++) {
			temp_numeric_attr[j] = train_data_feature[j][i];
			/* 각 attribute column 마다의 feature들 일시적으로 복사 */
		}
		numeric_to_categoric(temp_numeric_attr, data_num);
		for (j = 0; j < data_num; j++) {
			train_data_feature[j][i] = temp_numeric_attr[j];
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

int main(void) {
	FILE* train_file;
	int i, j;
	int data_num, attr_num;
	int numeric_attr_num, categoric_attr_num;
	int** train_data_feature;
	int* train_data_class;

	int* temp_numeric_attr;


	int** test_data_feature;
	int* test_data_class;


	/* train data 입력받기 */

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