#include <unistd.h>
#include "conf.h"

int main(int argc, char **argv)
{
    Conf conf = getConf(getOptConf(argc, argv));
    if (conf.isCoor)
    {
        execvp("./coordinator",argv);
    }
    else
    {
        execvp("./participant",argv);
    }
    return 0;
}