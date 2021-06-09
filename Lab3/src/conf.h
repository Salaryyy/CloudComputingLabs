#pragma once

#include <vector>
#include <fstream>
#include <getopt.h>
#include <unistd.h>
#include <cstring>
#include <string>


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
// 从输入参数获取配置文件信息 没有判断文件是否存在
std::string getOptConf(int argc, char **argv);