#pragma once
#include <vector>
#include <fstream>
#include <getopt.h>
#include <unistd.h>
#include <cstring>
#include <string>
using namespace std;


struct Part{
    string ip;
    int port;
};

struct Conf{
    bool isCoor;
    string coorIp;
    int coorPort;
    int partNum;
    vector<Part> part;
};

Conf getConf(string filename);
bool isExistConf(const string& name);
// 从输入参数获取配置文件信息 没有判断文件是否存在
std::string getOptConf(int argc, char **argv);