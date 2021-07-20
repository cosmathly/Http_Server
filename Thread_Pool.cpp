#include "Thread_Pool.h"
mylock::mylock() { pthread_mutex_init(&mymutex, nullptr); }
mylock::~mylock() { pthread_mutex_destroy(&mymutex); }
void mylock::lock() { pthread_mutex_lock(&mymutex); }
void mylock::unlock() { pthread_mutex_unlock(&mymutex); }
std::queue<int> Thread_Pool::task_queue = { };
mylock Thread_Pool::lock_of_task_queue = { };
mysem::mysem(int value = 0) { sem_init(&sem, 0, value); }
mysem::~mysem() { sem_destroy(&sem); }
void mysem::wait() { sem_wait(&sem); }
void mysem::post() { sem_post(&sem); }
mysem Thread_Pool::sem_for_task_num = {0};
bool Thread_Pool::whether_stop = {false};
Thread_Pool::Thread_Pool(int _thread_num, int _max_task_num):thread_num(_thread_num), max_task_num(_max_task_num)
{
     if(thread_num<=0||max_task_num<=0) throw std::exception();
     all_thread = new pthread_t[thread_num];
     if(all_thread==nullptr) throw std::exception();
     for(int i = 0; i < thread_num; ++i)
     {
        int ret = pthread_create(all_thread+i, nullptr, work, this);
        if(ret!=0)
        {
             delete [] all_thread;
             std::cout<<strerror(ret)<<std::endl;
             exit(-1);
        }
        ret = pthread_detach(all_thread[i]);
        if(ret!=0) 
        {
             delete [] all_thread;
             std::cout<<strerror(ret)<<std::endl;
             exit(-1);
        }
     }
}
Thread_Pool::~Thread_Pool() { delete [] all_thread; whether_stop = true; }
void Thread_Pool::add_task(int task_sockfd)
{
     lock_of_task_queue.lock();
     if(task_queue.size()==max_task_num) 
     { 
       lock_of_task_queue.unlock(); 
       all_client[task_sockfd].close_conn(); 
       return ;
     }
     task_queue.push(task_sockfd);
     sem_for_task_num.post();
     lock_of_task_queue.unlock();
}
void * Thread_Pool::work(void *arg)
{
       Thread_Pool *thread_pool = (Thread_Pool *)arg;
       Parse_Info *parse_info = new Parse_Info;
       int task_sockfd;
       Parse_Sta_Code parse_sta_code;
       while(whether_stop==false)
       {
            lock_of_task_queue.lock();
            sem_for_task_num.wait();
            task_sockfd = task_queue.front();
            task_queue.pop();
            lock_of_task_queue.unlock();
            parse_sta_code = thread_pool->http_parse(task_sockfd, parse_info);
            all_client[task_sockfd].read_in_content_len = 0;
            thread_pool->http_response(task_sockfd, (const Parse_Info *)parse_info, parse_sta_code);
       }
       delete parse_info;
       return thread_pool;
}
Parse_Sta_Code Thread_Pool::http_parse_line(int &cur_idx, int to_parse_sockfd, Parse_Info *parse_info)
{
      
}
Parse_Sta_Code Thread_Pool::http_parse_header(int &cur_idx, int to_parse_sockfd, Parse_Info *parse_info)
{

}
Parse_Sta_Code Thread_Pool::http_parse_empty_line(int &cur_idx, int to_parse_sockfd)
{

}
Parse_Sta_Code Thread_Pool::http_parse_body(int &cur_idx, int to_parse_sockfd, Parse_Info *parse_info)
{
      
}
Parse_Sta_Code Thread_Pool::http_parse(int to_parse_sockfd, Parse_Info *parse_info)
{
     int cur_idx = 0;
     Parse_Sta cur_parse_sta = parse_line;
     Parse_Sta_Code cur_parse_sta_code;
     while(cur_parse_sta!=parse_end)
     {
          switch(cur_parse_sta)
          {
               case parse_line:
               cur_parse_sta_code = http_parse_line(cur_idx, to_parse_sockfd, parse_info);
               if(cur_parse_sta_code!=OK) return cur_parse_sta_code;
               else cur_parse_sta = parse_header;
               break;
               case parse_header:
               cur_parse_sta_code = http_parse_header(cur_idx, to_parse_sockfd, parse_info);
               if(cur_parse_sta_code!=OK) return cur_parse_sta_code;
               else cur_parse_sta = parse_empty_line;
               break;
               case parse_empty_line:
               cur_parse_sta_code = http_parse_empty_line(cur_idx, to_parse_sockfd);
               if(cur_parse_sta_code!=OK) return cur_parse_sta_code;
               else cur_parse_sta = parse_body;
               break;
               case parse_body:
               cur_parse_sta_code = http_parse_body(cur_idx, to_parse_sockfd, parse_info);
               if(cur_parse_sta_code!=OK) return cur_parse_sta_code;
               else cur_parse_sta = parse_end;
               break;
          }
     }
     return OK;
}