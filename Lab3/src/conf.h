#pragma once

#include <vector>
#include <fstream>

#include <unistd.h>
#include <string.h>


struct Part
{
    std::string ip;
    int port;
};

struct Conf
{
    bool isCoor;
    std::string coorIp;
    int coorPort;
    int partNum;
    std::vector<Part> part;
};

Conf getConf(std::string filename);
bool isExistConf(const std::string& name);
