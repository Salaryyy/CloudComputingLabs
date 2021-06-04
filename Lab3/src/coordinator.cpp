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
#include "conf.h"
using namespace std;


// 包头结构
enum Type {heart, data};
struct packet_head
{
    Type type;
    int length;
};

void *response_heart(void *argv);

class coordinator{
private:
    unsigned int myip;
    unsigned short myport;
    int n_participant;
    int listenfd;
    vector<unsigned int> ip_participant;
    vector<unsigned short> port_participant;
    vector<int> fd_participant;
    vector<int> count_heart;
    Conf myconf;
    struct sockaddr_in servaddr;


public:
    coordinator(Conf conf){
        // 在这里读配置文件将ip和port设置好
        myconf = conf;
        char ip[32];
        strcpy(ip,conf.coorIp.c_str());
        myip = ip_int(ip);
        myport = conf.coorPort; 
        n_participant = conf.partNum;
        ip_participant.resize(n_participant);
        port_participant.resize(n_participant);
        fd_participant.resize(n_participant);
        for(int i=0;i<n_participant;i++)
        {
            strcpy(ip,conf.coorIp.c_str());
            ip_participant[i]=ip_int(ip);
            port_participant[i]=conf.part[i].port;
            fd_participant[i] = -1;
        }
        
        count_heart.resize(3);
        count_heart[0] = 0, count_heart[1] = 0, count_heart[2] = 0;
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
            cout<<"等待连接中"<<endl;
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
            
            if(cur_participant == n_participant){
                cout<<"所有参与者都连接成功"<<endl;
                break;
            }
        }
    }

    void receive_participant(){
        for(int i = 0; i < n_participant; ++i){
            packet_head head;
            int ret;
            ret = recv(fd_participant[i], &head, sizeof(head), 0);   // 先接受包头
            if(ret != sizeof(head)){ // 要是只读了一段没读完咋办 这里默认是没有数据了
                continue;
            }
            // 心跳包
            if(head.type == heart){
                count_heart[i] = 0;        // 每次收到心跳包，count置0
                cout<<"收到来自参与者"<<i<<"的心跳包"<<endl;
                // 发回去
                send(fd_participant[i], &head, sizeof(head), 0);
                cout<<"给参与者"<<i<<"回应心跳包"<<endl;
            }
            
            // 数据包
            else{
                // 数据包，通过head.length确认数据包长度 
            }   


        }
    }

    friend void *response_heart(void *argv);

    void run(){
        while(1){
            receive_participant();
            //receive_client();
        }
    }

    
};




int main(int argc, char **argv){
    Conf conf = getConf(getOptConf(argc, argv));
    coordinator coor(conf);
    coor.init_serveraddr();
    coor.connect_participant();
    coor.run();
    // 开个线程接收客户端连接
}