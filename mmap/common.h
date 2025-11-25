#ifndef COMMON_H
#define COMMON_H

#include<unistd.h>
#include<sys/mman.h>
#include<cerrno>
#include<cstring>
#include <sys/stat.h>
#include <aio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string>
#include <stdio.h>

namespace qiniu
{
    namespace largefile
    {
        const int32_t OP_SUCCESS = 0; //操作成功
        const int32_t OP_ERROR = -1;   //操作失败
        const int32_t EXIT_DISK_OP_INCOMPLETE = -8012;  //读写长度小于要求的
    }
}


#endif