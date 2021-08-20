#include <stdio.h>
#include <stdlib.h>
#include <math.h>

int int_max(int a, int b)
{
    return (a > b ? a : b);
}

int float_max(float a, float b)
{
    return (a > b ? a : b);
}

int weighted_knn(float **data_list, int data_num, int feature_num, int *data_class_list, float *test_data, int class_num)
{
    /* test_data 를 받아서 data_list와 비교하여 weighted knn 으로 분류한 class를 리턴 */
    int near_K = 5;
    float cur_min_dist = -1;
    float temp_min_dist = 0;
    int temp_min_dist_class = 0;
    float temp_dist;
    int i, j, k;
    float *dist_list;
    float *min_dist_list;
    int *min_dist_class_list;
    float *class_weight_list;
    int min_dist_class;

    /* test_data와 i번째 data 간의 L2 distance가 dist[i] */
    dist_list = malloc(sizeof(**data_list) * data_num);
    /* near_K 개의 가장 가까운 점들과의 거리와 그 클래스 */
    min_dist_list = malloc(sizeof(**data_list) * near_K);
    min_dist_class_list = malloc(sizeof(*data_class_list) * near_K);
    /* class 별로 가중치가 어떻게 되는지*/
    class_weight_list = malloc(sizeof(class_num) * class_num);

    for (i = 0; i < data_num; i++)
    {
        /* test_data와 data_list[i] 번 데이터 간의 거리 계산 */
        temp_dist = 0;
        for (j = 0; j < feature_num; j++)
        {
            temp_dist += (test_data[j] - data_list[i][j]) * (test_data[j] - data_list[i][j]);
        }
        temp_dist = sqrt(temp_dist);
        dist_list[i] = temp_dist;
    }

    for (k = 0; k < near_K; k++)
    {
        /* 처음엔 0번째 데이터가 가장 가까운 점*/
        temp_min_dist = -1;
        for (i = 0; i < data_num; i++)
        {
            if (temp_min_dist == -1)
            {
                temp_min_dist = dist_list[i];
                temp_min_dist_class = data_class_list[i];
            }
            else
            {
                /* test data와 거리가 더 작은 쪽으로 간다. 그러나 현재의 최소 거리보단 커야 한다 */
                if (dist_list[i] < temp_min_dist && dist_list[i] > cur_min_dist)
                {
                    temp_min_dist = dist_list[i];
                    temp_min_dist_class = data_class_list[i];
                }
            }
        }
        min_dist_list[k] = temp_min_dist;
        min_dist_class_list[k] = temp_min_dist_class;
        cur_min_dist = temp_min_dist;
    }

    for (i = 0; i < near_K; i++)
    {
        min_dist_list[i] = 1.0 / min_dist_list[i];
    }

    for (i = 0; i < near_K; i++)
    {
        class_weight_list[min_dist_class_list[i]] += min_dist_list[i];
    }

    /* 처음엔 당연히 0이 가장 가중치 작은 클래스 */
    min_dist_class = 0;
    for (i = 0; i < class_num; i++)
    {
        if (class_weight_list[i] > class_weight_list[min_dist_class])
        {
            min_dist_class = i;
        }
    }

    return min_dist_class;
}

int main(void)
{
    FILE *data_file, *test_data_file, *output_file;
    int i, j;
    int data_num, feature_num, class_num = 0;
    int *data_class_list;
    float **data_list;
    int test_data_num;
    float **test_data_list;

    /* data.txt 읽기 */
    data_file = fopen("data.txt", "r");
    fscanf(data_file, "%d %d", &data_num, &feature_num);

    data_list = malloc(sizeof(*data_list) * data_num);
    data_list[0] = malloc(sizeof(**data_list) * data_num * feature_num);
    for (i = 1; i < data_num; i++)
    {
        data_list[i] = data_list[i - 1] + feature_num;
    }
    data_class_list = malloc(sizeof(*data_class_list) * data_num);

    for (i = 0; i < data_num; i++)
    {
        for (j = 0; j < feature_num; j++)
        {
            fscanf(data_file, "%f", &data_list[i][j]);
        }
        fscanf(data_file, "%d", &data_class_list[i]);
        class_num = int_max(class_num, data_class_list[i]);
    }
    class_num++;
    /* 클래스는 총 몇 개인지를 따진다. 0,1,2 클래스가 있다면 클래스 개수 3개 */

    /* test.txt 읽기 */
    test_data_file = fopen("test.txt", "r");
    fscanf(test_data_file, "%d", &test_data_num);
    test_data_list = malloc(sizeof(*test_data_list) * test_data_num);
    test_data_list[0] = malloc(sizeof(**test_data_list) * test_data_num * feature_num);
    for (i = 1; i < test_data_num; i++)
    {
        test_data_list[i] = test_data_list[i - 1] + feature_num;
    }

    for (i = 0; i < test_data_num; i++)
    {
        for (j = 0; j < feature_num; j++)
        {
            fscanf(test_data_file, "%f", &test_data_list[i][j]);
        }
    }

    fclose(data_file);
    fclose(test_data_file);

    output_file = fopen("output.txt", "w");
    for (i = 0; i < test_data_num; i++)
    {
        fprintf(output_file, "%d\n", weighted_knn(data_list, data_num, feature_num, data_class_list, test_data_list[i], class_num));
    }
    fclose(output_file);
    return 0;
}