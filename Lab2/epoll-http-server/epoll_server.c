#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>
#include "epoll_server.h"
#include "threadpool.h"

#define MAXSIZE 2000

void send_error(int cfd, int status, char *title, char *text)
{
	char buf[4096] = {0};

	sprintf(buf, "%s %d %s\r\n", "HTTP/1.1", status, title);
	sprintf(buf+strlen(buf), "Content-Type:%s\r\n", "text/html");
	sprintf(buf+strlen(buf), "Content-Length:%d\r\n", -1);// 这里长度设置为-1，也可以写在下面 用strlen求出buf的长度
	sprintf(buf+strlen(buf), "Connection: close\r\n");
	send(cfd, buf, strlen(buf), 0);
	send(cfd, "\r\n", 2, 0);

	memset(buf, 0, sizeof(buf));

	sprintf(buf, "<html><head><title>%d %s</title></head>\n", status, title);
	sprintf(buf+strlen(buf), "<body bgcolor=\"#cc99cc\"><h2 align=\"center\">%d %s</h4>\n", status, title);
	sprintf(buf+strlen(buf), "%s\n", text);
	sprintf(buf+strlen(buf), "<hr>\n</body>\n</html>\n");
	send(cfd, buf, strlen(buf), 0);
	
	return ;
}

struct thread_argv{
    int lfd;
    int cfd;
};

void *hander(void *argv){
    struct thread_argv *can = (struct thread_argv *)argv;
    do_read(can->cfd, can->lfd);
    free((struct thread_argv *)argv);
    return NULL;
}


void epoll_run(int port, int n_thread)
{
    int i = 0;
    // 初始化线程池
    pool_init(n_thread);
    
    // 创建一个epoll树的根节点
    int epfd = epoll_create(MAXSIZE);
    if(epfd == -1) {   
        perror("epoll_create error");
        exit(1);
    }

    // 添加要监听的节点
    // 先添加监听lfd
    int lfd = init_listen_fd(port, epfd);

    // 委托内核检测添加到树上的节点
    struct epoll_event all[MAXSIZE];
    while(1) {
    
        int ret = epoll_wait(epfd, all, MAXSIZE, 0);
        if(ret == -1) {
            perror("epoll_wait error");
            exit(1);
        }

        // 遍历发生变化的节点 epoll相对与select的优势，不用全部遍历
        for(i=0; i < ret; ++i)
        {
            // 只处理读事件, 其他事件默认不处理
            struct epoll_event *pev = &all[i];
            if(!(pev->events & EPOLLIN)) {
                // 不是读事件
                continue;
            }
            if(pev->data.fd == lfd){
                // 接受连接请求
                do_accept(lfd, epfd);
            } else {
                // 读数据
                //printf("======================before do read, ret = %d\n", ret);
                // 这里使用的是短连接，读完数据后就断开连接
                // 如果是长连接 这里有个问题啊 如果建立连接之后一直没有发数据来，那么就不会去检测这个socket
                // 那么这个socket就会一直打开，虽然说epoll没有监听上限，但是会占内存
                // 需要加一个超时功能
                //do_read(pev->data.fd, epfd);
                struct thread_argv *can = malloc(sizeof(struct thread_argv));
                can->cfd = pev->data.fd;
                can->lfd = epfd;
                // 加入到线程池的任务队列中
                pool_add_worker(hander, (void*)can);
                //printf("=========================================after do read\n");
            }
        }
    }
}

// 读数据
void do_read(int cfd, int epfd)
{
    // 将浏览器发过来的数据, 读到buf中 
    char line[1024] = {0};
    // 读请求行
    int len = get_line(cfd, line, sizeof(line));
    if(len == 0) {   
        //printf("客户端断开了连接...\n");
        // 关闭套接字, cfd从epoll上del
        disconnect(cfd, epfd);         
    } else { 
    	//printf("============= 请求头 ============\n");   
        //printf("请求行数据: %s", line);
        // 还有数据没读完, 继续读走
		while (1) {
			char buf[1024] = {0};
			len = get_line(cfd, buf, sizeof(buf));	
			if (buf[0] == '\n') {
				break;	
			} else if (len == -1)
				break;
		}
        //printf("============= The End ============\n");
    }
    
    // 判断get请求
    if(strncasecmp("get", line, 3) == 0) { // 请求行: get /hello.c http/1.1   
        // 处理http请求
        http_request(line, cfd);
        
        // 关闭套接字, cfd从epoll上del
        disconnect(cfd, epfd);         
    }
    // 也可以处理post请求
}

// 断开连接的函数
void disconnect(int cfd, int epfd)
{
    // 从epoll的树上摘下来
    int ret = epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL);
    if(ret == -1) {
        perror("epoll_ctl del cfd error");
        exit(1);
    }
    close(cfd);
}

// http请求处理
void http_request(const char* request, int cfd)
{
    // 拆分http请求行
    char method[12], path[1024], protocol[12];
    sscanf(request, "%[^ ] %[^ ] %[^ ]", method, path, protocol);
    //printf("method = %s, path = %s, protocol = %s\n", method, path, protocol);

    // 转码 将不能识别的中文乱码 -> 中文
    // 解码 %23 %34 %5f
    decode_str(path, path);
        
    char* file = path + 1; // 去掉path中的/ 获取访问文件名
    //默认是当前路径 也就是把第一个/去掉
    
    // 如果没有指定访问的资源, 默认显示资源目录中的内容
    if(strcmp(path, "/") == 0) {    
        // file的值, 资源目录的当前位置
        file = "./";
    }

    // 获取文件属性
    struct stat st;
    int ret = stat(file, &st);
    if(ret == -1) { 
        send_error(cfd, 404, "Not Found", "NO such file or direntry");     
        return;
    }

    // 判断是目录还是文件
    if(S_ISDIR(st.st_mode)) {  		// 目录 
        // 发送头信息
        send_respond_head(cfd, 200, "OK", get_file_type(".html"), -1);
        // 发送目录信息
        send_dir(cfd, file);
    } else if(S_ISREG(st.st_mode)) { // 文件        
        // 发送消息报头
        send_respond_head(cfd, 200, "OK", get_file_type(file), st.st_size);
        // 发送文件内容
        send_file(cfd, file);
    }
}

// 发送目录内容
void send_dir(int cfd, const char* dirname)
{
    int i, ret;

    // 拼一个html页面<table></table>
    char buf[4094] = {0};

    sprintf(buf, "<html><head><title>目录名: %s</title></head>", dirname);
    sprintf(buf+strlen(buf), "<body><h1>当前目录: %s</h1><table>", dirname);

    char enstr[1024] = {0};
    char path[1024] = {0};
    
    // 目录项二级指针
    struct dirent** ptr;
    int num = scandir(dirname, &ptr, NULL, alphasort);
    
    // 遍历
    for(i = 0; i < num; ++i) {
    
        char* name = ptr[i]->d_name;

        // 拼接文件的完整路径
        sprintf(path, "%s/%s", dirname, name);
        //printf("path = %s ===================\n", path);
        struct stat st;
        stat(path, &st);

		// 编码生成 %E5 %A7 之类的东西
        encode_str(enstr, sizeof(enstr), name);
        
        // 如果是文件
        if(S_ISREG(st.st_mode)) {      
            // 这里如果不编码发过去，对面会打不开本地的中文文档和目录 
            sprintf(buf+strlen(buf), 
                    "<tr><td><a href=\"%s\">%s</a></td><td>%ld</td></tr>",
                    enstr, name, (long)st.st_size);
        } else if(S_ISDIR(st.st_mode)) {		// 如果是目录       
            sprintf(buf+strlen(buf), 
                    "<tr><td><a href=\"%s/\">%s/</a></td><td>%ld</td></tr>",
                    enstr, name, (long)st.st_size);
        }
        ret = send(cfd, buf, strlen(buf), 0);
        if (ret == -1) {
            if (errno == EAGAIN) {
                perror("send error:");
                continue;
            } else if (errno == EINTR) {
                perror("send error:");
                continue;
            } else {
                perror("send error:");
                exit(1);
            }
        }
        memset(buf, 0, sizeof(buf));
        // 字符串拼接
    }

    sprintf(buf+strlen(buf), "</table></body></html>");
    send(cfd, buf, strlen(buf), 0);

    //printf("dir message send OK!!!!\n");
#if 0
    // 打开目录
    DIR* dir = opendir(dirname);
    if(dir == NULL)
    {
        perror("opendir error");
        exit(1);
    }

    // 读目录
    struct dirent* ptr = NULL;
    while( (ptr = readdir(dir)) != NULL )
    {
        char* name = ptr->d_name;
    }
    closedir(dir);
#endif
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
    char buf[4096] = {0};
    int len = 0, ret = 0;
    // 设置成了非阻塞
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
                exit(1);
            }
        }
    }
    if(len == -1)  {  
        perror("read file error");
        exit(1);
    }

    close(fd);
}

// 解析http请求消息的每一行内容
int get_line(int sock, char *buf, int size)
{
    int i = 0;
    char c = '\0';
    int n;
    while ((i < size - 1) && (c != '\n')) { 
        // 这个地方是阻塞接收，可以定个时   
        n = recv(sock, &c, 1, 0);
        if (n > 0) {        
            if (c == '\r') {            
                n = recv(sock, &c, 1, MSG_PEEK);
                if ((n > 0) && (c == '\n')) {               
                    recv(sock, &c, 1, 0);
                } else {                            
                    c = '\n';
                }
            }
            buf[i] = c;
            i++;
        } else {       
            c = '\n';
        }
    }
    buf[i] = '\0';

    return i;
}

// 接受新连接处理
void do_accept(int lfd, int epfd)
{
    struct sockaddr_in client;
    socklen_t len = sizeof(client);
    int cfd = accept(lfd, (struct sockaddr*)&client, &len);
    if(cfd == -1) {  
        perror("accept error");
        exit(1);
    }

    // 打印客户端信息
    char ip[64] = {0};
    // printf("New Client IP: %s, Port: %d, cfd = %d\n",
    //        inet_ntop(AF_INET, &client.sin_addr.s_addr, ip, sizeof(ip)),
    //        ntohs(client.sin_port), cfd);

    // 设置cfd为非阻塞
    int flag = fcntl(cfd, F_GETFL);
    flag |= O_NONBLOCK;
    fcntl(cfd, F_SETFL, flag);

    // 得到的新节点挂到epoll树上
    struct epoll_event ev;
    ev.data.fd = cfd;
    // 边沿非阻塞模式
    ev.events = EPOLLIN | EPOLLET;
    int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev);
    if(ret == -1) { 
        perror("epoll_ctl add cfd error");
        exit(1);
    }
}

int init_listen_fd(int port, int epfd)
{
    //　创建监听的套接字
    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    if(lfd == -1) {  
        perror("socket error");
        exit(1);
    }

    // lfd绑定本地IP和port
    struct sockaddr_in serv;
    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_port = htons(port);
    serv.sin_addr.s_addr = htonl(INADDR_ANY);

    // 端口复用
    int flag = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    
    int ret = bind(lfd, (struct sockaddr*)&serv, sizeof(serv));
    if(ret == -1) {    
        perror("bind error");
        exit(1);
    }

    // 设置监听
    ret = listen(lfd, 64);
    if(ret == -1) {    
        perror("listen error");
        exit(1);
    }

    // lfd添加到epoll树上
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = lfd;
    ret = epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &ev);
    if(ret == -1) {  
        perror("epoll_ctl add lfd error");
        exit(1);
    }
    return lfd;
}

// 16进制数转化为10进制
int hexit(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;

    return 0;
}

/*
 *  这里的内容是处理%20之类的东西！是"解码"过程。
 *  %20 URL编码中的‘ ’(space)
 *  %21 '!' %22 '"' %23 '#' %24 '$'
 *  %25 '%' %26 '&' %27 ''' %28 '('......
 *  相关知识html中的‘ ’(space)是&nbsp
 */
void encode_str(char* to, int tosize, const char* from)
{
    int tolen;

    for (tolen = 0; *from != '\0' && tolen + 4 < tosize; ++from) {    
        if (isalnum(*from) || strchr("/_.-~", *from) != (char*)0) {      
            *to = *from;
            ++to;
            ++tolen;
        } else {
            sprintf(to, "%%%02x", (int) *from & 0xff);
            to += 3;
            tolen += 3;
        }
    }
    *to = '\0';
}

void decode_str(char *to, char *from)
{
    for ( ; *from != '\0'; ++to, ++from  ) {     
        if (from[0] == '%' && isxdigit(from[1]) && isxdigit(from[2])) {       
            *to = hexit(from[1])*16 + hexit(from[2]);
            from += 2;                      
        } else {
            *to = *from;
        }
    }
    *to = '\0';
}

// 通过文件名获取文件的类型
const char *get_file_type(const char *name)
{
    char* dot;

    // 自右向左查找‘.’字符, 如不存在返回NULL
    dot = strrchr(name, '.');   
    if (dot == NULL)
        return "text/plain; charset=utf-8";
    if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0)
        return "text/html; charset=utf-8";
    if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0)
        return "image/jpeg";
    if (strcmp(dot, ".gif") == 0)
        return "image/gif";
    if (strcmp(dot, ".png") == 0)
        return "image/png";
    if (strcmp(dot, ".css") == 0)
        return "text/css";
    if (strcmp(dot, ".au") == 0)
        return "audio/basic";
    if (strcmp( dot, ".wav" ) == 0)
        return "audio/wav";
    if (strcmp(dot, ".avi") == 0)
        return "video/x-msvideo";
    if (strcmp(dot, ".mov") == 0 || strcmp(dot, ".qt") == 0)
        return "video/quicktime";
    if (strcmp(dot, ".mpeg") == 0 || strcmp(dot, ".mpe") == 0)
        return "video/mpeg";
    if (strcmp(dot, ".vrml") == 0 || strcmp(dot, ".wrl") == 0)
        return "model/vrml";
    if (strcmp(dot, ".midi") == 0 || strcmp(dot, ".mid") == 0)
        return "audio/midi";
    if (strcmp(dot, ".mp3") == 0)
        return "audio/mpeg";
    if (strcmp(dot, ".ogg") == 0)
        return "application/ogg";
    if (strcmp(dot, ".pac") == 0)
        return "application/x-ns-proxy-autoconfig";

    return "text/plain; charset=utf-8";
}