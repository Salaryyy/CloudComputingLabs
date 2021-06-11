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
#include <sstream>
#include "tools.h"
#include "conf.h"
using namespace std;



class participant{
private:
    unordered_map<string, vector<string>> kv;
    unordered_map<string, vector<string>> kv_record;

    unsigned int myip;
    unsigned short myport;
    unsigned int ip_coordinator;
    unsigned short port_coordinator;
    Conf myconf;

    int connectSocket;
    sockaddr_in client;
    sockaddr_in server;

    //pthread_mutex_t write_lock;
    //pthread_mutex_t heart_lock;
    //pthread_cond_t connecting;
    // int  heart_count;
    // bool recv_heart;
    //bool forbidden_recv;

public:
    participant(Conf conf){
        // 在这里读配置文件将ip和port设置好
        myconf = conf;
        char ip[32];
        strcpy(ip,conf.part[0].ip.c_str());
        myip = ip_int(ip);
        myport = conf.part[0].port; 
        strcpy(ip,conf.coorIp.c_str());
        ip_coordinator = ip_int(ip);
        port_coordinator = conf.coorPort;
        //cout<<myip<<" "<<myport<<" "<<ip_coordinator<<" "<<port_coordinator<<endl;

        //write_lock = PTHREAD_MUTEX_INITIALIZER;
        //heart_lock = PTHREAD_MUTEX_INITIALIZER;
        //connecting = PTHREAD_COND_INITIALIZER;
        //heart_count = 0;
        //recv_heart = false;
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
        connectSocket = socket(AF_INET, SOCK_STREAM, 0);
        cout<<"新建socket "<<connectSocket<<endl;
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
            sleep(10);
        }
        cout<<"连接成功 "<<endl;
    }

    void receive_coordinator(){
        int ret;
        packet_head head;

        while(1){
            // 这里可以封装一下
            ret = read_head(connectSocket, &head);
            if(ret == 0){
                cout<<"协调者 断开连接"<<endl;
                close(connectSocket);
                connect_coordinator(); 
                continue;
            }
            cout<<"收到包头"<<head.length<<endl;
            if(head.type == data){
                string message;
                cout<<"开始接收"<<endl;
                ret = readn(connectSocket, message, head.length);
                cout<<"收到数据包: "<<message<<endl;
                if(ret == 0){
                    cout<<"协调者 断开连接"<<endl;
                    close(connectSocket);
                    connect_coordinator();
                    continue;
                }
                Order order = getOrder(message);
                if(order.op == NIL){
                    continue;
                }
                else if(order.op == GET){
                    if(kv.find(order.key) != kv.end()){
                        order.value = kv[order.key];
                    }
                    string respose = setOrder(order); 
                    head.type = data;
                    head.length = respose.size();  //不需要了
                    // 不用加锁了没有其他线程
                    //cout<<"回复长度"<<respose.size()<<endl;
                    //pthread_mutex_lock(&write_lock);
                    write_head(connectSocket, &head);
                    writen(connectSocket, respose);
                    // 判断ret。。。。。。交给下次read的时候判断吧
                    //pthread_mutex_unlock(&write_lock);
                }
                // 两阶段提交
                else if(order.op == SET || order.op == DEL){
                    //先完成 然后回复个done
                    cout<<"修改指令"<<endl;
                    kv_record = kv; //备个份
                    string respose;
                    if(order.op == SET){
                        kv[order.key] = order.value;
                        respose = "+OK\r\n";
                    }
                    else if(order.op == DEL){
                        int ndel = 0;
                        for(int i = 0; i < order.value.size(); ++i){
                            if(kv.find(order.value[i]) != kv.end()){
                                kv.erase(order.value[i]);
                                ++ndel;
                            }
                        }
                        respose = ":" + to_string(ndel) + "\r\n";
                    }
                    head.type = done;
                    head.length = respose.size();

                    write_head(connectSocket, &head);
                    ret = writen(connectSocket, respose);
                    // //如果对面断了 停止两阶段 回滚停止两阶段 其实也可以先不回滚 等待commit包超时再回滚
                    // if(ret == 0)
                    //     continue;
                    cout<<"完成请求 回发done包"<<endl;
                    //接收commit或rollback消息
                    ret = read_head(connectSocket, &head);
                    // 超时或者接收到rollback
                    if(ret == 0 || head.type == rollback){
                        kv = kv_record;
                    }
                    else if(head.type == commit){
                        ;//什么也不做
                    }
                }
                else if(order.op == SYN){//同步 恢复某个数据库

                /*
                     if(kv.find(order.key) != kv.end()){
                        order.value = kv[order.key];
                    }
                    string respose = setOrder(order); 
                    head.type = data;
                    head.length = respose.size();  //不需要了
                    // 不用加锁了没有其他线程
                    //cout<<"回复长度"<<respose.size()<<endl;
                    //pthread_mutex_lock(&write_lock);
                    write_head(connectSocket, &head);
                    writen(connectSocket, respose);
                    // 判断ret。。。。。。交给下次read的时候判断吧
                    //pthread_mutex_unlock(&write_lock);

                */

               
                }

            }
        }
    }

    void run(){
        kv.insert({"CS06142", {"Cloud", "Computing"}});
        receive_coordinator();
    }

};

void handle_for_sigpipe(){
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    if(sigaction(SIGPIPE, &sa, NULL))
        return;
}

int main(int argc, char **argv){
    handle_for_sigpipe();
    Conf conf = getConf(getOptConf(argc, argv));
    participant part(conf);
    part.init_addr();
    part.connect_coordinator();
    part.run();
}