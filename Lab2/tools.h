#ifndef TOOLS_H
#define TOOLS_H
#include <stdbool.h>

// ip转换成int 不进行非法判断
unsigned int ip_int(char *ip);

// 通过文件名获取文件的类型
const char *get_file_type(const char *name);

// 读取http请求的一行
int get_line(int sock, char *buf, int size);

// 获取name和id
bool get_name_id(char *str, int len, char *name, char *id);

#endif