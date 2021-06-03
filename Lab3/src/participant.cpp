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
#include <iostream>
#include "tools.h"
#include "conf.h"
using namespace std;

class participant{
private:
    unsigned int myip;
    unsigned short myport;
    unsigned int ip_coordinator;
    unsigned short port_coordinator;
    Conf myconf;
    int connectSocket;
    sockaddr_in client; 
    sockaddr_in server;
public:
    participant(Conf conf){
        // 在这里读配置文件将ip和port设置好
        myconf=conf;
        char ip[32];
        strcpy(ip,conf.part[0].ip.c_str());
        myip = ip_int(ip);
        //myport = 8091;
        myport = conf.part[0].port; 
        // myip = ip_int(ip);
        // myport = port;
        strcpy(ip,conf.part[0].ip.c_str());
        ip_coordinator = ip_int(ip);
        port_coordinator = conf.coorPort;
        cout<<myip<<" "<<myport<<" "<<ip_coordinator<<" "<<port_coordinator<<endl;
    }

    void init_addr(){
        connectSocket = socket(AF_INET, SOCK_STREAM, 0);
        // 协调者信息
        server.sin_family = AF_INET;
        server.sin_addr.s_addr = htonl(ip_coordinator); 
        server.sin_port = htons(port_coordinator);

        // 绑定自己信息
        client.sin_family = AF_INET;
        client.sin_addr.s_addr = htonl(myip); 
        client.sin_port = htons(myport);

    }

    void connect_coordinator(){
        int opt = 1;                                                    //设置端口重用
        setsockopt(connectSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)); 
        // 如果不bind 系统自动分配端口号
        if(bind(connectSocket, (struct sockaddr *) &client, sizeof(client)) == SO_ERROR){
            printf("bind() failed.\n");
            close(connectSocket);
            exit(1);
        }

        while(connect(connectSocket, (struct sockaddr*)&server, sizeof(server)) != 0){
            printf("连接协调者失败， 继续尝试\n");
            sleep(1);
        }
    }
};


int main(int argc, char **argv){
    Conf conf=getConf(getOptConf(argc, argv));
    participant part(conf);
    part.init_addr();
    part.connect_coordinator();

    // 心跳机制


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