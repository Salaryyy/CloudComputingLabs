#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <malloc.h>
#include<pthread.h>
#include <sys/time.h>
#include"sudoku.h"

int lines = 0;
int n_pthread;
int max_thread = 100;
char *data[1000];

/*void *child(char **data){
    int i = 1;
    printf("%s", data[0]);
}*/

int64_t now()
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000000 + tv.tv_usec;
}

void *child(void *argv){
    
    int num = *((int*) argv);
    int temp = num;
    int board[81];
    while(num < lines){
        //printf("thread %d sovle %s", temp, data[num]);
        for(int i = 0; i < 81; ++i)
            board[i] = data[num][i] - '0';
        solve_sudoku_dancing_links(board);
        for(int i = 0; i < 81; ++i)
            data[num][i] = board[i] + '0';
        num += n_pthread;
    }
    pthread_exit(NULL);
}


int main(int argc, char *argv[]){
    pthread_t tid[max_thread];
    for(int i = 0; i < 1000; ++i)
        data[i] = (char*)malloc(83);
    FILE *fp = fopen(argv[1], "r");
    for(int i = 0; i < 1000; ++i){
        if(fgets(data[i], 83, fp) == NULL)
            break;
        ++lines;
    }
    fclose(fp);

    n_pthread = atoi(argv[2]);
    if(n_pthread > max_thread || n_pthread <= 0){
        perror("out range of thread!");
        exit(1);
    }
    
    int64_t start = now();

    //int *pint[max_thread];
    int begin[max_thread];
    for(int i = 0; i < n_pthread; ++i){
        begin[i] = i;
        pthread_create(&tid[i], NULL, child, &begin[i]);

    }
    for(int i = 0; i < n_pthread; ++i){
        pthread_join(tid[i], NULL);
    }
    

    fp = fopen("result.txt", "w+");
    for(int i = 0; i < lines; ++i){
        fputs(data[i] ,fp);
        free(data[i]);
    }

    int64_t end = now();
    double sec = (end-start)/1000000.0;
    printf("%f sec %f ms each %d\n", sec, 1000*sec/lines, lines);
    //printf("%d", n_pthread);
    //print(data);
    //print(data + 2);
    //printf("%d", lines);
}


/*int main(){
    FILE *fp = fopen("input.txt", "r");
    char *s = (char*)malloc(83);
    fgets(s, 83, fp);
    fclose(fp);
    int borad[81];
    for(int i = 0; i < 81; ++i){
        borad[i] = s[i] - '0';
    }
    solve_sudoku_dancing_links(borad);
    for(int i = 0; i < 81; ++i){
        s[i] = borad[i] + '0';
    }
    fp = fopen("result.txt", "w+");
    fputs(s, fp);
    fclose(fp);
}*/