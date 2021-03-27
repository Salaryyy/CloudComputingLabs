#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <malloc.h>
#include<pthread.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include"sudoku.h"

const int n_pthread = 4;   // 在这里修改线程数量

pthread_mutex_t visit_buf = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lock_print = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t empty = PTHREAD_COND_INITIALIZER;
pthread_cond_t full = PTHREAD_COND_INITIALIZER;
pthread_cond_t print_order[n_pthread];

int total = 0;
int cur_print = 0;

char *buf[n_pthread];
int fill_ptr = 0;
int use_ptr = 0;
int n_data = 0;
bool end = false;

int64_t now()
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000000 + tv.tv_usec;
}

//void put() put写在main里面了

char* get(){
    char* tmp = buf[use_ptr];
    use_ptr = (use_ptr + 1) % n_pthread;
    n_data--;
    return tmp;
}

void *solver(void *arg)
{
    int board[81];
    while(1){
        pthread_mutex_lock(&visit_buf);
        while(n_data == 0 && !end)     //这里类似于线程池的解法
            pthread_cond_wait(&full, &visit_buf);
        if(n_data == 0 && end){        //文件读到尾了
            pthread_mutex_unlock(&visit_buf); //开锁跑路
            //printf("paolu\n");
            pthread_exit(NULL);
        }
        ++total;
        int record_print = total; //调试用
        int myturn = (total - 1) % n_pthread;
        //printf("消费%d\n", cur_print);
        char *data = get();
        for(int i = 0; i < 81; ++i){
            board[i] = data[i] -  '0';
        }
        pthread_cond_signal(&empty);
        pthread_mutex_unlock(&visit_buf);

        //解决问题
        solve_sudoku_dancing_links(board);


        //这里还有个大问题，输出顺序
        pthread_mutex_lock(&lock_print);
        while(myturn != cur_print){
            //printf("第%d行想打印，但是没轮到我, 在%d号条件变量上等待\n", record_print, myturn);
            pthread_cond_wait(&print_order[myturn], &lock_print);
        }
        
        //打印到屏幕 注释掉可以省不少时间
        printf("现在打印第%d行", record_print);
        for(int i = 0; i < 81; ++i)
            printf("%d", board[i]);
        printf("\n");
        //printf("唤醒%d\n", (myturn + 1) % n_pthread);

        cur_print = (cur_print + 1) % n_pthread;
        pthread_cond_signal(&print_order[(myturn + 1) % n_pthread]);
        pthread_mutex_unlock(&lock_print);

    }
}
 
int main (int argc, char *argv[])
{
    pthread_t tid[n_pthread];
    //开辟缓冲区
    for(int i = 0; i < n_pthread; ++i)
        buf[i] = (char*) malloc(83);
    //初始化 输出顺序 条件变量
    for(int i = 0; i < n_pthread; ++i){
        print_order[i] = PTHREAD_COND_INITIALIZER;
        pthread_create(&tid[i], NULL, solver, NULL);
    }

    FILE *fp = fopen("input.txt", "r");
    
    int64_t start = now();

    while(1){   //生产者
        pthread_mutex_lock(&visit_buf);
        while(n_data == n_pthread){
            //printf("缓冲区满了，生产者释放锁\n");
            pthread_cond_wait(&empty, &visit_buf);
        }
        //生产开始
        if(fgets(buf[fill_ptr], 83, fp) != NULL){    //put函数在这里
            //printf("生产%d\n", total);
            fill_ptr = (fill_ptr + 1) % n_pthread;
            ++n_data;
        }
        else{
            end = true;  //读到文件末尾了
            pthread_mutex_unlock(&visit_buf);
            pthread_cond_broadcast(&full);  //广播唤醒所有消费者 没有数据了
            break;
        }
        pthread_cond_signal(&full);
        pthread_mutex_unlock(&visit_buf);
    }

    //等待子线程全部完成
    for(int i = 0; i < n_pthread; ++i){
        pthread_join(tid[i], NULL);
    }
    
    int64_t end = now();
    double sec = (end-start)/1000000.0;
    printf("%f sec %f ms each %d, \n", sec, 1000*sec/total, total);
    //释放缓冲区
    for(int i = 0; i < n_pthread; ++i)
        free(buf[i]);
    return 0;
} 