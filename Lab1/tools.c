#include "tools.h"
#include "stdio.h"
#include "string.h"

int getCpuInfo()  
{  
    FILE *fp = fopen("/proc/cpuinfo", "r");    
    if(NULL == fp)     
        printf("failed to open cpuinfo\n");     
    char szTest[1000] = {0};    
    while(!feof(fp))    
    {    
        memset(szTest, 0, sizeof(szTest));    
        fgets(szTest, sizeof(szTest) - 1, fp); 
        //printf("%s", szTest);
        if(strstr(szTest,"siblings")!=NULL){
            fclose(fp);
            //std::cout << szTest[strlen(szTest) - 1] << std::endl;
            return szTest[strchr(szTest,':') - szTest+2] - '0';
        } 
    }    
    return 0;
}

void debug_tool(const char a[]){
    printf("------%s-----\n",a);
}