#ifndef TOOLS_H
#define TOOLS_H
#define ERROR_SIZE 256
#include <string>
#include <getopt.h>
#include <vector>
#define MAX_BUFF 1024


// ip转换成int 不进行非法判断
unsigned int ip_int(char *ip);

// 读取http请求的一行
int get_line(int sock, char *buf, int size);

// 从输入参数获取配置文件信息 没有判断文件是否存在
std::string getOptConf(int argc, char **argv);



ssize_t readn(int fd, std::string &inBuffer);
ssize_t writen(int fd, std::string &sbuff);

// 获取请求类型
int get_type(std::string &ask);

enum Op{NIL, SET, GET, DEL};

struct Order
{
    Op op;
    std::string key;
    std::vector<std::string> value;
};

Order getOrder(std::string orderStr);
std::string setOrder(Order order);

#endif