#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <malloc.h>
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <list>

#include "sudoku.h"
#include "tools.h"

pthread_mutex_t visit_buf = PTHREAD_MUTEX_INITIALIZER;    //缓冲区锁
pthread_mutex_t lock_print = PTHREAD_MUTEX_INITIALIZER;   //打印锁
pthread_mutex_t lock_file = PTHREAD_MUTEX_INITIALIZER;    //文件队列锁
pthread_cond_t empty = PTHREAD_COND_INITIALIZER;          //生产者挂起
pthread_cond_t full = PTHREAD_COND_INITIALIZER;           //消费者挂起
//pthread_cond_t tj = PTHREAD_COND_INITIALIZER;           //文件队列为空时消费者挂起
pthread_cond_t *print_order;                              //控制输出顺序
pthread_t *tid;                                           //解题线程
pthread_t file_thread;                                    //读文件线程

char **buf;             //缓冲区，存放题目
int n_pthread;          //线程数量
int total = 0;          //解决问题总数
int n_data = 0;         //缓冲区剩余题目个数
int use_ptr = 0;        //消费下标
int fill_ptr = 0;       //生产下标
int cur_print = 0;      //当前打印者编号
int finish_num = 0;     //当前打开的文件 线程完成的数量

//bool endfile = false; //是否读到文件尾

std::list<char *> file_list; //文件名队列 

void init()
{
    //获取cpu核心数
    int n = getCpuInfo(); 
    n_pthread = n;

    //缓冲区开辟n行
    buf = (char **)malloc(n * sizeof(char *));
    for (int i = 0; i < n_pthread; ++i)
        buf[i] = (char *)malloc(83);

    //n个条件变量控制输出
    print_order = (pthread_cond_t *)malloc(n * sizeof(pthread_cond_t));

    //n个线程号
    tid = (pthread_t *)malloc(n * sizeof(pthread_t));

    //初始化 输出顺序的条件变量
    for (int i = 0; i < n_pthread; ++i){
        print_order[i] = PTHREAD_COND_INITIALIZER;
    }
}

//释放malloc的空间
void program_end()
{
    free(print_order);
    free(tid);
    for (int i = 0; i < n_pthread; ++i)
        free(buf[i]);
    free(buf);
}

//void put() 生产put写在main里面了

//消费
char *get()
{
    char *tmp = buf[use_ptr];
    use_ptr = (use_ptr + 1) % n_pthread;
    n_data--;
    return tmp;
}

//解题执行函数
void *solver(void *arg)
{
    int board[81];
    while (1)
    {
        pthread_mutex_lock(&visit_buf);
        while (n_data == 0){
            pthread_cond_wait(&full, &visit_buf);
        }

        ++total;                              //当前数据的行数
        //int record_print = total;           //调试用
        int myturn = (total - 1) % n_pthread; //应该的打印顺序

        //读一行题放到board里面
        char *data = get();
        for (int i = 0; i < 81; ++i){
            board[i] = data[i] - '0';
        }

        pthread_cond_signal(&empty);
        pthread_mutex_unlock(&visit_buf);

        //解决问题
        solve_sudoku_dancing_links(board);

        //输出顺序
        pthread_mutex_lock(&lock_print);
        while (myturn != cur_print){    //没有轮到则在自己的条件变量上等待
            pthread_cond_wait(&print_order[myturn], &lock_print);
        }

        //打印到屏幕 注释掉可以省不少时间
        for (int i = 0; i < 81; ++i)
            printf("%d", board[i]);
        printf("\n");

        cur_print = (cur_print + 1) % n_pthread;                     //下一个该打印的编号
        pthread_cond_signal(&print_order[(myturn + 1) % n_pthread]); //唤醒下一个，如果对方在睡的话
        //这里也可以只用一个条件变量，到这里用broadcast唤醒所有其他线程，但是效率可能会低一点，没有尝试-.-
        pthread_mutex_unlock(&lock_print);
    }
}

//处理从屏幕读入的文件名，加入文件队列
void *file_handler(void *argv) 
{
    char tmp[20];
    while(1){
        scanf("%s", tmp);
        pthread_mutex_lock(&lock_file);
        file_list.push_back(tmp);
        pthread_mutex_unlock(&lock_file);
    }
}

int main(int argc, char *argv[])
{
    char filename[20];

    init();

    for (int i = 0; i < n_pthread; ++i){
        pthread_create(&tid[i], NULL, solver, NULL);        //解题
    }

    pthread_create(&file_thread, NULL, file_handler, NULL); //读文件名
    while(1){

        while(file_list.size() == 0);  //这里可以加个cond，算了没啥必要-.-
        //从队列里面获取一个文件名, 反正只有一个生产者, 下来就肯定有文件可读，所以就不加cond了
        pthread_mutex_lock(&lock_file);
        strcpy(filename, file_list.front());
        file_list.pop_front();
        pthread_mutex_unlock(&lock_file);
        
        if(strcmp(filename, "quit") == 0){
            printf("退出\n");
            exit(0);
        }

        if(access(filename, F_OK) == -1){
            printf("文件不存在\n");
            continue;
        }

        FILE *fp = fopen(filename, "r");
        total = 0;           //重新开始计数
        cur_print = 0;       //重新从第0行开始打印

        while (1){ //生产者
            pthread_mutex_lock(&visit_buf);

            while (n_data == n_pthread){    //缓冲区满了
                pthread_cond_wait(&empty, &visit_buf);
            }

            //put函数在这里, 从文件读一行放到缓冲区
            if(fgets(buf[fill_ptr], 83, fp) != NULL){
                fill_ptr = (fill_ptr + 1) % n_pthread;
                ++n_data;
            }
            //读到文件末尾了，算一次无效读入，去取下一个文件名
            else{
                pthread_mutex_unlock(&visit_buf);
                break;
            }
            pthread_cond_signal(&full);
            pthread_mutex_unlock(&visit_buf);
        }
    }

    //等待子线程全部完成 这里其实执行不到
    for (int i = 0; i < n_pthread; ++i){
        pthread_join(tid[i], NULL);
    }

    program_end();
    return 0;
}