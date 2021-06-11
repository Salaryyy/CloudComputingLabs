#ifndef TOOLS_H
#define TOOLS_H
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
#include <stdbool.h>
#include <sstream>
#include <string>
#include <getopt.h>
#include <vector>
#include <iostream>
using namespace std;

#define ERROR_SIZE 256
#define MAX_BUFF 1024

// ip转换成int 不进行非法判断
unsigned int ip_int(char *ip);

// 获取请求类型
int get_type(std::string &ask);

// 请求类型
enum Op{NIL, SET, GET, DEL, SYN};
struct Order{
    Op op;
    string key;
    vector<std::string> value;
};

//把string转换为请求  把请求转换为string
Order getOrder(string orderStr);
string setOrder(Order order);

// 事先不知道长度的 从客户端读  设置定时
ssize_t readn(int fd, string &inBuffer);
// 事先知道长度的  阻塞读 不定时 根据错误信息返回
ssize_t readn(int fd, string &inBuffer, int len);
ssize_t writen(int fd, string &sbuff);


enum Type {heart, data, done, commit, rollback};
struct packet_head{
    Type type;
    int length;
};

//readhead  writehead
ssize_t read_head(int fd, packet_head *head);
ssize_t write_head(int fd, packet_head *head);




#endif