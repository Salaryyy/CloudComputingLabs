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
#include "threadpool.h"
#include "tools.h"
#include <getopt.h>


// 指定界面
// socket 状态码 短语 内容
void send_error(int cfd, int status, char *title, char *text)
{
	char buf[1024] = {0};
    char buf2[1024] = {0};

    // 界面
    sprintf(buf2, "<html><head><title>%d %s</title></head>\n", status, title);
	sprintf(buf2+strlen(buf2), "<body bgcolor=\"#cc99cc\"><h2 align=\"center\">%d %s</h4>\n", status, title);
	sprintf(buf2+strlen(buf2), "%s\n", text);
	sprintf(buf2+strlen(buf2), "<hr>\n</body>\n</html>\n");
	
    // 头部
    sprintf(buf, "%s %d %s\r\n", "HTTP/1.1", status, title);
	sprintf(buf+strlen(buf), "Content-Type:%s\r\n", "text/html");
	sprintf(buf+strlen(buf), "Content-Length:%ld\r\n", strlen(buf2)); // 这里需要获取html的长度
	//sprintf(buf+strlen(buf), "Connection: close\r\n");

	send(cfd, buf, strlen(buf), 0);
	send(cfd, "\r\n", 2, 0);
    send(cfd, buf2, strlen(buf2), 0);
	
	return;
}


// 发送响应头
void send_respond_head(int cfd, int no, const char* desp, const char* type, long len)
{
    char buf[1024] = {0};

    // 状态行
    sprintf(buf, "HTTP/1.1 %d %s\r\n", no, desp);
    send(cfd, buf, strlen(buf), 0);

    // 消息报头
    sprintf(buf, "Content-Type:%s\r\n", type);
    sprintf(buf+strlen(buf), "Content-Length:%ld\r\n", len);

    send(cfd, buf, strlen(buf), 0);
    // 空行
    send(cfd, "\r\n", 2, 0);	
}

// 发送文件
void send_file(int cfd, const char* filename)
{
    // 打开文件
    int fd = open(filename, O_RDONLY);
    if(fd == -1) {   
        send_error(cfd, 404, "Not Found", "NO such file or direntry");
        exit(1);
    }

    // 循环读文件
    char buf[1024] = {0};
    int len = 0, ret = 0;
    while( (len = read(fd, buf, sizeof(buf))) > 0 ) {   
        // 发送读出的数据
        ret = send(cfd, buf, len, 0);
        if (ret == -1) {
            if (errno == EAGAIN) {
                perror("send error:");
                continue;
            } else if (errno == EINTR) {
                perror("send error:");
                continue;
            } else {
                perror("send error:");
                close(fd);
                exit(1);
            }
        }
    }
    if(len == -1)  {  
        perror("read file error");
        close(fd);
        exit(1);
    }

    close(fd);
}


void http_request(const char* request, int cfd)
{
    // 拆分http请求行
    char method[12], path[1024], protocol[12];
    sscanf(request, "%[^ ] %[^ ] %[^ ]", method, path, protocol);

    char* file = path+1; // 去掉path中的/ 获取访问文件名
    // 获取文件属性
    if(strlen(file)==0){
        file="index.html";
    }
    //printf("file: %s\n",file);
    struct stat st;
    int ret = stat(file, &st);
    if(ret == -1) { 
        send_error(cfd, 404, "Not Found", "NO such file or direntry");     
        return;
    }

    if(S_ISREG(st.st_mode)) { // 是一个文件        
        // 发送消息报头
        send_respond_head(cfd, 200, "OK", get_file_type(file), st.st_size);
        // 发送文件内容
        send_file(cfd, file);
    }
}


void *http_hander(void *argc){
    int *p_confd = (int *)argc;
    int confd = *p_confd;

    // 将浏览器发过来的数据, 读到line中 
    char line[1024] = {0};
    int data_len = 0;
    // 读请求行
    int len = get_line(confd, line, sizeof(line));
    if(len == 0){   // 为啥用游览器访问这里要跳转好几次
        goto end;      
    } 
    else{ 
    	// printf("============= 请求头 ============\n");   
        // printf("请求行数据: %s", line);
		while(1) 
        {
			char buf[1024] = {0};
			len = get_line(confd, buf, sizeof(buf));
            if(strncasecmp("Content-Length", buf, 14) == 0){
                strcpy(buf, buf + 16);
                data_len = atoi(buf);
            }	
			if(buf[0] == '\n'){
                break;
            }
            else if(len == 0){
                break;
            }
            //printf("%s", buf);
		}
        //printf("============= The End ============\n");
    }

    char method[12], path[1024], protocol[12];
    sscanf(line, "%[^ ] %[^ ] %[^ ]", method, path, protocol);

    // 判断请求方法
    // get方法 以空行为结尾
    if(strncasecmp("get", line, 3) == 0) {
        http_request(line, confd);
    }
    // post方法 空行后面还有实体行
    else if(strncasecmp("post", line, 4) == 0){ // 这里还要固定文件名 正则表达式拿下来
        // 实体行里面没有换行符号 读一次就行
        // 这里不用设置长度了 使用了定时器的话
		int len = get_line(confd, line, data_len);
        char name[50], id[50];
        if(strcmp(path, "/Post_show") != 0 || !get_name_id(line, len, name, id)){ // 路径不对 或者键值对不对
            send_error(confd, 404, "Not Found", "Not Found"); 
            goto end;
        }
        char dest[1024] = {'\0'};
        strcat(dest, "Your Name: ");
        strcat(dest, name);
        strcat(dest, "\nID: ");
        strcat(dest, id);
        send_error(confd, 200, "OK", dest);
    }
    // 其他方法不管
    else{
        char dest[1024] = {'\0'};
        strcat(dest, "Does not implement this method: ");
        strcat(dest, method);
        send_error(confd, 501, "Not Implemented", dest);
    }



    // 关闭文件描述符 和 释放对应空间  一定要关 否则文件描述符很快就会达到上限
    end:
        //printf("客户端 %d 断开了连接...\n", confd);
        close(confd);  
        free(p_confd);
}



int main(int argc, char *argv[]){
    int ip, port, n_thread;
    if(argc != 7){
        printf("please check your input, example:\n");
        printf("./httpserver --ip 127.0.0.1 --port 8888 --number-thread 8\n");
        exit(1);
    }
    while(1){
        int option_index=0;
        static struct option long_options[]={
            {"ip",              required_argument,  0,  0},
            {"port",            required_argument,  0,  0},
            {"number-thread",   required_argument,  0,  0}
        };
        int c = getopt_long(argc, argv, "", long_options, &option_index);
        if(c == -1){
            break;
        }
        //printf("%d\n",option_index);
        switch (option_index)
        {
        case 0:
            ip = ip_int(optarg);
            if(ip==__INT32_MAX__){//非法地址
                exit(1);
            }
            break;
        case 1:
            port = atoi(optarg);
            break;
        case 2:
            n_thread = atoi(optarg);
            break;
        default:
            printf("参数错误\n");
            exit(1);
        }
    }
    // ip = ip_int(argv[2]);
    //  if(ip==__INT32_MAX__){//非法地址
    //     exit(1);
    // }
    // port = atoi(argv[4]);
    // n_thread = atoi(argv[6]);
    //printf("%d, %d, %d\n", ip, port, n_thread);

    // 设置好监听套接字
    struct sockaddr_in servaddr, cliaddr;
    socklen_t cliaddr_len;
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);                 //创建一个socket, 得到lfd
    bzero(&servaddr, sizeof(servaddr));                             //地址结构清零
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(ip);                           //指定本地IP // INADDR_ANY
    servaddr.sin_port = htons(port);                                //指定端口号 
    int opt = 1;                                                    //设置端口重用
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));   
    bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)); //绑定
    listen(listenfd, 1000);                                         //设置同一时刻链接服务器上限数
    //printf("Accepting client connect ...\n");


    //初始化线程池
    pool_init(n_thread);

    while (1) {
        cliaddr_len = sizeof(cliaddr);
        int *connfd = malloc(sizeof(int));
        *connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &cliaddr_len);   //阻塞监听客户端链接请求
        
        //看一下客户端信息
        //char str[1024];
        // printf("received from %s at PORT %d\n",
        //         inet_ntop(AF_INET, &cliaddr.sin_addr, str, sizeof(str)),
        //         ntohs(cliaddr.sin_port));
        // printf("cfd: %d\n", *connfd);


        // 加入线程池中
        pool_add_worker(http_hander, (void *)connfd);
    }

    pool_destroy();
}