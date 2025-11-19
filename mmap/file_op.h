#ifndef FILE_OP_H
#define FILE_OP_H

#include<unistd.h>
#include<sys/mman.h>
#include<cerrno>
#include<cstring>
#include <sys/stat.h>
#include <aio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string>

namespace qiniu
{
    namespace largefile
    {
        class FileOperation
        {
        public:
            FileOperation(const std::string& file_name, const int open_flag = O_RDWR | O_LARGEFILE){}
            ~FileOperation(){}

            //打开和关闭文件
            int open_file();
            void close_file();

            //将文件立即写入磁盘
            int flush_file();

            //删除文件
            int unlink_file();

            //读文件
            int pread_file(char *buf, const int32_t size, const int64_t offset);    //buf：将文件读到缓冲区 size：读文件的长度    offset：文件偏移量，读的位置

            //写文件
            int pwrite_file(const char* buf, const int32_t size, const int64_t offset); //将buf写入文件，buf不能修改

            //直接提供偏移量，从当前位置开始写，不需要再传偏移量
            int write_file(const char* buf, const int32_t size);

            //获取文件大小
            int64_t get_file_size();

            //文件截断，减少文件大小
            int ftruncate_file(int64_t size);   //size：要截断的大小

            //设置文件偏移量
            int seek_file(int64_t offset);

            //获取文件描述符
            int get_fd();

        private:
            int fd; //文件描述符
            int64_t size;   //文件大小
            std::string file_name;  //文件名
            int open_flag;  //文件打开标志

        protected:
            static const mode_t open_mode = 0644;   //文件权限
            static const int max_disk = 5;  //最大访问磁盘的次数，防止偶尔失败后仍频繁读写导致系统崩溃

        };
    }
}




#endif