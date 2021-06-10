#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* linked list structure */

typedef struct node {
	int data; /* save the index of the input_data*/
	struct node* prev;
	struct node* next;
}node;

typedef struct linked_list {
	node* header;
	node* trailer;
}linked_list;

/* linked list functions */

void init_list(linked_list* l);

void insert_between(linked_list* list, node* pre, node* suc, int new_data);

void push_center(linked_list* list, int new_data);

void push_data(linked_list* list, int new_data);


/* k-medoids clustering functions */

int cluster_center(linked_list list);

void allocate_to_cluster(linked_list* clusters, float** input_data, int data_num, int feature_num, int k_num);

int make_new_medoid(linked_list* list, float** input_data, int feature_num);

void clear_cluster(linked_list* cluster, int k_num);

void copy_array(int* copy_array, int* original_array, int k_num);

void choose_medoid(linked_list* cluster, int* medoids, int k_num);

int medoids_comp(int* medoids1, int* medoids2, int k_num);

void clustering_iteration(linked_list* clusters, float** input_data, int data_num, int feature_num, int k_num, int* prev_medoids, int* cur_medoids);

/* print the list. for the test. */

void print_all(linked_list list, float** input_data, int feature_num);

int main(void) {
	FILE* input_file;
	FILE* output_file;
	int data_num, feature_num, k_num, random_index;
	float** input_data;
	linked_list* clusters;
	linked_list cur_list;
	int* prev_medoids, *cur_medoids;
	node* cur;
	int i, j, k, count;

	/* read the input file */

	input_file = fopen("clustering_input.txt", "r");
	fscanf(input_file, "%d %d %d", &data_num, &feature_num, &k_num);

	input_data = (float**)malloc(sizeof(float*) * data_num);
	for (i = 0; i < data_num; i++) {
		input_data[i] = (float*)malloc(sizeof(float) * feature_num);
	}

	for (i = 0; i < data_num; i++) {
		for (j = 0; j < feature_num; j++) {
			fscanf(input_file, "%f", &input_data[i][j]);
		}
	}

	/* initialize the clusters */

	clusters = (linked_list*)malloc(sizeof(linked_list) * k_num);
	/* k clusters. each head of the list contains the center of the cluster */
	for (i = 0; i < k_num; i++) {
		init_list(&clusters[i]);
	}

	srand(1000);
	for (i = 0; i < k_num; i++) {
		random_index = rand() % data_num;
		/* random index below data_num */
		push_center(&clusters[i], random_index);
	}

	prev_medoids = (int*)malloc(sizeof(int) * k_num);
	cur_medoids = (int*)malloc(sizeof(int) * k_num);

	clustering_iteration(clusters, input_data, data_num, feature_num, k_num, prev_medoids, cur_medoids);

	output_file = fopen("clustering_output.txt", "w");

	/* make the output file */

	/*for (i = 0; i < k_num; i++) {
		count = 0;
		fprintf(output_file, "%d\n", i);
		for (cur = clusters[i].header->next->next; cur->next != NULL; cur = cur->next) {
			count++;
			for (j = 0; j < feature_num; j++) {
				fprintf(output_file, "%f ", input_data[cur->data][j]);
			}
			fprintf(output_file, "\n");
		}
		fprintf(output_file, "%d\n", count);
	}*/

	for (i = 0; i < k_num; i++) {
		/*count = 0;*/
		printf("%d\n", i);
		for (cur = clusters[i].header->next->next; cur->next != NULL; cur = cur->next) {
			count++;
			for (j = 0; j < feature_num; j++) {
				printf("%f ", input_data[cur->data][j]);
			}
			printf("\n");
		}
		/*printf("%d\n", count);*/
	}


	return 0;
}


/* linked list functions */

void init_list(linked_list* l) {
	l->header = (node*)malloc(sizeof(node));
	l->trailer = (node*)malloc(sizeof(node));
	l->header->prev = NULL;
	l->header->next = l->trailer;
	l->trailer->prev = l->header;
	l->trailer->next = NULL;
}

void insert_between(linked_list* list, node* pre, node* suc, int new_data) {
	node* new_node = (node*)malloc(sizeof(node));
	new_node->prev = pre;
	new_node->next = suc;
	new_node->data = new_data;
	pre->next = new_node;
	suc->prev = new_node;
}

void push_center(linked_list* list, int new_data) {
	insert_between(list, list->header, list->header->next, new_data);
}

void push_data(linked_list* list, int new_data) {
	insert_between(list, list->trailer->prev, list->trailer, new_data);
}


/* k-medoids clustering functions */

int cluster_center(linked_list list) {
	return list.header->next->data;
}


void allocate_to_cluster(linked_list* clusters, float** input_data, int data_num, int feature_num, int k_num) {
	int center_idx;
	int i, j, k;
	float dist, temp_dist;

	for (i = 0; i < data_num; i++) {
		// allocate the i_th data to some cluster
		center_idx = 0;
		dist = 0;
		for (k = 0; k < feature_num; k++) {
			dist += (input_data[i][k] - input_data[cluster_center(clusters[center_idx])][k]) * (input_data[i][k] - input_data[cluster_center(clusters[center_idx])][k]);
		}
		dist = sqrt(dist);

		for (j = 1; j < k_num; j++) {
			/* finding the nearest cluster center */
			temp_dist = 0;
			for (k = 0; k < feature_num; k++) {
				temp_dist += (input_data[i][k] - input_data[cluster_center(clusters[j])][k]) * (input_data[i][k] - input_data[cluster_center(clusters[j])][k]);
			}
			temp_dist = sqrt(temp_dist);

			if (temp_dist < dist) {
				dist = temp_dist;
				center_idx = j;
			}
		}
		/* allocate data */
		push_data(&clusters[center_idx], i);

	}
}


int make_new_medoid(linked_list* list, float** input_data, int feature_num) {
	/* take the list and update its medoids using the data of the list */
	int i, j, k;
	node* cur_list_start, * cur;
	node* cur_medoid, * new_medoid_candidate, * new_medoid;
	float dist_sum, dist, temp_dist_sum;

	cur_medoid = list->header->next;
	cur_list_start = cur_medoid->next;
	new_medoid = cur_medoid;
	cur = cur_list_start;
	dist_sum = 0;

	while (cur->next != NULL) {
		/* sum of the distance from the current medoid */
		dist = 0;
		for (i = 0; i < feature_num; i++) {
			dist += (input_data[cur_medoid->data][i] - input_data[cur->data][i]) * (input_data[cur_medoid->data][i] - input_data[cur->data][i]);
		}
		dist = sqrt(dist);
		dist_sum += dist;
		cur = cur->next;
	}

	new_medoid_candidate = cur_medoid->next;

	while (new_medoid_candidate->next != NULL) {
		/* check if there exists the better medoid */
		cur = cur_list_start;
		temp_dist_sum = 0;
		while (cur->next != NULL) {
			dist = 0;
			for (i = 0; i < feature_num; i++) {
				dist += (input_data[new_medoid_candidate->data][i] - input_data[cur->data][i]) * (input_data[new_medoid_candidate->data][i] - input_data[cur->data][i]);
			}
			dist = sqrt(dist);
			temp_dist_sum += dist;
			cur = cur->next;
		}

		if (temp_dist_sum < dist_sum) {
			dist_sum = temp_dist_sum;
			new_medoid = new_medoid_candidate;
		}

		new_medoid_candidate = new_medoid_candidate->next;
	}

	return new_medoid->data;

}

void clear_cluster(linked_list* cluster, int k_num) {
	/* clear the cluster except its medoid */
	int i;
	node* cur, * next;
	for (i = 0; i < k_num; i++) {
		cur = cluster[i].header->next->next;
		while (cur->next != NULL) {
			next = cur->next;
			free(cur);
			cur = next;
		}
		cluster[i].header->next->next = cluster[i].trailer;
		cluster[i].trailer->prev = cluster[i].header->next;
	}
}

void copy_array(int* copy_array, int* original_array, int k_num) {
	int i;
	for (i = 0; i < k_num; i++) {
		copy_array[i] = original_array[i];
	}
}

void choose_medoid(linked_list* cluster, int* medoids, int k_num) {
	/* choose the medoid from the cluster and save it */
	int i;
	for (i = 0; i < k_num; i++) {
		medoids[i] = cluster[i].header->next->data;
	}
}

int array_comp(int* arr1, int* arr2, int k_num) {
	/* return 0 if two array are different. otherwise return 1*/
	int i;
	for (i = 0; i < k_num; i++) {
		if (arr1[i] != arr2[i]) { return 0; }
	}
	return 1;
}


void clustering_iteration(linked_list* clusters, float** input_data, int data_num, int feature_num, int k_num, int* prev_medoids, int* cur_medoids) {
	int i;

	allocate_to_cluster(clusters, input_data, data_num, feature_num, k_num);
	choose_medoid(clusters, prev_medoids, k_num);

	for (i = 0; i < k_num; i++) {
		clusters[i].header->next->data = make_new_medoid(&clusters[i], input_data, feature_num);
	}

	clear_cluster(clusters, k_num);
	allocate_to_cluster(clusters, input_data, data_num, feature_num, k_num);
	choose_medoid(clusters, cur_medoids, k_num);


	while (array_comp(prev_medoids, cur_medoids, k_num) == 0) {
		/* while the cluster changes */
		for (i = 0; i < k_num; i++) {
			clusters[i].header->next->data = make_new_medoid(&clusters[i], input_data, feature_num);
		}
		copy_array(prev_medoids, cur_medoids, k_num);
		/* save the previous medoid */
		clear_cluster(clusters, k_num);
		allocate_to_cluster(clusters, input_data, data_num, feature_num, k_num);
		choose_medoid(clusters, cur_medoids, k_num);
	}
}

/* print the list. for the test. */

void print_all(linked_list list, float** input_data, int feature_num) {
	int cnt = 0, i;
	node* temp;
	if (list.header->next == list.trailer) { printf("fail\n"); }
	for (temp = list.header->next->next; temp->next != NULL; temp = temp->next) {
		cnt++;
		for (i = 0; i < feature_num; i++) {
			printf("%f ", input_data[temp->data][i]);
		}printf("\n");
	}
	printf("%d\n", cnt);
}