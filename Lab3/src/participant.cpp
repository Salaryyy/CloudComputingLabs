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
    // kv系统中的数据和备份
    unordered_map<string, vector<string>> kv;
    unordered_map<string, vector<string>> kv_record;

    // 双方ip和port
    unsigned int myip;
    unsigned short myport;
    unsigned int ip_coordinator;
    unsigned short port_coordinator;
    // Conf myconf;

    int connectSocket;
    sockaddr_in client;
    sockaddr_in server;

    int version;

public:
    participant(Conf conf){
        // 在这里读配置文件将ip和port设置好
        // myconf = conf;
        char ip[32];
        strcpy(ip, conf.part[0].ip.c_str());
        myip = ip_int(ip);
        myport = conf.part[0].port; 
        strcpy(ip,conf.coorIp.c_str());
        ip_coordinator = ip_int(ip);
        port_coordinator = conf.coorPort;
        //cout<<myip<<" "<<myport<<" "<<ip_coordinator<<" "<<port_coordinator<<endl;
        version = 0;
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
        // cout<<"新建socket "<<connectSocket<<endl;
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
            //cout<<"连接协调者失败， 继续尝试"<<endl;
            usleep(1000);
        }
        write(connectSocket, &version, sizeof(version));
        // cout<<"myversion"<<version<<endl;
        cout<<"连接成功 "<<endl;
    }

    // 会受到对方的ip和port
    void recover_sendd(){
        unsigned int ip; 
        unsigned short port;
        read(connectSocket, &ip, sizeof(ip));
        read(connectSocket, &port, sizeof(port));
        cout<<"对方ip和port分别是"<<ip<<" "<<port<<endl;

        cout<<"主动方恢复开始"<<endl;
        int ret;
        sockaddr_in newcome;
        newcome.sin_family = AF_INET;
        newcome.sin_addr.s_addr = htonl(ip);
        newcome.sin_port = htons(port);
        int conn = socket(AF_INET, SOCK_STREAM, 0);
        while(connect(conn, (struct sockaddr*)&newcome, sizeof(newcome)) != 0){
            cout<<"连接待恢复参与者失败，继续尝试"<<endl;
            sleep(1);
        }
        cout<<"连接待恢复参与者成功"<<endl;
        //发送kv中的内容
        Order order;
        packet_head head;
        for(auto i : kv){
            order.key = i.first;
            order.value = i.second;
            string temp =  setOrder_recover(order);
            head.length = temp.size();
            
            write_head(conn, &head);
            ret = writen(conn, temp);
            // cout<<"key: "<<order.key<<endl;
            // cout<<"数据包长"<<head.length<<endl;
            // cout<<temp<<endl;
            // if(ret == 0){
            //     cout<<"发送恢复内容失败"<<endl;
            //     // 给协调者发个失败消息 测试脚本在这里不会把协调者kill
            // }
        }
        // sleep(10);
        close(conn);
    }

    void recover_recvv(){
        cout<<"接收方恢复开始"<<endl;
        int ret;
        struct sockaddr_in servaddr;
        int listenfd = socket(AF_INET, SOCK_STREAM, 0);
        bzero(&servaddr, sizeof(servaddr));                                   //地址结构清零
        servaddr.sin_family = AF_INET;
        servaddr.sin_addr.s_addr = htonl(myip);                               //指定本地IP  INADDR_ANY自动
        servaddr.sin_port = htons(myport);                                    //指定端口号 
        int opt = 1;                                                          //设置端口重用
        setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));   
        ret = bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)); //绑定
        if(ret == -1){
            cout<<"Server Bind Failed!"<<endl;
            exit(1);
        }
        listen(listenfd, 1000);

        packet_head head;
        string data;
        Order order;
        struct sockaddr_in cliaddr;
        socklen_t cliaddr_len = sizeof(cliaddr);
        int conn = accept(listenfd, (struct sockaddr *)&cliaddr, &cliaddr_len);  
        while(1){
            ret = read_head(conn, &head);
            if(ret == 0){
                cout<<"对方断开连接，应该是消息传完了"<<endl;
                break;
            }
            // cout<<"收到一个包头"<<head.length<<endl;
            ret = readn(conn, data, head.length);
            // cout<<"收到恢复数据"<<data<<endl;
            order = getOrder(data);
            // cout<<"收到恢复数据"<<order.key<<" "<<order.value[0]<<" "<<order.value[1]<<endl;
            kv.insert({order.key, order.value});
        }
        cout<<"数据恢复完成"<<endl;
    }   

    void receive_coordinator(){
        int ret;
        packet_head head;

        while(1){
            ret = read_head(connectSocket, &head);
            if(ret == 0){
                cout<<"协调者 断开连接"<<endl;
                close(connectSocket);
                connect_coordinator(); 
                continue;
            }
            //cout<<"收到包头"<<head.length<<endl;
            if(head.type == data){
                string message;
                // cout<<"开始接收"<<endl;
                ret = readn(connectSocket, message, head.length);
                // cout<<"收到数据包: "<<message<<endl;
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
                    head.length = respose.size();
                    write_head(connectSocket, &head);
                    writen(connectSocket, respose);
                    // 判断ret。交给下次read的时候判断
                }
                // 两阶段提交
                else if(order.op == SET || order.op == DEL){
                    ++version; // 修改时版本号+1
                    //cout<<"修改指令"<<endl;
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
                    // 如果对面断了 停止两阶段 回滚 其实也可以先不回滚 等待commit包超时再回滚
                    
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
            }
            else if(head.type == recover_recv){
                recover_recvv();
            }
            else if(head.type == recover_send){
                recover_sendd();
            }
        }
    }

    void run(){
        // kv.insert({"CS06142", {"Cloud", "Computing"}});
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