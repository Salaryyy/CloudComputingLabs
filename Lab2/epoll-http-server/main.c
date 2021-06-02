#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "epoll_server.h"

int main(int argc, const char* argv[])
{
    if(argc < 4)
    {
        printf("eg: ./a.out port path num_thread\n");
        exit(1);
    }

    // 采用指定的端口
    int port = atoi(argv[1]);
    int n_thread = atoi(argv[3]);

    // 修改进程工作目录, 方便后续操作
    int ret = chdir(argv[2]);
    if(ret == -1)
    {
        perror("chdir error");
        exit(1);
    }
    
    // 启动epoll模型 
    epoll_run(port, n_thread);

    return 0;
}
