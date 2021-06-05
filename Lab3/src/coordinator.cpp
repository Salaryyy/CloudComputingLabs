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
#include <signal.h>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cstring>
#include "tools.h"
#include "conf.h"
using namespace std;


// 包头结构 data包应该还要细分 有的是控制信息包 有的是数据包
enum Type {heart, data};
struct packet_head
{
    Type type;
    int length;
};

void *check_participant(void *argv);

class coordinator{
private:
    unsigned int myip;
    unsigned short myport;
    int n_participant;
    int listenfd;
    vector<unsigned int> ip_participant;
    vector<unsigned short> port_participant;
    vector<int> fd_participant;
    Conf myconf;
    struct sockaddr_in servaddr;

    // 心跳变量
    pthread_mutex_t write_lock;
    pthread_mutex_t heart_lock;
    // 记录没有收到的心跳个数
    vector<int>  count_heart;
    vector<bool> recv_heart;
    vector<bool> shut_down;
    

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
        for(int i = 0; i < n_participant; ++i){
            strcpy(ip,conf.coorIp.c_str());
            ip_participant[i]=ip_int(ip);
            port_participant[i]=conf.part[i].port;
            fd_participant[i] = -1;
        }
        
        write_lock = PTHREAD_MUTEX_INITIALIZER;
        heart_lock = PTHREAD_MUTEX_INITIALIZER;
        count_heart.resize(n_participant);
        recv_heart.resize(n_participant);
        shut_down.resize(n_participant);
        for(int i = 0; i < n_participant; ++i){
            count_heart[i] = 0;
            recv_heart[i] = false;
            shut_down[i] = false;
        }
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

    // 如果尝试send到一个disconnected socket上，就会让底层抛出一个SIGPIPE信号。
    // 这个信号的缺省处理方法是退出进程，大多数时候这都不是我们期望的。
    // 因此我们需要重载这个信号的处理方法。调用以下代码，即可安全的屏蔽SIGPIP
    void handle_for_sigpipe(){
        struct sigaction sa;
        memset(&sa, '\0', sizeof(sa));
        sa.sa_handler = SIG_IGN;
        sa.sa_flags = 0;
        if(sigaction(SIGPIPE, &sa, NULL))
            return;
    }

    void receive_participant(){
        for(int i = 0; i < n_participant; ++i){
            // 关机检测
            pthread_mutex_lock(&heart_lock);
            if(shut_down[i]){
                pthread_mutex_unlock(&heart_lock);
                continue;
            }
            pthread_mutex_unlock(&heart_lock);


            packet_head head;
            int ret;
            ret = recv(fd_participant[i], &head, sizeof(head), 0);   // 先接受包头
            if(ret != sizeof(head)){ // 要是只读了一段没读完咋办 这里默认是没有数据了
                continue;
            }
            // 心跳包
            if(head.type == heart){

                // 这里得加点锁
                pthread_mutex_lock(&heart_lock);
                count_heart[i] = 0;        // 每次收到心跳包，count置0
                recv_heart[i] = true;
                pthread_mutex_unlock(&heart_lock);
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

    void receive_client(){
        ;
    }

    // 定期检查参与者是否活着
    friend void *check_participant(void *argv);

    void run(){
        pthread_t tid;
        pthread_create(&tid, NULL, check_participant, (void *)this);
        while(1){
            receive_participant();
            //receive_client();
        }
    }

    
};

void *check_participant(void *argv){
    coordinator *p = (coordinator *)argv;
    while(1){
        // 每隔一秒钟检查一下
        sleep(1);
        pthread_mutex_lock(&(p->heart_lock));

        for(int i = 0; i < p->n_participant; ++i){
            if(p->recv_heart[i]){
                p->recv_heart[i] = false;
            }
            else{
                ++(p->count_heart[i]);
                if(p->count_heart[i] == 5){
                    p->count_heart[i] = 0;
                    cout<<"参与者"<<i<<"断开连接~~~~~~~~~~~~~~~~~~~~~~~~~~~"<<endl;
                    close(p->fd_participant[i]);
                    // 启动重连机制 要求2永久扔掉  要求3暂时扔掉持续监听恢复请求

                    // 要求2
                    p->shut_down[i] = true;
                }
            }
        }

        pthread_mutex_unlock(&(p->heart_lock));
    }
}




int main(int argc, char **argv){
    Conf conf = getConf(getOptConf(argc, argv));
    coordinator coor(conf);
    coor.init_serveraddr();
    coor.connect_participant();
    coor.handle_for_sigpipe();
    coor.run();
    // 开个线程接收客户端连接
}