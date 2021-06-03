#include "conf.h"

//  读取配置文件
//  参数：文件 相对/绝对 路径
//  返回：Conf 结构体
Conf getConf(std::string filename){
    std::fstream fin;
    fin.open(filename,std::ios::in);
    Conf conf;
    char line[255];
    char *token;
    conf.partNum=0;
    while(!fin.eof())
    {
        fin.getline(line,255);
        if(line[0]=='!'){
            continue;
        }
        //printf("%s\n",line);
        token=line+17;
        if(strspn(line,"mode")==4)  //mode
        {
            conf.isCoor=(line[5]=='c');
        }
        else if(line[0]=='c')       //coordinator_info
        {
            char* p;
            p = strsep(&token, ":");
            std::string ip(p);
            conf.coorIp=ip;
            p = strsep(&token, ":");
            conf.coorPort=atoi(p);
            //std::cout<<conf.coorIp<<" "<<conf.coorPort<<"\n";
        }
        else                        //participant_info
        {
            conf.partNum++;
            char* p;
            p = strsep(&token, ":");
            std::string ip(p);
            Part tmp;
            tmp.ip=ip;
            p = strsep(&token, ":");
            tmp.port=atoi(p);
            conf.part.push_back(tmp);
            //std::cout<<tmp.ip<<" "<<tmp.port<<"\n";
        }
    }
    fin.close();
    return conf;
}

//  判断文件是否存在
//  True    存在
//  False   不存在
bool isExistConf(const std::string& name) {
    return ( access( name.c_str(), F_OK ) != -1 );
}
