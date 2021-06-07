#pragma once
#include <string>
#include <cstring>
#include <stdlib.h>
#include <vector>

const std::string subStr="\r\n";

enum Op
{
    NIL,
    SET,
    GET,
    DEL
};

struct Order
{
    Op op;
    std::string key;
    std::vector<std::string> value;
};

struct Done
{
    bool isTrue;
    unsigned long long int number;
    int version;
};

Order getOrder(std::string orderStr);
std::string setOrderToStr(Order order);

Done getDone(std::string doneStr);
std::string setDoneToStr(Done done);

std::string retDelNumToCli(int num);
std::string retSet(bool ok);