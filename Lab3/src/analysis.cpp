#include "analysis.h"
#include <sstream>

// 从cli中获得的字符串中提取指令
// return Order
//         op      操作类型
//             Op   枚举类型
//         key     键
//             set  中为键
//             del  为空
//             get  中为键
//         value   值，
//             set  中为空格隔开的各个单词
//             del  中为键，此时key为空
//             get  为空
Order getOrder(std::string orderStr)
{
    Order order;
    if (orderStr[0] != '*')
    {
        order.op = NIL;
        return order;
    }
    std::vector<std::string> orderList;
    const char *source = orderStr.c_str();
    char *input = strdup(source);
    char *token = strsep(&input, subStr.c_str());
    while (token != NULL)
    {
        if (*token != '\0')
        {
            orderList.emplace_back(std::string(token));
        }
        token = strsep(&input, subStr.c_str());
    }
    free(input);
    int num = orderList.size();
    if (orderList[2] == "SET")
    {
        order.op = SET;
        order.key = orderList[4];
        for (int i = 6; i < num; i += 2)
        {
            order.value.emplace_back(orderList[i]);
        }
    }
    else if (orderList[2] == "GET")
    {
        order.op = GET;
        order.key = orderList[4];
    }
    else
    {
        order.op = DEL;
        for (int i = 4; i < num; i += 2)
        {
            order.value.emplace_back(orderList[i]);
        }
    }
    return order;
}

//将指令或回复打包为字符串
//GET 时转发
std::string setOrderToStr(Order order)
{
    std::string ret;
    if (order.op == 0)
    {
        ret = "*1\r\n$3\r\nnil\r\n";
        return ret;
    }
    int num = order.value.size();
    ret="*";
    ret=ret+std::to_string(num)+subStr;
    for (auto a : order.value)
    {
        num=a.size();
        ret=ret+std::to_string(num)+subStr+a+subStr;
    }
    return ret;
}

// 2PC 中参与者与协调者之间的通讯协议
// isTrue 参与者同意或Done
// number 参与者编号，可用二进制表示
// version 版本号
Done getDone(std::string doneStr)
{
    Done done;
    std::vector<std::string> doneList;
    const char *source = doneStr.c_str();
    char *input = strdup(source);
    char *token = strsep(&input, subStr.c_str());
    while (token != NULL)
    {
        if (*token != '\0')
        {
            doneList.emplace_back(std::string(token));
        }
        token = strsep(&input, subStr.c_str());
    }
    free(input);
    int num = doneList.size();
    if(num!=3){
        done.isTrue=false;
        return done;
    }
    done.isTrue=("1"==doneList[0]);
    done.number=strtoul(doneList[1].c_str(),0,10);
    done.version=atoi(doneList[2].c_str());
    return done;
}

// 将Done对象打包成字符串
std::string setDoneToStr(Done done)
{
    std::string ret;
    if(done.isTrue){
        ret="1";
    }else{
        ret="0";
    }
    ret=ret+subStr+std::to_string(done.number)+subStr+std::to_string(done.version)+subStr;
    return ret;
}

// 用于给协调者针对SET返回相应结果的字符串
// ok 
//  true 表明成功
//  false 表明失败
std::string retSet(bool ok)
{
    if(ok)
    {
        return "+OK\r\n";
    }
    return "-ERROR\r\n";
}

// 用于构造协调者针对DEL返回相应的字符串
// 对于所给的del的个数num
// 构造如 
//      :num\r\n
// 的字符串
std::string retDelNumToCli(int num)
{
    std::string ret=":";
    ret=ret+std::to_string(num)+subStr;
    return ret;
}