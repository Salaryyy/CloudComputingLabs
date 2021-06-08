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
#include "tools.h"
#include <iostream>
#include <vector>
using namespace std;

// 到底在哪边解析呢..............
enum OP {set, get, del};
struct ask{
    OP op;
    string key;
    vector<string> value;
};

// // `*4\r\n$3\r\nSET\r\n$7\r\nCS06142\r\n$5\r\nCloud\r\n$9\r\nComputing\r\n` 1
// // `*2\r\n$3\r\nGET\r\n$7\r\nCS06142\r\n`
// // `*3\r\n$3\r\nDEL\r\n$7\r\nCS06142\r\n$5\r\nCS162\r\n`

// // 在协调者这边判断方法  整个串发给参与者
// // 

// // void recv{
//     // hash<string, vector<string>> new old


//     // 方案2 反向操作
//     //  收到set a b 
//     //  
//     //
//     //
//     //



//     // 方案1 备份
//     // 收到put包  old = new 修改new
//     // 回发done
//     // 收到控制包   确认/回滚
//     // 回滚：new = old  

// //}




// void parse_message(string &data){
// ;
// }

// int main(){
//     string s = "...";
//     parse_message(s);

// }


int main(){
    int connectSocket = socket(AF_INET, SOCK_STREAM, 0);

    char ip[32] = "127.0.0.1";
    unsigned int myip = ip_int(ip);
    sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(myip); 
    server.sin_port = htons(8001);

    int opt = 1;                                                    
    setsockopt(connectSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)); 

    while(connect(connectSocket, (struct sockaddr*)&server, sizeof(server)) != 0){
        cout<<"连接服务端失败， 继续尝试"<<endl;
        sleep(1);
    }

    cout<<"连接成功"<<endl;
    
    //string data = "*4\r\n$3\r\nSET\r\n$7\r\nCS06142\r\n$5\r\nCloud\r\n$9\r\nComputing999999\r\n";
    string data = "*2\r\n$3\r\nGET\r\n$7\r\nCS06142\r\n";
    //string data = "*3\r\n$3\r\nDEL\r\n$7\r\nCS06142\r\n$5\r\nCS162\r\n";
    //cin>>data;

    writen(connectSocket, data);
    char jieshou[2014];
    //jieshou[30] = '\0';
    //sleep(100);
    int ret = read(connectSocket, jieshou, sizeof(jieshou));
    jieshou[ret] = '\0';
    cout<<jieshou<<endl;
    
}