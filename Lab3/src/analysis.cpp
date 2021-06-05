#include "analysis.h"

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
    char delim[] = "\r\n";
    char *input = strdup(source);
    char *token = strsep(&input, delim);
    while (token != NULL)
    {
        if (*token != '\0')
        {
            orderList.emplace_back(std::string(token));
        }
        token = strsep(&input, delim);
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

std::string setOrder(Order order)
{
    std::string ret;
    if (order.type == 0)
    {
        ret = "*1\r\n$3\r\nnil\r\n";
        return ret;
    }
    int num = order.value.size();
    std::string substr="\r\n";
    ret="*";
    ret=ret+char(num+'0')+substr;
    for (auto a : order.value)
    {
        num=a.size();
        ret=ret+char(num+'0')+substr+a+substr;
    }
    return ret;
}
