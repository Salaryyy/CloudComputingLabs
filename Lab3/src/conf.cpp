#include "conf.h"

//  读取配置文件
//  参数：文件 相对/绝对 路径
//  返回：Conf 结构体
Conf getConf(std::string filename){
    std::fstream fin;
    fin.open(filename, std::ios::in);
    Conf conf;
    char line[255];
    char *token;
    conf.partNum = 0;
    while(!fin.eof())
    {
        fin.getline(line,255);
        if(strlen(line)==0||line[0] == '!'){
            continue;
        }
        //printf("%s\n",line);
        token = line + 17;
        if(strspn(line, "mode") == 4)  //mode
        {
            conf.isCoor = (line[5] == 'c');
        }
        else if(line[0] == 'c')       //coordinator_info
        {
            char* p;
            p = strsep(&token, ":");
            std::string ip(p);
            conf.coorIp = ip;
            p = strsep(&token, ":");
            conf.coorPort = atoi(p);
            //std::cout<<conf.coorIp<<" "<<conf.coorPort<<"\n";
        }
        else                        //participant_info
        {
            conf.partNum++;
            char* p;
            p = strsep(&token, ":");
            std::string ip(p);
            Part tmp;
            tmp.ip = ip;
            p = strsep(&token, ":");
            tmp.port = atoi(p);
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

std::string getOptConf(int argc, char **argv)
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
            //printf("config_path optarg = %s\n", optarg); // 参数内容
            //printf("\n");
            name = optarg;
        }
    }
    return name;
}