#ifndef _Thread_Pool_
#define _Thread_Pool_
#include <queue>
#include <sys/mman.h>
#include <pthread.h>
#include <semaphore.h>
#include <iostream>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
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
        int trylock();
        void unlock();
};
class mysem
{
      public:
        sem_t sem;
      public:
        mysem();
        ~mysem();
        int wait();
        int post();
};
class Parse_Info
{
      public:
        char *request_content = {nullptr};
        bool whether_keep_alive = {false};
        int content_length = {-1};
};
extern const char *doc_path;
enum Response_Sta {response_line, response_header, response_empty_line, response_body, response_end};
enum Parse_Sta {parse_line, parse_header, parse_empty_line, parse_body, parse_end};
enum Parse_Sta_Code {OK=200, Bad_Request=400, Forbidden=403, Not_Found=404};
extern const char *error_400;
extern const char *error_403;
extern const char *error_404;
class Thread_Pool;
class Thread_Pool
{
      private:
        static mylock lock_of_task_queue;
        static mysem sem_for_task_num;
        std::queue<int> task_queue;
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
        void add_response(char *add_pos, const char *add_content);
        void http_response(int to_response_sockfd, const Parse_Info *parse_info, Parse_Sta_Code parse_sta_code);
        void http_response_line(int &cur_idx, int to_response_sockfd, Parse_Sta_Code parse_sta_code);
        void http_response_header(int &cur_idx, int to_response_sockfd, const Parse_Info *parse_info, Parse_Sta_Code parse_sta_code);
        void http_response_header_content_length(int &cur_idx, int to_response_sockfd, const Parse_Info *parse_info, Parse_Sta_Code parse_sta_code);
        void http_response_header_content_type(int &cur_idx, int to_response_sockfd);
        void http_response_header_connection(int &cur_idx, int to_response_sockfd);
        void http_response_empty_line(int &cur_idx, int to_response_sockfd);
        void http_response_body(int &cur_idx, int to_response_sockfd, const Parse_Info *parse_info, Parse_Sta_Code parse_sta_code);
        static void * work(void *arg);
};
#endif