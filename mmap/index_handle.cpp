#include "common.h"
#include "index_handle.h"

namespace qiniu
{
    namespace largefile
    {
        IndexHandle::IndexHandle(const std::string & base_path, const uint32_t main_block_id)
        {
            //获取索引文件路径
            std::stringstream temp_stream;  //临时字符串流
            temp_stream << base_path << INDEX_DIR_PREFIX << main_block_id;   //例：传入的base_path为/root/kyrie，后面拼上/index/
            std::string index_path;
            temp_stream >> index_path;

            //创建MMapFileOperation实例
            this->mmap_file_op = new MMapFileOperation(index_path);
            this->is_load = false;
        }

        IndexHandle::~IndexHandle()
        {
            if(this->mmap_file_op != NULL)
            {
                delete this->mmap_file_op;
                this->mmap_file_op = NULL;
            }
        }

        //获取索引文件中indexheader的数据
        IndexHeader * IndexHandle::get_index_header()
        {
            return reinterpret_cast<IndexHeader *>(this->mmap_file_op->get_map_data());
        }

        //创建索引文件，初始化，并映射到内存
        int IndexHandle::create(const uint32_t logic_block_id, const int32_t bucket_size, const MMapOption & mmap_option)
        {
            int ret = 0;

            if(this->is_load)
            {
                return EXIT_INDEX_ALREADY_LOAD_ERROR;
            }

            int64_t file_size = this->mmap_file_op->get_file_size();
            if(file_size < 0)
            {
                return OP_ERROR;
            }
            else if(file_size == 0) //说明是个空文件，作初始化
            {
                //初始化索引头部
                IndexHeader index_header;
                index_header.block_info.block_id = logic_block_id;
                index_header.block_info.seq_no = 1; //下一个可用的文件编号从1开始
                index_header.bucket_size = bucket_size;
                
                //整个块的头部大小 + 用来存真正块的索引桶，之后才是真正的块
                index_header.index_file_size = sizeof(IndexHeader) + sizeof(int32_t)*bucket_size;

                //创建一块缓存，包含index header和total buckets
                //类型是char是因为写进文件的类型是char
                char * init_data = new char[index_header.index_file_size];  //创建一个大小为头部大小 + 所有哈希桶的内存，目的就是用来存放头部和桶
                memcpy(init_data,&index_header,sizeof(IndexHeader));    //先将index_header放进来
                memset(init_data+sizeof(IndexHeader),0,sizeof(int32_t)*bucket_size);    //再将哈希桶放进来，起始位置是init_data + IndexHeader大小

                //将这块缓存写进索引文件（放进块的头部）
                ret = this->mmap_file_op->pwrite_file(init_data,index_header.index_file_size,0);

                delete[] init_data;
                init_data = NULL;

                if(ret != OP_SUCCESS)
                {
                    return ret; //将错误信息返回，让main函数调用错误打印将-ret打印出来
                }


                ret = this->mmap_file_op->flush_file(); //将内存文件立即同步到磁盘，成功返回0，失败返回-1
                if(ret != OP_SUCCESS)
                {
                    return ret;
                }
            }
            else    //索引文件本身就存在，导致create失败
            {
                return EXIT_META_UNEXPECT_FOUND_ERROR;
            }

            //将创建好的索引文件映射到内存
            ret = this->mmap_file_op->mmap_file(mmap_option);
            if(ret != OP_SUCCESS)
            {
                return ret;
            }

            //映射成功后说明索引文件已经加载
            this->is_load = true;

            return OP_SUCCESS;
        }
    }
}