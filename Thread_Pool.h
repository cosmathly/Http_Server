#ifndef _Thread_Pool_
#define _Thread_Pool_
#include <queue>
#include <pthread.h>
#include <semaphore.h>
#include <iostream>
#include <string.h>
#include "Reactor.h"
class mylock
{
      private:
        pthread_mutex_t mymutex;
      public:
        mylock();
        ~mylock();
        void lock();
        void unlock();
};
class mysem
{
      private:
        sem_t sem;
      public:
        mysem(int value = 0);
        ~mysem();
        void wait();
        void post();
};
class Parse_Info
{

};
enum Parse_Sta {parse_line, parse_header, parse_empty_line, parse_body, parse_end};
enum Parse_Sta_Code {OK=200, Bad_Request=400, Forbidden=403, Not_Found=404};
class Thread_Pool
{
      private:
        static mylock lock_of_task_queue;
        static mysem sem_for_task_num;
        static std::queue<int> task_queue;
        int max_task_num = {10000};
        int thread_num = {4};
        static bool whether_stop;
        pthread_t *all_thread = {nullptr};
      public: 
        void add_task(int task_sockfd);
        Thread_Pool(int _thread_num, int _max_task_num);
        ~Thread_Pool();
        Parse_Sta_Code http_parse(int to_parse_sockfd, Parse_Info *parse_info);
        Parse_Sta_Code http_parse_line(int &cur_idx, int to_parse_sockfd, Parse_Info *parse_info);
        Parse_Sta_Code http_parse_header(int &cur_idx, int to_parse_sockfd, Parse_Info *parse_info);
        Parse_Sta_Code http_parse_empty_line(int &cur_idx, int to_parse_sockfd);
        Parse_Sta_Code http_parse_body(int &cur_idx, int to_parse_sockfd, Parse_Info *parse_info);
        void http_response(int to_response_sockfd, const Parse_Info *parse_info, Parse_Sta_Code parse_sta_code);
        static void * work(void *arg);
};
#endif