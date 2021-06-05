#pragma once
#include <string>
#include <cstring>
#include <stdlib.h>
#include <vector>

struct Order{
    int type;
    std::string key;
    std::vector<std::string> value;
};

Order getOrder(std::string orderStr);
std::string setOrder(Order order);