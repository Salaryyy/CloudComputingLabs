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
using namespace std;

// 包头结构
enum Type {heart, data};
struct packet_head
{
    Type type;
    int length;
};

// 这个一定要放在前面 不然成员函数找不到
void* send_heart(void* argv);

class participant{
private:
    unsigned int myip;
    unsigned short myport;
    unsigned int ip_coordinator;
    unsigned short port_coordinator;

    int connectSocket;
    sockaddr_in client;
    sockaddr_in server;
    pthread_mutex_t write_lock;
    pthread_mutex_t heart_lock;
    //pthread_cond_t connecting;
    // 记录协调者没有返回的心跳个数
    int heart_count;
    bool recv_heart;
    //bool forbidden_recv;

public:
    participant(unsigned short port){
        // 在这里读配置文件将ip和port设置好
        char ip[32] = "127.0.0.1";
        myip = ip_int(ip);
        myport = port;
        ip_coordinator = myip;
        port_coordinator = 8095;

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
        cout<<"新建"<<connectSocket<<endl;
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
        cout<<"连接成功，现在把他关掉"<<endl;
        // close(connectSocket);
        // goto begin;
    }

    // 接受包不应该区分 因为不确定收到的是数据包还是心跳包
    void receive(){
        //尝试获取一下心跳锁，因为有可能心跳线程正在重连协调者，这个时候应该阻塞等待
        pthread_mutex_lock(&heart_lock);
        pthread_mutex_unlock(&heart_lock);
        //sleep(1);
        packet_head head;// 先接受包头
        //这里得设置一个超时 因为此时也可能断开连接 发生超时后跳转到开头
        
        //尝试太多次失败后会直接被操作系统kill
        //cout<<"准备recv"<<endl;
        int ret = recv(connectSocket, &head, sizeof(head), MSG_DONTWAIT);//？？MSG_DONTWAIT
        if(ret != sizeof(head)){//?
            //cout<<"长度只有"<<ret<<endl;
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
        else{
            //调用处理数据包的函数解析并发送数据
        }
    }

    // 声明为普通的友元函数? 供子线程去调用
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


void* send_heart(void* argv){
    // 发心跳包的时候得加个锁 避免和普通数据同时写
    participant *p = (participant*)argv;
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
        //cout<<"发送heart方现在持有heart"<<endl;
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
                p->connect_coordinator(); // !!! 重新建连的时候不能读数据
            }
        }
        //cout<<"发送heart方现在释放heart"<<endl;
        pthread_mutex_unlock(&(p->heart_lock));
    }
}


int main(int argc, char **argv){
    participant part(atoi(argv[1]));
    part.init_addr();
    part.connect_coordinator();
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