#include "tools.h"

// ip转换成int 不进行非法判断
unsigned int ip_int(char *ip)
{   
    unsigned int re = 0;
    unsigned char tmp = 0;
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

string getOptConf(int argc, char **argv)
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

int get_type(string &ask){
    int n = ask.size();
    if(n == 0) 
        return 0;
    int count_rn = 0;
    int i;
    for(i = 0; i < n; ++i){
        if(count_rn == 2) 
            break;
        if(i + 1 < n && ask[i] == '\r' && ask[i + 1] == '\n'){
            ++count_rn;
            ++i;
            continue;
        }
    }
    if(count_rn < 2 || i >= n) 
        return 0;
    if(ask[i] == 'G')
        return 1;
    if(ask[i] == 'S')
        return 2;
    if(ask[i] == 'D')
        return 3;
    return 0;
}

Order getOrder(std::string orderStr){
    Order order;
    if(orderStr[0] != '*'){
        order.op = NIL;
        return order;
    }
    std::vector<std::string> orderList;
    const char *source = orderStr.c_str();
    char delim[] = "\r\n";
    char *input = strdup(source);
    char *token = strsep(&input, delim);
    while(token != NULL){
        if(*token != '\0'){
            orderList.emplace_back(std::string(token));
        }
        token = strsep(&input, delim);
    }
    free(input);
    int num = orderList.size();
    if(orderList[2] == "SET"){
        order.op = SET;
        order.key = orderList[4];
        for (int i = 6; i < num; i += 2){
            order.value.emplace_back(orderList[i]);
        }
    }
    else if(orderList[2] == "GET"){
        order.op = GET;
        order.key = orderList[4];
    }
    else{
        order.op = DEL;
        for (int i = 4; i < num; i += 2){
            order.value.emplace_back(orderList[i]);
        }
    }
    return order;
}

std::string setOrder(Order order){
    std::string ret;
    int num = order.value.size();
    if (num == 0)
    {
        ret = "*1\r\n$3\r\nnil\r\n";
        return ret;
    }
    std::string substr = "\r\n";
    ret = "*";
    ret = ret + std::to_string(num) + substr;
    for (auto a : order.value)
    {
        num = a.size();
        ret = ret + "$" +std::to_string(num) + substr + a + substr;
    }
    return ret;
}


// 从客户端读
ssize_t readn(int fd, std::string &inBuffer){
    ssize_t nread = 0;
    ssize_t readSum = 0;
    // 设置个定时 客户端那边收到回复之前不会关闭连接
    struct timeval timeout = {0, 1};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    while(true){
        char buff[MAX_BUFF];
        if((nread = read(fd, buff, MAX_BUFF)) < 0){
            // 正常中断处理
            if(errno == EINTR)
                continue;
            // 如果设置了超时的话 超过设置的时间了 
            else if(errno == EAGAIN){
                return readSum;
            }  
            else{
                perror("read error");
                return -1;
            }
        }
        // 对端关闭了socket  或者写缓冲满了
        else if(nread == 0){
            break;
        }
        readSum += nread;
        inBuffer += std::string(buff, buff + nread);
    }
    return readSum;
}

// kv系统内部使用 有个问题 可能会读超过len个
ssize_t readn(int fd, string &inBuffer, int len){
    ssize_t nread = 0;
    ssize_t readSum = 0;
    inBuffer.clear(); //先清空
    while(readSum < len){
        char buff[MAX_BUFF];
        if((nread = read(fd, buff, MAX_BUFF)) < 0){
            // 正常中断处理
            if(errno == EINTR)
                continue;
            // 如果设置了超时的话 超过设置的时间了 
            else if(errno == EAGAIN){
                return readSum;
            }  
            else{
                perror("read error");
                return -1;
            }
        }
        // 读：对端关闭了socket  写：写缓冲满了
        else if(nread == 0){
            break;
        }
        readSum += nread;
        inBuffer += string(buff, buff + nread);
    }
    return readSum;
}


ssize_t writen(int fd, std::string &sbuff){
    size_t nleft = sbuff.size();
    ssize_t nwritten = 0;
    ssize_t writeSum = 0;
    const char *ptr = sbuff.c_str();
    while(nleft > 0){
        if((nwritten = write(fd, ptr, nleft)) <= 0){
            if(nwritten < 0){
                if(errno == EINTR){
                    nwritten = 0;
                    continue;
                }
                else if(errno == EAGAIN)
                    break;
                else
                    return -1;
            }
        }
        writeSum += nwritten;
        nleft -= nwritten;
        ptr += nwritten;
    }
    // 传完的清除掉
    if(writeSum == static_cast<int>(sbuff.size()))
        sbuff.clear();
    else
        sbuff = sbuff.substr(writeSum);
    return writeSum;
}

ssize_t read_head(int fd, packet_head *head){
    ssize_t ret;
    while(1){
        ret = read(fd, head, 8);
        if(ret < 0){
            if(errno == EINTR)
                continue;
            else{
                perror("read error");
                return -1;
            }
        }
        if(ret == 0)
            return 0;
        else{
            return ret;
        }
    }
    return 0;
}

ssize_t write_head(int fd, packet_head *head){
    ssize_t ret;
    while(1){
        ret = write(fd, head, 8);
        if(ret < 0){
            if(errno == EINTR)
                continue;
            else{
                perror("read error");
                return -1;
            }
        }
        if(ret == 0)
            return 0;
        else{
            return ret;
        }
    }
    return 0;
}