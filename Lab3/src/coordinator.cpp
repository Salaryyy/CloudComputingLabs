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

// // 心跳包可能用不上了 
// enum Type {heart, data, done, commit, rollback};
// // 包头八个字节
// struct packet_head{
//     Type type;
//     int length;
// };

// 处理完后记得关闭连接fd
struct client_ask{
    int fd; 
    string message;
};

// void *check_participant(void *argv);
void *receive_client(void *argv);
void *hand_out(void *argv);
void *receive_participant(void *argv);

class coordinator{
private:
    // 配置信息
    int listenfd;
    struct sockaddr_in servaddr;
    unsigned int myip;
    unsigned short myport;
    int n_participant;
    int cur;
    vector<unsigned int> ip_participant;
    vector<unsigned short> port_participant;
    vector<int> fd_participant;
    Conf myconf;
    
    // 参与者
    vector<pthread_mutex_t> write_lock;
    // pthread_mutex_t heart_lock;
    // pthread_mutex_t data_lock;
    // vector<int>  count_heart;
    // vector<bool> recv_heart;
    vector<bool> shut_down;
    vector<string> data_return;
    vector<bool> recv_data;
    
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
        

        cur = 0;
        //heart_lock = PTHREAD_MUTEX_INITIALIZER;
        write_lock.resize(n_participant);
        //count_heart.resize(n_participant);
        //recv_heart.resize(n_participant);
        shut_down.resize(n_participant);
        data_return.resize(n_participant);
        recv_data.resize(n_participant);

        for(int i = 0; i < n_participant; ++i){
            write_lock[i] = PTHREAD_MUTEX_INITIALIZER;
            //count_heart[i] = 0;
            //recv_heart[i] = false;
            shut_down[i] = false;
            recv_data[i] = false;
            
        }

        worklist_lock = PTHREAD_MUTEX_INITIALIZER;
        worklist_cond = PTHREAD_COND_INITIALIZER;
    }

    void init_serveraddr(){
        int ret;
        listenfd = socket(AF_INET, SOCK_STREAM, 0);                          //创建一个socket, 得到lfd
        if(listenfd < 0){
            cout<<"Create Socket Failed!"<<endl;
            exit(1);
        }
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
            cout<<"等待参与者连接中"<<endl;
            int connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &cliaddr_len);   
            
            //看一下客户端信息
            char client_ip[40];
            int  client_port = ntohs(cliaddr.sin_port);
            inet_ntop(AF_INET, &cliaddr.sin_addr, client_ip, sizeof(client_ip));
            // printf("received from %s at PORT %d\n", client_ip, client_port);
            // printf("新连接cfd: %d\n", connfd);
            cout<<"新连接 ip"<<client_ip<<" port"<<client_port<<endl;

            // 检查是否是参与者
            for(int i = 0; i < n_participant; ++i){
                if(ip_int(client_ip) == ip_participant[i] && client_port == port_participant[i]){
                    cout<<"这是参与者"<<i<<endl;
                    // 如果之前没有建立过连接 计数+1
                    if(fd_participant[i] == -1){
                        ++cur_participant;
                    }
                    // 如果之前有建立过连接 把原来的socket关了
                    else{
                        cout<<"之前建立过连接，关闭原来的socket"<<fd_participant[i]<<endl;
                        close(fd_participant[i]);
                    }
                    fd_participant[i] = connfd;
                    break;
                }
                else{
                    if(i == n_participant - 1){
                        cout<<"这不是参与者，关闭连接"<<endl;
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

    // 接收参与者的消息 每一个参与者配一个
    friend void *receive_participant(void *argv);

    // 负责监听普通客户端的请求
    friend void *receive_client(void *argv);

    // 分发任务
    friend void *hand_out(void *argv);

    void run(){
        pthread_t tid[n_participant + 2];
        for(int i = 0; i < n_participant; ++i){
            pthread_create(&tid[i], NULL, receive_participant, (void *)this);
        }
        pthread_create(&tid[n_participant], NULL, receive_client, (void *)this);
        pthread_create(&tid[n_participant + 1], NULL, hand_out, (void *)this);

        for(int i = 0; i < n_participant + 2; ++i){
            pthread_join(tid[i], NULL);
        }
    }

};

void *hand_out(void *argv){
    coordinator *p = (coordinator *)argv;
    packet_head head;
    int ret;
    while(1){
        // 从任务队列里面获取一个任务
        pthread_mutex_lock(&(p->worklist_lock));

        while(p->worklist.empty()){ //if也行 没有相同的线程在竞争
            pthread_cond_wait(&(p->worklist_cond), &(p->worklist_lock));
        }
        client_ask ask = p->worklist.front();
        p->worklist.pop_front();

        pthread_mutex_unlock(&(p->worklist_lock));
        // 设置好包头
        head.type = data;
        head.length = ask.message.size();
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
            如果没有响应 则换下个人人发
        */
        cout<<"开始分发任务："<<ask.message<<endl;
        if(get_type(ask.message) == 1){
            int i = 0;
            // 干脆访问shutdown不加锁了 竞争比较小 
            again:
            while(i < p->n_participant && p->shut_down[i])
                ++i;
            if(i == p->n_participant){
                string message = "-ERROR\r\n";
                writen(ask.fd, message);
                cout<<"全部down机了，返回error"<<endl;
                close(ask.fd);
                continue;
                //return error message continue
            }
            // 每个协调者都需要一个写锁  
            /*
                锁住
                发送请求
                开锁
                等会 怎么个等法 等多久 没有shutdown就一直等   检查recvdata[i]
                接受回复转发
            */
            
            pthread_mutex_lock(&(p->write_lock[i]));
            write_head(p->fd_participant[i], &head);    //先发个包头
            writen(p->fd_participant[i], ask.message);  //数据   判断down机..............
            pthread_mutex_unlock(&(p->write_lock[i]));
            cout<<"将请求发送给"<<i<<"号参与者"<<endl;
            // 临界区统统不加锁了... 只要沒有关机就一直等
            while(!p->shut_down[i]){
                if(p->recv_data[i]){
                    p->recv_data[i] = false; //重置缓冲区状态
                    break;
                }
            }
            
            if(p->shut_down[i])
                goto again;
            //pthread_mutex_lock(&(p->data_lock));  //这个锁不用  每次都是读完了再叫这个线程过来取得 读完后recvdata才会更新
            cout<<"收到回复:  "<<p->data_return[i]<<endl;
            //ask.message = p->data_return[i];
            
            //p->recv_data[i] = false; //重置缓冲区状态
            //pthread_mutex_unlock(&(p->data_lock)); 
            writen(ask.fd, p->data_return[i]); //这里要不要定时
            p->data_return[i].clear();
            cout<<"回复完毕"<<endl;
            close(ask.fd);
        }
        // 两阶段提交
        /*
            del or set
            两阶段
            方案1：
            1 给每一个正在运行的参与者发送请求
            2 参与者收到请求(然后把请求做了) 给协调者确认
            3 协调者收到确认
              所有人都确认，给每个人确认包commit  有人没确认(定时) 给每个人发送回滚包rollback
            4 参与者收到确认包commit， 收到回滚包或者超时，rollback   有可能有人没收到这个 还没发完所有commit协调者断掉了怎么办 不管
            方案2：
            1 给每一个正在运行的参与者发送请求
            2 参与者收到请求 记录这个串 回个ready
            3 协调者如果收到所有ready 则commit 否则 rollback

        */
        else if(get_type(ask.message) == 2 || get_type(ask.message) == 3){
            //1 给每一个正在运行的参与者发送请求
            int total = 0;
            for(int i = 0; i < p->n_participant; ++i){
                if(p->shut_down[i]) //关机了就不发
                    continue;
                cout<<"正在发送给"<<i<<"请求"<<endl;
                pthread_mutex_lock(&(p->write_lock[i]));
                write_head(p->fd_participant[i], &head);          //先发个包头
                ret = writen(p->fd_participant[i], ask.message);  //数据
                pthread_mutex_unlock(&(p->write_lock[i]));
                // 有可能发不出去 发的时候down了
                if(ret == 0){
                    cout<<"发送给"<<i<<"失败"<<endl;
                    cout<<p->shut_down[i]<<endl;
                    continue;
                }
                cout<<"成功发送给 "<<i<<" 请求"<<endl;
                ++total;
            }
            cout<<"总共给 "<<total<<" 个参与者发送请求"<<endl;
            //2 协调者接收确认消息 所有人都确认，给每个人确认包commit  有人没确认(定时) 给每个人发送回滚包rollback
            int finish = 0;
            string respose;
            bool token = false;
            for(int i = 0; i < p->n_participant; ++i){
                // 在shutdown之前一直等
                while(!p->shut_down[i]){
                    if(p->recv_data[i]){
                        ++finish;
                        p->recv_data[i] = false; //重置缓冲区状态
                        if(!token){
                            token = true;
                            respose = p->data_return[i];
                        }
                        break;
                    }
                }
            }
            // commit
            if(finish == total){
                cout<<"全部确认，commit"<<endl;
                head.type = commit;
                // 回复消息结果
            }
            // rollback
            else{
                cout<<"有参与者未确认，rollback"<<endl;
                head.type = rollback;
                //回复error
                respose = "-ERROR\r\n";
            }
            head.length = 0;
            for(int i = 0; i < p->n_participant; ++i){
                if(p->shut_down[i])
                    continue;
                write_head(p->fd_participant[i], &head);
            }
            writen(ask.fd, respose);
            close(ask.fd);
        }
        else{
            string message = "-ERROR\r\n";
            writen(ask.fd, message);
            cout<<"无法解析该请求"<<endl;
            close(ask.fd);
        }
    }
    return NULL;
}




void *receive_participant(void *argv){
    coordinator *p = (coordinator *)argv;
    int pno, ret;
    packet_head head;
    char buf[10];
    string message;

    pthread_mutex_lock(&p->worklist_lock);
    pno = (p->cur)++;
    pthread_mutex_unlock(&p->worklist_lock);
    cout<<"掌管参与者 "<<pno<<endl;
    while(1){
        // 读取包头 这里可以封装一下
        ret = read_head(p->fd_participant[pno], &head);
        if(ret == 0){
            cout<<"参与者 "<<pno<<" 断开连接"<<endl;
            p->shut_down[pno] = true;
            // 只完成版本2的话 这个线程可以一直睡着了
            return NULL;
            // 要完成版本3的话 这个线程要accept该参与者上线
        }

        if(head.type == data){
            cout<<"读到一个data包"<<endl;
            ret = readn(p->fd_participant[pno], p->data_return[pno], head.length);
            if(ret == 0){ //对端关闭
                p->shut_down[pno] = true;
                return NULL;
                // 要完成版本3的话 这个线程要accept该参与者上线
            }
            cout<<"数据长度"<<ret<<endl;
            p->recv_data[pno] = true;
        }
        else if(head.type == done){
            cout<<"读到一个done包"<<endl;
            ret = readn(p->fd_participant[pno], p->data_return[pno], head.length);
            if(ret == 0){ //对端关闭
                p->shut_down[pno] = true;
                return NULL;
                // 要完成版本3的话 这个线程要accept该参与者上线
            }
            cout<<"数据长度"<<ret<<endl;
            p->recv_data[pno] = true;
        }
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
        cout<<"新客户端连接 ip"<<client_ip<<"  port "<<client_port<<endl;
        cout<<"新连接cfd: "<<connfd<<endl;

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
        // 可以用自己的read函数
        int ret = readn(connfd, data);
        if(ret > 0){
            cout<<"接收到来自客户端的请求： "<<data<<endl;
            pthread_mutex_lock(&(p->worklist_lock));

            p->worklist.push_back({connfd, data});
            pthread_cond_signal(&(p->worklist_cond)); // 唤醒正在等待的hand_out线程
            
            pthread_mutex_unlock(&(p->worklist_lock));
        }
    }
}

void *hand_out(void *argv){
    coordinator *p = (coordinator *)argv;
    packet_head head;
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
        cout<<"开始分发任务："<<ask.message<<endl;
        if(get_type(ask.message) == 1){
            int i = 0;
            // 干脆访问shutdown不加锁了 竞争比较小 
            again:
            while(i < p->n_participant && p->shut_down[i])
                ++i;
            if(i == p->n_participant){
                string message = "-ERROR\r\n";
                writen(ask.fd, message);
                cout<<"全部down机了，返回error"<<endl;
                continue;
                //return error message continue
            }
            // 每个协调者都需要一个写锁  
            /*
                锁住
                发送请求
                开锁
                等会 怎么个等法 等多久 没有shutdown就一直等   检查recvdatai
                接受回复转发
            */
            cout<<"准备发送\n";
            head.type = data;
            head.length = ask.message.size();
            pthread_mutex_lock(&(p->write_lock[i]));
            write_head(p->fd_participant[i], &head);                        //先发个包头
            writen(p->fd_participant[i], ask.message);  //数据
            pthread_mutex_unlock(&(p->write_lock[i]));
            cout<<"将请求发送给"<<i<<"号参与者"<<endl;
            // 临界区统统不加锁了... 只要沒有关机就一直等
            while(!p->shut_down[i]){
                if(p->recv_data[i]){
                    p->recv_data[i] = false; //重置缓冲区状态
                    break;
                }
            }
            
            if(p->shut_down[i])
                goto again;
            //pthread_mutex_lock(&(p->data_lock));  //这个锁不用  每次都是读完了再叫这个线程过来取得 读完后recvdata才会更新
            cout<<"收到回复:  "<<p->data_return[i]<<endl;
            ask.message = p->data_return[i];
            p->data_return[i].clear();
            //p->recv_data[i] = false; //重置缓冲区状态
            //pthread_mutex_unlock(&(p->data_lock)); 
            writen(ask.fd, ask.message); //这里要不要定时
            cout<<"回复完毕"<<endl;
            close(ask.fd);
        }
    }
    return NULL;
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


int main(int argc, char **argv){
    handle_for_sigpipe();
    Conf conf = getConf(getOptConf(argc, argv));
    coordinator coor(conf);
    coor.init_serveraddr();
    coor.connect_participant();
    coor.run();
}