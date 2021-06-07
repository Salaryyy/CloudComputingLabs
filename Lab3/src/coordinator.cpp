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
#include <sys/time.h>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <list>
#include <cstring>
#include "tools.h"
#include "conf.h"
using namespace std;


// 包头结构 data包应该还要细分 有的是控制信息包 有的是数据包
enum Type {heart, data, done, commit, rollback};
struct packet_head{
    Type type;
    int length;
};

struct client_ask{
    int fd; // 处理完后记得关闭连接
    string message;//或者拆得更细
};

void *check_participant(void *argv);
void *receive_client(void *argv);
void *hand_out(void *argv);

class coordinator{
private:
    // 配置信息
    unsigned int myip;
    unsigned short myport;
    int n_participant;
    vector<unsigned int> ip_participant;
    vector<unsigned short> port_participant;
    vector<int> fd_participant;
    Conf myconf;
    int listenfd;
    struct sockaddr_in servaddr;

    // 参与者
    vector<pthread_mutex_t> write_lock;
    pthread_mutex_t heart_lock;
    //pthread_mutex_t data_lock;
    // 记录没有收到的心跳个数
    vector<int>  count_heart;
    vector<bool> recv_heart;
    vector<bool> shut_down;
    vector<string> data_return;
    vector<bool> recv_data;
    //int alive;
    
    // 客户端
    list<client_ask> worklist;
    pthread_mutex_t worklist_lock;
    pthread_cond_t worklist_cond;
    

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
        
        //write_lock = PTHREAD_MUTEX_INITIALIZER;
        heart_lock = PTHREAD_MUTEX_INITIALIZER;
        //data_lock = PTHREAD_MUTEX_INITIALIZER;
        write_lock.resize(n_participant);
        count_heart.resize(n_participant);
        recv_heart.resize(n_participant);
        shut_down.resize(n_participant);
        data_return.resize(n_participant);
        recv_data.resize(n_participant);

        for(int i = 0; i < n_participant; ++i){
            count_heart[i] = 0;
            recv_heart[i] = false;
            shut_down[i] = false;
            recv_data[i] = false;
            write_lock[i] = PTHREAD_MUTEX_INITIALIZER;
        }
        //alive = n_participant;

        worklist_lock = PTHREAD_MUTEX_INITIALIZER;
        worklist_cond = PTHREAD_COND_INITIALIZER;
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
            char client_ip[40];
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

                
                pthread_mutex_lock(&heart_lock);
                count_heart[i] = 0;        // 每次收到心跳包，count置0
                recv_heart[i] = true;
                pthread_mutex_unlock(&heart_lock);
                cout<<"收到来自参与者"<<i<<"的心跳包"<<endl;

                // 这里得加锁
                // 发回去
                pthread_mutex_lock(&write_lock[i]);
                send(fd_participant[i], &head, sizeof(head), 0);
                pthread_mutex_unlock(&write_lock[i]);
                cout<<"给参与者"<<i<<"回应心跳包"<<endl;
            }
            
            // 数据包 get的数据  del的个数 都是原封不动地返回 但是del需要完成两阶段后返回
            else if(head.type == data){
                // 数据包，通过head.length确认数据包长度  好像这个参数没必要用了
                //pthread_mutex_lock(&data_lock);
                // 告诉handout线程 我正在读你等会
                ret = readn(fd_participant[i], data_return[i]);
                if(ret > 0){
                    recv_data[i] = true;
                }
                //pthread_mutex_unlock(&data_lock);
            }   


        }
    }

    

    // 定期检查参与者是否活着
    friend void *check_participant(void *argv);

    // 负责监听普通客户端的请求
    friend void *receive_client(void *argv);

    // 分发任务
    friend void *hand_out(void *argv);

    void run(){
        pthread_t tid1, tid2, tid3;
        pthread_create(&tid1, NULL, check_participant, (void *)this);
        pthread_create(&tid2, NULL, receive_client, (void *)this);
        pthread_create(&tid3, NULL, hand_out, (void *)this);
        while(1){
            receive_participant();
        }
    }

    
};

void *hand_out(void *argv){
    coordinator *p = (coordinator *)argv;
    while(1){
        // 从任务队列里面获取一个任务
        pthread_mutex_lock(&(p->worklist_lock));
        while(p->worklist.empty()){ //if也行 没有相同的线程在竞争
            pthread_cond_wait(&(p->worklist_cond), &(p->worklist_lock));
        }

        client_ask ask = p->worklist.front();
        p->worklist.pop_front();
        pthread_mutex_unlock(&(p->worklist_lock));

        // 判断一下是什么请求
        /*
            if(ask[i] == 'G')
                return 1;
            if(ask[i] == 'S')
                return 2;
            if(ask[i] == 'D')
                return 3;
        */
        /*
            get
            找一个能在运行的参与者直接发过去 然后把响应直接给客户端    
            如果没有响应 则换下个人人发    (定时) 要不要换呢 麻烦...
        */
        if(get_type(ask.message) == 1){
            int i = 0;
            // 干脆访问shutdown不加锁了 竞争比较小 
            // pthread_mutex_lock(&(p->heart_lock));
            again:
            while(i < p->n_participant && p->shut_down[i])
                ++i;
            // pthread_mutex_unlock(&(p->heart_lock));
            if(i == p->n_participant){
                ;
                //return error message continue
            }
            // 理论上每个协调者都需要一个锁  
            /*
                锁住
                发送请求
                开锁
                等会 怎么个等法 等多久 没有shutdown就一直等   检查recvdatai
                接受回复转发
            */
            pthread_mutex_lock(&(p->write_lock[i]));
            writen(p->fd_participant[i], ask.message);
            pthread_mutex_unlock(&(p->write_lock[i]));
            // 啊临界区统统不加锁了...
            while(!p->shut_down[i]){
                if(p->recv_data[i])
                    break;
            }
            if(p->shut_down[i])
                goto again;
            //pthread_mutex_lock(&(p->data_lock));  //这个锁是不是不用啊  每次都是读完了再叫这个线程过来取得 读完后recvdata才会更新
            ask.message = p->data_return[i];
            p->data_return[i].clear();
            p->recv_data[i] = false; //重置缓冲区状态
            //pthread_mutex_unlock(&(p->data_lock)); 
            writen(ask.fd, ask.message);
            close(ask.fd);
        }

        /*
            heart 锁的临界区太长了 可能无法正常收发心跳包
            del or set
            两阶段
            1 给每一个正在运行的参与者发送请求
            2 参与者收到请求(然后把请求做了) 给协调者确认
            3 协调者收到确认
              所有人都确认，给每个人确认包commit  有人没确认(定时) 给每个人发送回滚包rollback
            4 参与者收到确认包commit， 收到回滚包或者超时，rollback   有可能有人没收到这个 还没发完所有commit协调者断掉了怎么办

            1 给每一个正在运行的参与者发送请求
            2 参与者收到请求 记录这个串 回个ready
            3 协调者如果收到所有ready 则commit 否则 rollback

        */


    }
}

void *check_participant(void *argv){
    coordinator *p = (coordinator *)argv;
    while(1){
        // 每隔一秒钟检查一下
        sleep(1);
        pthread_mutex_lock(&(p->heart_lock));

        for(int i = 0; i < p->n_participant; ++i){
            // 这里如果断了 就不用检测了
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
                    //--(p->alive);

                    // 要求3
                    // 开个子线程去调用reconnecte_participant();
                }
            }
        }

        pthread_mutex_unlock(&(p->heart_lock));
    }
}

void *receive_client(void *argv){
    coordinator *p = (coordinator *)argv;

    struct sockaddr_in cliaddr;
    socklen_t cliaddr_len = sizeof(cliaddr);
    while(1){
        // 如果系统正在恢复 或者当前参与者全部失效了 阻塞在这里
        // ...

        //阻塞监听链接请求 这个连接可能来自参与者 可能来自客户端 需要判断一下
        cout<<"等待客户端连接中"<<endl;
        int connfd = accept(p->listenfd, (struct sockaddr *)&cliaddr, &cliaddr_len);   

        //看一下客户端信息
        char client_ip[40];
        int  client_port = ntohs(cliaddr.sin_port);
        inet_ntop(AF_INET, &cliaddr.sin_addr, client_ip, sizeof(client_ip));
        printf("新客户端连接ip: %s, port: %d\n", client_ip, client_port);
        printf("新连接cfd: %d\n", connfd);

        // 检查是否是参与者
        bool flag = false;
        for(int i = 0; i < p->n_participant; ++i){
            if(ip_int(client_ip) == p->ip_participant[i] && client_port == p->port_participant[i]){
                cout<<"这不是普通客户端"<<endl;
                close(connfd);
                flag = true;
                break;
            }
        }
        if(flag){
            continue;
        }
        // 只在这边解析方法 具体工作在hand_out里面做
        string data;
        int ret = readn(connfd, data);
        if(ret > 0){
            //cout<<data<<endl;
            pthread_mutex_lock(&(p->worklist_lock));
            // emm能不能用右值引用呢 拷贝好几次有很大额外开销
            p->worklist.push_back({connfd, data});
            pthread_cond_signal(&(p->worklist_cond)); // 唤醒正在等待的hand_out线程
            pthread_mutex_unlock(&(p->worklist_lock));
        }

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