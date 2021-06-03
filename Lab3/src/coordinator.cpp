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
#include <iostream>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cstring>
#include "tools.h"
using namespace std;

class coordinator{
private:
    unsigned int myip;
    unsigned short myport;
    int n_participant;
    int listenfd;
    vector<unsigned int> ip_participant;
    vector<unsigned short> port_participant;
    vector<int> fd_participant;
    struct sockaddr_in servaddr;


public:
    coordinator(){
        // 在这里读配置文件将ip和port设置好
        char ip[32] = "127.0.0.1";
        myip = ip_int(ip);
        myport = 8091;
        n_participant = 3;
        ip_participant.resize(3);
        ip_participant[0] = myip, ip_participant[1] = myip, ip_participant[2] = myip;
        port_participant.resize(3);
        port_participant[0] = 8092, port_participant[1] = 8093, port_participant[2] = 8094; 
        fd_participant.resize(3);
        fd_participant[0] = -1, fd_participant[1] = -1, fd_participant[2] = -1;

    }

    void init_serveraddr(){
        int ret;
        listenfd = socket(AF_INET, SOCK_STREAM, 0);                     //创建一个socket, 得到lfd
        if(listenfd < 0){
            cout<<"Create Socket Failed!"<<endl;
            exit(1);
        }
        bzero(&servaddr, sizeof(servaddr));                             //地址结构清零
        servaddr.sin_family = AF_INET;
        servaddr.sin_addr.s_addr = htonl(myip);                         //指定本地IP // INADDR_ANY
        servaddr.sin_port = htons(myport);                              //指定端口号 
        int opt = 1;                                                    //设置端口重用
        setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));   
        ret = bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)); //绑定
        if(ret == -1){
            cout<<"Server Bind Failed!"<<endl;
            exit(1);
        }
        ret = listen(listenfd, 1000);
        if(ret == -1){
            cout<<"Server Listen Failed!"<<endl;
            exit(1);
        }
    }

    void connect_participant(){
        int cur_participant = 0;
        struct sockaddr_in cliaddr;
        socklen_t cliaddr_len = sizeof(cliaddr);
        while(1){
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
                    printf("client port %d  port_participant %d\n", client_port, port_participant[i]);
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
};



int main(){
    coordinator coor;
    coor.init_serveraddr();
    coor.connect_participant();
}