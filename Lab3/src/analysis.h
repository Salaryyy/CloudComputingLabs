#pragma once
#include <string>
#include <cstring>
#include <stdlib.h>
#include <vector>

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

Order getOrder(std::string orderStr);
std::string setOrder(Order order);