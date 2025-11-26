#ifndef INDEX_HANDLE_H
#define INDEX_HANDLE_H

#include "common.h"
#include "mmap_file.h"
#include "file_op.h"
#include "mmap_file_op.h"

namespace qiniu
{
    namespace largefile
    {
        //索引头部
        struct IndexHeader
        {
            IndexHeader()
            {
                memset(this,0,sizeof(IndexHeader));
            }


            BlockInfo block_info;   //块信息
            int32_t bucket_size;    //哈希桶的数量
            int32_t data_file_offset;    //数据文件的起始位置
            int32_t index_file_size;    //下一次要写入的位置（偏移量）：初始index_header + all buckets
            int32_t free_head_offset;   //可重用地址：之前用过的某个块如果删掉了，则用一个地址保存，下次可以重用（即空闲链表节点的地址）
        };

        //索引操作
        class IndexHandle
        {
        public:
            IndexHandle(const std::string & base_path, const uint32_t block_id);
            ~IndexHandle();

            //获取索引文件中indexheader的数据
            IndexHeader * get_index_header();

            //创建索引文件，初始化，并映射到内存
            int create(const uint32_t logic_block_id, const int32_t bucket_size, const MMapOption & mmap_option);
            //参数：logic_block_id：和主块id是一个意思  bucket_size：哈希桶大小 mmap_option：用于构造MMapFile

        private:
            MMapFileOperation *mmap_file_op;
            bool is_load;   //索引文件是否被加载
        };
    }
}





#endif