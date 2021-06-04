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
#include <regex.h>
#include "tools.h"


//static char errbuf[ERROR_SIZE] = {0};

//合法ip的正则
//static const char *ip_format = "(25[0-4]|2[0-4][0-9]|1[0-9][0-9]|[1-9][0-9]|[1-9])[.](25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9][0-9]|[0-9])[.](25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9][0-9]|[0-9])[.](25[0-4]|2[0-4][0-9]|1[0-9][0-9]|[1-9][0-9]|[1-9])";
//static const char *ip_format = "^([0-9]|[1-9][0-9]|1[0-9]{1,2}|2[0-4][0-9]|25[0-4]).([0-9]|[1-9][0-9]|1[0-9]{1,2}|2[0-4][0-9]|25[0-5]).([0-9]|[1-9][0-9]|1[0-9]{1,2}|2[0-4][0-9]|25[0-5]).([1-9]|[1-9][0-9]|1[0-9]{1,2}|2[0-4][0-9]|25[0-5])$";

// bool get_IP_legal(regex_t *ipreg, const char *ip){
// 	regmatch_t pmatch[1];
// 	const size_t nmatch = 1;
// 	int status = regexec(ipreg, ip, nmatch, pmatch, 0);
// 	if(status == 0) {
// 		//printf("Match Successful!\n");
//         return true;
// 	}
// 	else if(status == REG_NOMATCH) {
// 		regerror(status, ipreg, errbuf, ERROR_SIZE);	
// 		//printf("%s\n", errbuf);
//         memset(errbuf, 0, ERROR_SIZE);
//         return false;
		
// 	}
//     return true;
// }

bool get_name_id(char *str, int len, char *name, char *id){
    int spite = 0;
    while(spite < len && str[spite] != '&')
        ++spite;
    if(spite == len)
        return false;

    // 获取name
    int equal1 = 0;
    while(equal1 < spite && str[equal1] != '=')
        ++equal1;
    if(!(equal1 == 4 && str[0] == 'N' && str[1] == 'a' && str[2] == 'm' && str[3] == 'e'))
        return false;
    int index1 = 0;
    for(int i = equal1 + 1; i < spite; ++i){
        name[index1++] = str[i];
    }
    name[index1] = '\0';

    // 获取id
    int equal2 = spite + 1;
    while(equal2 < len && str[equal2] != '=')
        ++equal2;
    if(!(equal2 - spite == 3 && str[spite + 1] == 'I' && str[spite + 2] == 'D'))
        return false;
    int index2 = 0;
    for(int i = equal2 + 1; i < len; ++i){
        id[index2++] = str[i];
    }
    id[index2] = '\0'; 
    return true;
}

// ip转换成int 进行非法判断
unsigned int ip_int(char *ip)
{   
    // //编译正则
    // regex_t ipreg1;
	// int reg = regcomp(&ipreg1, ip_format, REG_EXTENDED);
	// if(reg != 0) {
	// 	regerror(reg, &ipreg1, errbuf, ERROR_SIZE);	
	// 	printf("%s\n", errbuf);
	// 	memset(errbuf, 0, ERROR_SIZE);
	// 	return 0;
	// }
    // if(!get_IP_legal(&ipreg1 ,ip)){
    //     printf("Illegal IP!! Please Check!\n");
    //     return __INT32_MAX__;
    // }
    unsigned int re = 0;
    unsigned char tmp = 0;
    //printf("%s\n", ip);
    //printf("%d\n", strlen(ip));
    while (1) {
        if (*ip != '\0' && *ip != '.') {
            tmp = tmp * 10 + *ip - '0';
        } else {
            re = (re << 8) + tmp;
            if (*ip == '\0')
                break;
            tmp = 0;
        }
        ip++;
    }
    return re;
}

// 通过文件名获取文件的类型
// const char *get_file_type(const char *name)
// {
//     char* dot;
//     // 自右向左查找‘.’字符, 如不存在返回NULL
//     dot = strrchr(name, '.');   
//     if (dot == NULL)
//         return "text/plain; charset=utf-8";
//     if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0)
//         //return "text/html; charset=utf-8";
//         return "text/html";
//     if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0)
//         return "image/jpeg";
//     if (strcmp(dot, ".gif") == 0)
//         return "image/gif";
//     if (strcmp(dot, ".png") == 0)
//         return "image/png";
//     if (strcmp(dot, ".css") == 0)
//         return "text/css";
//     if (strcmp(dot, ".au") == 0)
//         return "audio/basic";
//     if (strcmp( dot, ".wav" ) == 0)
//         return "audio/wav";
//     if (strcmp(dot, ".avi") == 0)
//         return "video/x-msvideo";
//     if (strcmp(dot, ".mov") == 0 || strcmp(dot, ".qt") == 0)
//         return "video/quicktime";
//     if (strcmp(dot, ".mpeg") == 0 || strcmp(dot, ".mpe") == 0)
//         return "video/mpeg";
//     if (strcmp(dot, ".vrml") == 0 || strcmp(dot, ".wrl") == 0)
//         return "model/vrml";
//     if (strcmp(dot, ".midi") == 0 || strcmp(dot, ".mid") == 0)
//         return "audio/midi";
//     if (strcmp(dot, ".mp3") == 0)
//         return "audio/mpeg";
//     if (strcmp(dot, ".ogg") == 0)
//         return "application/ogg";
//     if (strcmp(dot, ".pac") == 0)
//         return "application/x-ns-proxy-autoconfig";
//     return "text/plain; charset=utf-8";
// }

// 读取http请求的一行
int get_line(int sock, char *buf, int size)
{
    int i = 0;
    char c = '\0';
    int n;
    while ((i < size ) && (c != '\n')) {   
        //n = recv(sock, &c, 1, MSG_DONTWAIT); // 非阻塞  即没有数据不会死等 然后就会跳到else 跳出循环 如果没有数据就返回0
        //设置定时器
        struct timeval timeout = {0, 1};//3s
        // int ret=setsockopt(sock,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));
        int ret=setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        n = recv(sock, &c, 1, 0);
	    if (n > 0) {        
            if (c == '\r') {        
                n = recv(sock, &c, 1, MSG_PEEK); //不从管道中拿出来
                if ((n > 0) && (c == '\n')) {              
                    recv(sock, &c, 1, 0);
                } else {                       
                    c = '\n';
                }
            }
            buf[i] = c;
            i++;
        } else {    
           // if(errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
            //    continue; 
            c = '\n';
        }
    }
    buf[i] = '\0';
    return i;
}

std::string getOptConf(int argc, char **argv)
{
    int opt;              // getopt_long() 的返回值
    int digit_optind = 0; // 设置短参数类型及是否需要参数

    // 如果option_index非空，它指向的变量将记录当前找到参数符合long_opts里的
    // 第几个元素的描述，即是long_opts的下标值
    int option_index = 0;
    // 设置短参数类型及是否需要参数
    const char *optstring = "";

    // 设置长参数类型及其简写，比如 --reqarg <==>-r
    /*
    struct option {
             const char * name;  // 参数的名称
             int has_arg; // 是否带参数值，有三种：no_argument， required_argument，optional_argument
             int * flag; // 为空时，函数直接将 val 的数值从getopt_long的返回值返回出去，
                     // 当非空时，val的值会被赋到 flag 指向的整型数中，而函数返回值为0
             int val; // 用于指定函数找到该选项时的返回值，或者当flag非空时指定flag指向的数据的值
        };
    其中：
        no_argument(即0)，表明这个长参数不带参数（即不带数值，如：--name）
            required_argument(即1)，表明这个长参数必须带参数（即必须带数值，如：--name Bob）
            optional_argument(即2)，表明这个长参数后面带的参数是可选的，（即--name和--name Bob均可）
     */
    static struct option long_options[] = {
        {"config_path", required_argument, NULL, 'r'},
        {0, 0, 0, 0} // 添加 {0, 0, 0, 0} 是为了防止输入空值
    };
    std::string name = "1";
    while ((opt = getopt_long(argc,
                              argv,
                              optstring,
                              long_options,
                              &option_index)) != -1)
    {
        if (strcmp(long_options[option_index].name, "config_path") == 0){
            printf("config_path optarg = %s\n", optarg); // 参数内容
            printf("\n");
            name = optarg;
        }
    }
    return name;
}