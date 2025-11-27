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
            //拿到这块内存的起始地址后，强制转成IndexHeader *类型就会直接拿到这块内存中的IndexHeader *数据（因为IndexHeader数据就在这块内存的起始位置）
            return reinterpret_cast<IndexHeader *>(this->mmap_file_op->get_map_data());
        }

        //获取索引头部中BlockInfo的数据
        BlockInfo * IndexHandle::get_block_info()
        {
            //拿到这块内存的起始地址后，强制转成BlockInfo *类型就会直接拿到这块内存中的BlockInfo *数据（因为BlockInfo数据就在这块内存的起始位置）
            return reinterpret_cast<BlockInfo *>(this->mmap_file_op->get_map_data());
        }

        //获取哈希桶的数量
        int32_t IndexHandle::get_bucket_size() const
        {
            //转成IndexHeader *后直接去拿该结构体里面的bucket_size
            return reinterpret_cast<IndexHeader *>(this->mmap_file_op->get_map_data())->bucket_size;
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

        //加载索引文件
        int IndexHandle::load(const uint32_t logic_block_id, const int32_t bucket_size, const MMapOption & mmap_option)
        {
            int ret = 0;

            if(this->is_load)
            {
                printf("index load failed. index file is already loaded.\n");
                return EXIT_INDEX_ALREADY_LOAD_ERROR;
            }

            int64_t file_size = this->mmap_file_op->get_file_size();
            if(file_size <0)
            {
                return file_size;   //打开失败会返回-1，且get_file_size内部会打印错误信息
            }
            else if(file_size == 0) //说明是个空文件，不合理，应该先create
            {
                printf("index file load failed. file_size = 0, index file destroyed.\n");
                return EXIT_INDEX_DESTROY_ERROR;
            }

            MMapOption temp_mmap_option = mmap_option;  //因为传进来的mmap_option是const类型，因此用一个临时变量接收，以便修改其中的参数

            //如果当前要加载的索引文件的大小大于初次要映射的内存大小，且小于最大映射大小时，将初次映射的内存大小改为file_size，使得文件能够映射
            if(file_size > temp_mmap_option.first_mmap_size && file_size <= temp_mmap_option.max_mmap_size)
            {
                temp_mmap_option.first_mmap_size = file_size;
            }
            //如果文件超过了max_size，则在mmap_file_op中的pread和pwrite都定义了不在内存映射中读，而是直接读磁盘

            ret = this->mmap_file_op->mmap_file(temp_mmap_option);
            if(ret < 0)
            {
                return ret;
            }

            //因为在create的时候已经给block_id和bucket_size赋值了，因此如果此时还为0则文件异常
            if(this->get_block_info()->block_id == 0 || this->get_bucket_size() == 0)
            {
                fprintf(stderr,"load index file failed. block_id and bucket_size = 0, file destroyed.\n");
                return EXIT_INDEX_DESTROY_ERROR;
            }

            //检查文件大小，不能小于IndexHeader和哈希桶加起来的大小
            if(file_size < sizeof(IndexHeader) + bucket_size*sizeof(int32_t))
            {
                fprintf(stderr,"failed in IndexHandle::load(). file_size < sizeof(IndexHeader) + \\
                        bucket_size*sizeof(int32_t), file_size: %d, index_file_size: %d\n",
                        file_size,sizeof(IndexHeader) + bucket_size*sizeof(int32_t));

                return EXIT_INDEX_DESTROY_ERROR;
            }

            //检查传入的块id和索引文件中的是否相等
            if(logic_block_id != this->get_block_info()->block_id)
            {
                fprintf(stderr,"failed in IndexHandle::load(). logic_block_id != this->get_block_info()->block_id, \\
                        logic_block_id: %d, this->get_block_info()->block_id: %d\n",
                        logic_block_id,this->get_block_info()->block_id);

                return EXIT_BLOCKID_CONFLICT_ERROR;
            }

            //检查传入的bucket_size和索引文件中的是否相等
            if(bucket_size != this->get_bucket_size())
            {
                fprintf(stderr,"failed in IndexHandle::load(). bucket_size != this->get_bucket_size(), \\
                        bucket_size: %d, this->get_bucket_size(): %d\n",
                        bucket_size,this->get_bucket_size());

                return EXIT_BUCKET_CONFLICT_ERROR;
            }

            this->is_load = true;
            printf("index file load success!\n");

            return OP_SUCCESS;
        }



    }
}