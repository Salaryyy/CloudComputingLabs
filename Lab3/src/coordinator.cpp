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
#include <pthread.h>
#include <sys/socket.h>
#include <unordered_map>
#include <unordered_set>
#include <cstring>
#include "tools.h"
using namespace std;



int main(){
    // 知道自己的ip和端口
    char ip[32] = "127.0.0.1";
    int myip = ip_int(ip);
    int myport = 8091;
    // 知道所有参与者的ip和端口
    int n_participant = 3;
    unsigned ip_participant[3] = {ip_int(ip), ip_int(ip), ip_int(ip)};
    int port_participant[3] = {8092, 8093, 8094};
    int fd_participant[3] = {-1, -1, -1};

    // 设置好监听套接字
    struct sockaddr_in servaddr, cliaddr;
    socklen_t cliaddr_len;
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);                 //创建一个socket, 得到lfd
    bzero(&servaddr, sizeof(servaddr));                             //地址结构清零
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(myip);                         //指定本地IP // INADDR_ANY
    servaddr.sin_port = htons(myport);                              //指定端口号 
    int opt = 1;                                                    //设置端口重用
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));   
    bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)); //绑定
    listen(listenfd, 1000);

    int cur_participant = 0;
    while(1){
        cliaddr_len = sizeof(cliaddr);
        //阻塞监听链接请求 这个连接可能来自参与者 可能来自客户端 需要判断一下
        printf("等待连接中\n");
        int connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &cliaddr_len);   
        
        //看一下客户端信息
        char client_ip[1024];
        int  client_port = ntohs(cliaddr.sin_port);
        inet_ntop(AF_INET, &cliaddr.sin_addr, client_ip, sizeof(client_ip));
        printf("received from %s at PORT %d\n", client_ip, client_port);
        printf("新连接cfd: %d\n", connfd);

        // 检查是否是参与者
        for(int i = 0; i < n_participant; ++i){
            if(ip_int(client_ip) == ip_participant[i] && client_port == port_participant[i]){
                printf("this is participant %d\n", i);
                // 如果之前没有建立过连接 计数+1
                if(fd_participant[i] == -1){
                    ++cur_participant;
                }
                // 如果之前有建立过连接 把原来的socket关了
                else{
                    printf("close socket %d\n", fd_participant[i]);
                    close(fd_participant[i]);
                }
                fd_participant[i] = connfd;
                break;
            }
            else{
                printf("client ip %d  ip_participant %d\n", ip_int(client_ip), ip_participant[i]);
                if(i == n_participant - 1){
                    printf("close this connection\n");
                    close(connfd);
                }
            }
        }
        
        // 所有参与者都连上了 退出循环
        if(cur_participant == n_participant)
            break;
    }



}