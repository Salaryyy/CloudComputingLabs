#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>
#include <sys/socket.h>
#include <pthread.h>

int main(int argc, char **argv){
    int connectSocket = socket(AF_INET, SOCK_STREAM, 0);
    if(connectSocket == -1){
        perror("socket error");
    }
    // 协调者信息
    sockaddr_in coordinator; 
    coordinator.sin_family = AF_INET;
    coordinator.sin_addr.s_addr = inet_addr("127.0.0.1");
    coordinator.sin_port = htons(8091);
 
    // 绑定自己信息
    sockaddr_in participant;
    participant.sin_family = AF_INET;
    participant.sin_addr.s_addr = inet_addr("127.0.0.1");//
    participant.sin_port = htons(atoi(argv[1]));
    int opt = 1;                                                    //设置端口重用
    setsockopt(connectSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)); 
    if(bind(connectSocket, (struct sockaddr *) &participant, sizeof(participant)) == SO_ERROR){
        printf("bind() failed.\n");
        close(connectSocket);
        return 1;
    }

    
    while(connect(connectSocket, (struct sockaddr*)&coordinator, sizeof(coordinator)) != 0){
        printf("连接协调者失败， 继续尝试\n");
        sleep(1);
    }


    while(1){
        sleep(1);
        printf("xinglai\n");
    }
    // int i = 10;
    // char* buf[BUFSIZ];
    // while(--i){
    //     write(cfd, "hello\n", 6);
    //     ret = read(cfd, buf, sizeof(buf));
    //     write(STDOUT_FILENO, buf, ret);
    //     sleep(1);
    // }
}