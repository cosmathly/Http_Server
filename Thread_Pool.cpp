#include "Thread_Pool.h"
const char *error_400 = "HTTP request can not been parsed by server";
const char *error_403 = "Server do not provide this kind of http request";
const char *error_404 = "Resource can not been found";
const char *doc_path = "/home/cosmathly/Linux/http_server/resources";
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
     int tmp_idx = {-1};
     char *read_str = all_client[to_parse_sockfd].read_buf;
     int read_str_len = all_client[to_parse_sockfd].read_in_content_len;
     while(cur_idx<read_str_len&&read_str[cur_idx]!=' ') ++cur_idx;
     if(cur_idx==read_str_len) return Bad_Request;
     read_str[cur_idx] = '\0';
     if(strcasecmp(read_str, "GET")!=0) 
     {
        if(strcasecmp(read_str, "POST")==0) return Forbidden;
        else return Bad_Request;
     }
     ++cur_idx;
     tmp_idx = cur_idx;
     while(cur_idx<read_str_len&&read_str[cur_idx]!=' ') ++cur_idx;
     if(cur_idx==read_str_len) return Bad_Request;
     read_str[cur_idx] = '\0';
     for(int i = cur_idx-1; i >= tmp_idx; --i)
     if(read_str[i]=='/') { tmp_idx = i; break; }
     if(strcasecmp(read_str+tmp_idx, "/index.html")!=0) return Not_Found;
     parse_info->request_content = "/index.html";
     ++cur_idx;
     tmp_idx = cur_idx;
     while(cur_idx<read_str_len&&read_str[cur_idx]!='\r') ++cur_idx;
     if(cur_idx==read_str_len) return Bad_Request;
     read_str[cur_idx] = '\0';
     if(strcasecmp(read_str+tmp_idx, "HTTP/1.1")!=0) return Forbidden;
     cur_idx += 2;
     return OK;
}
Parse_Sta_Code Thread_Pool::http_parse_header(int &cur_idx, int to_parse_sockfd, Parse_Info *parse_info)
{
     int tmp_idx = {-1};
     char *read_str = all_client[to_parse_sockfd].read_buf;
     int read_str_len = all_client[to_parse_sockfd].read_in_content_len;
     while(true)
     {
        if(read_str[cur_idx]=='\r') return OK; 
        tmp_idx = cur_idx;
        while(cur_idx<read_str_len&&read_str[cur_idx]!=':') ++cur_idx;
        if(cur_idx==read_str_len) return Bad_Request;
        read_str[cur_idx] = '\0';
        if(strcasecmp(read_str+tmp_idx, "Connection")==0)
        {
          ++cur_idx;
          while(cur_idx<read_str_len&&read_str[cur_idx]==' ') ++cur_idx;
          if(cur_idx==read_str_len) return Bad_Request;
          tmp_idx = cur_idx;
          while(cur_idx<read_str_len&&read_str[cur_idx]!='\r') ++cur_idx;
          if(cur_idx==read_str_len) return Bad_Request;
          read_str[cur_idx] = '\0';
          if(strcasecmp(read_str+tmp_idx, "keep-alive")==0) 
          {
            parse_info->whether_keep_alive = true;
            all_client[to_parse_sockfd].whether_keep_alive = true;
          }
          cur_idx += 2;
          continue;
        }
        else if(strcasecmp(read_str+tmp_idx, "Content-Length")==0)
        {
             ++cur_idx;
             while(cur_idx<read_str_len&&read_str[cur_idx]==' ') ++cur_idx;
             if(cur_idx==read_str_len) return Bad_Request;
             tmp_idx = cur_idx;
             while(cur_idx<read_str_len&&read_str[cur_idx]!='\r') ++cur_idx;
             if(cur_idx==read_str_len) return Bad_Request;
             read_str[cur_idx] = '\0';
             parse_info->content_length = atoi(read_str+tmp_idx);
             cur_idx += 2;
             continue;
        }
        else 
        {
             while(cur_idx<read_str_len&&read_str[cur_idx]!='\r') ++cur_idx;
             if(cur_idx==read_str_len) return Bad_Request;
             cur_idx += 2;
             continue;
        }
     }
}
Parse_Sta_Code Thread_Pool::http_parse_empty_line(int &cur_idx, int to_parse_sockfd)
{
     if(all_client[to_parse_sockfd].read_buf[cur_idx+1]!='\n') return Bad_Request;
     else { cur_idx += 2; return OK; }
}
Parse_Sta_Code Thread_Pool::http_parse_body(int &cur_idx, int to_parse_sockfd, Parse_Info *parse_info)
{
     int read_str_len = all_client[to_parse_sockfd].read_in_content_len;
     if(cur_idx==read_str_len) return OK;
     else if(read_str_len-cur_idx<parse_info->content_length) return Bad_Request;
     else return OK;
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
void Thread_Pool::add_response(char *add_pos, const char *add_content)
{
     if(add_pos==nullptr||add_content==nullptr) throw std::exception();
     while((*add_content)!='\0') { (*add_pos) = (*add_content); ++add_pos; ++add_content; }
}
void Thread_Pool::http_response_line(int &cur_idx, int to_response_sockfd, Parse_Sta_Code parse_sta_code)
{
     char *write_str = all_client[to_response_sockfd].write_buf;
     add_response(write_str, "HTTP/1.1 ");
     cur_idx += strlen("HTTP/1.1 ");
     switch(parse_sta_code)
     {
          case OK:
          add_response(write_str+cur_idx, "200 OK\r\n");
          cur_idx += strlen("200 OK\r\n");
          break;
          case Bad_Request:
          add_response(write_str+cur_idx, "400 Bad Request\r\n");
          cur_idx += strlen("400 Bad Request\r\n");
          break;
          case Forbidden:
          add_response(write_str+cur_idx, "403 Forbidden\r\n");
          cur_idx += strlen("403 Forbidden\r\n");
          break;
          case Not_Found:
          add_response(write_str+cur_idx, "404 Not Found\r\n");
          cur_idx += strlen("404 Not Found\r\n");
          break;
     }
}
void Thread_Pool::http_response_header(int &cur_idx,int to_response_sockfd, const Parse_Info *parse_info, Parse_Sta_Code parse_sta_code)
{
     char *file_path = ()
}
void Thread_Pool::http_response(int to_response_sockfd, const Parse_Info *parse_info, Parse_Sta_Code parse_sta_code)
{
     if(all_client[to_response_sockfd].write_buf==nullptr) 
     all_client[to_response_sockfd].write_buf = new char[all_client[to_response_sockfd].write_buf_size];
     all_client[to_response_sockfd].write_out_content_len = 0;
     all_client[to_response_sockfd].have_write_content_len = 0;
     Response_Sta cur_response_sta = response_line;
     int cur_idx = 0;
     while(cur_response_sta!=response_end)
     {
          switch(cur_response_sta)
          {
               case response_line:
               http_response_line(cur_idx, to_response_sockfd, parse_sta_code);
               cur_response_sta = response_header;
               break;
               case response_header: 
               http_response_header(cur_idx, to_response_sockfd, parse_info, parse_sta_code);
               cur_response_sta = response_empty_line;
               break;
               case response_empty_line:
               http_response_empty_line(cur_idx, to_response_sockfd);
               cur_response_sta = response_body;
               break;
               case response_body:
               http_response_body(cur_idx, to_response_sockfd, parse_info, parse_sta_code);
               cur_response_sta = response_end;
               break;
          }
     }
}