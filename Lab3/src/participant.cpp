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
#include <signal.h>
#include <sys/time.h>
#include <pthread.h>
#include <iostream>
#include <unordered_map>
#include "tools.h"
#include "conf.h"
using namespace std;

// 包头结构
enum Type {heart, data, done, commit, rollback};
struct packet_head
{
    Type type;
    int length;
};

// 这个一定要放在前面 不然成员函数找不到
void* send_heart(void* argv);

class participant{
private:
    unordered_map<string, vector<string>> kv;

    unsigned int myip;
    unsigned short myport;
    unsigned int ip_coordinator;
    unsigned short port_coordinator;
    Conf myconf;

    int connectSocket;
    sockaddr_in client;
    sockaddr_in server;

    pthread_mutex_t write_lock;
    pthread_mutex_t heart_lock;
    //pthread_cond_t connecting;
    // 记录协调者没有返回的心跳个数
    int  heart_count;
    bool recv_heart;
    //bool forbidden_recv;

public:
    participant(Conf conf){
        // 在这里读配置文件将ip和port设置好
        myconf = conf;
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

        write_lock = PTHREAD_MUTEX_INITIALIZER;
        heart_lock = PTHREAD_MUTEX_INITIALIZER;
        //connecting = PTHREAD_COND_INITIALIZER;

        heart_count = 0;
        recv_heart = false;
        //forbidden_recv = false;
    }

    void init_addr(){
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
        
        //begin:
        connectSocket = socket(AF_INET, SOCK_STREAM, 0);
        cout<<"新建socket"<<connectSocket<<endl;
        // 设置端口重用
        int opt = 1;                                                    
        setsockopt(connectSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)); 
        // 如果不bind 系统自动分配端口号
        if(bind(connectSocket, (struct sockaddr *) &client, sizeof(client)) == SO_ERROR){
            cout<<"bind() failed."<<endl;
            close(connectSocket);
            exit(1);
        }
        while(connect(connectSocket, (struct sockaddr*)&server, sizeof(server)) != 0){
            cout<<"连接协调者失败， 继续尝试"<<endl;
            sleep(1);
        }
        cout<<"连接成功"<<endl;
        // close(connectSocket);
        // goto begin;
    }

    // 接受包不应该区分 因为不确定收到的是数据包还是心跳包
    void receive(){
        //尝试获取一下心跳锁，因为有可能心跳线程正在重连协调者，这个时候应该阻塞等待
        pthread_mutex_lock(&heart_lock);
        pthread_mutex_unlock(&heart_lock);
        packet_head head;// 先接受包头
        //这里得设置一个超时 因为此时也可能断开连接 发生超时后跳转到开头 
        //这个recv flag参数得好好研究下
        int ret = recv(connectSocket, &head, sizeof(head), MSG_DONTWAIT);// !!!!!!MSG_DONTWAIT不要改这里了
        if(ret != sizeof(head)){//?
            // cout<<"长度只有"<<ret<<endl;
            return;
        }
        // 接收到心跳回应包
        if(head.type == heart){
            cout<<"收到来自协调者的心跳回应包"<<endl;
            pthread_mutex_lock(&heart_lock);
            heart_count = 0;       // 每次收到心跳包，count置0
            recv_heart = true;
            pthread_mutex_unlock(&heart_lock);
        }

        // 接收到数据包
        else if(head.type == data){
            string message;
            readn(connectSocket, message);
            Order order = getOrder(message);//单独测一下
            if(order.op == NIL)
                return; //如果要开线程的话 这里改成continue
            else if(order.op == GET){
                if(kv.find(order.key) != kv.end()){
                    order.value = kv[order.key];
                }
                // 查一下发过去  这个函数有个bug
                string respose = setOrder(order); 
                packet_head head;
                head.type = data;
                head.length = 0;  //不需要了
                // 加锁发送数据包头 和 数据包体
                pthread_mutex_lock(&write_lock);
                send(connectSocket, &head, sizeof(head), 0);
                writen(connectSocket, respose);
                pthread_mutex_unlock(&write_lock);
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

    // 声明为普通的友元函数 供子线程去调用 在类函数里面开线程不能直接调用类里面的函数
    friend void* send_heart(void* argv);

    void run(){
        pthread_t tid;
        // this指针传进去
        pthread_create(&tid, NULL, send_heart, (void*)this);
        while(1){
            receive();
        }
    }
};

// 定期发送心跳包
void* send_heart(void* argv){
    // 发心跳包的时候得加个锁 避免和普通数据同时写
    participant *p = (participant *)argv;
    while(1){
        // 一秒钟发一个心跳包
        sleep(1);
        packet_head head;
        head.type = heart;
        head.length = 0;  
        // 加锁发送心跳包
        pthread_mutex_lock(&(p->write_lock));
        send(p->connectSocket, &head, sizeof(head), 0); //发送的坑。可能这里会有错
        pthread_mutex_unlock(&(p->write_lock));
        cout<<"心跳包已发送，等待回应"<<endl;

        // 再等一秒钟 检测是不是收到了包
        sleep(1);
        pthread_mutex_lock(&(p->heart_lock));
        if(p->recv_heart){
            p->recv_heart = false;
        }
        else{
            ++(p->heart_count); //没有收到的回应数+1
            if(p->heart_count == 5){ // 连续2个心跳包没收到
                //首先停止接收数据   然后重新建立连接
                p->heart_count = 0;
                cout<<"超时， 关闭"<<p->connectSocket<<endl;
                close(p->connectSocket);
                // ????设置一个标志信息 让其他线程停止不做了
                p->connect_coordinator(); // !!! 重新建连的时候不能读数据
            }
        }
        pthread_mutex_unlock(&(p->heart_lock));
    }
}


int main(int argc, char **argv){
    Conf conf = getConf(getOptConf(argc, argv));
    participant part(conf);
    part.init_addr();
    part.connect_coordinator();
    part.handle_for_sigpipe();
    part.run();

    // 心跳机制

    while(1){
        sleep(1);
        cout<<"连接中."<<endl;
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