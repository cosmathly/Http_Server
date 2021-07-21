#include "Thread_Pool.h"
const char *error_400 = "HTTP request can not been parsed by server";
const char *error_403 = "Server do not provide this kind of http request";
const char *error_404 = "Resource can not been found";
const char *doc_path = "/home/cosmathly/Linux/http_server/resources";
extern client *all_client;
extern myepoll epoll;
mylock::mylock() { pthread_mutex_init(&mymutex, nullptr); }
mylock::~mylock() { pthread_mutex_destroy(&mymutex); }
void mylock::lock() { pthread_mutex_lock(&mymutex); }
int mylock::trylock() { return pthread_mutex_trylock(&mymutex); }
void mylock::unlock() { pthread_mutex_unlock(&mymutex); }
mylock Thread_Pool::lock_of_task_queue = { };
mysem::mysem() { sem_init(&sem, 0, 0); }
mysem::~mysem() { sem_destroy(&sem); }
int mysem::wait() { return sem_wait(&sem); }
int mysem::post() { return sem_post(&sem); }
mysem Thread_Pool::sem_for_task_num = {};
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
       all_client[task_sockfd].close_conn(true); 
       return ;
     }
     task_queue.push(task_sockfd);
     lock_of_task_queue.unlock();
     //sem_for_task_num.post();
}
void * Thread_Pool::work(void *arg)
{
       Thread_Pool *thread_pool = (Thread_Pool *)arg;
       Parse_Info *parse_info = new Parse_Info;
       int task_sockfd;
       Parse_Sta_Code parse_sta_code;
       while(whether_stop==false)
       {   
            //sem_for_task_num.wait();
            lock_of_task_queue.lock();
            if(thread_pool->task_queue.empty()==true)
            {
              lock_of_task_queue.unlock();
              continue;
            }
            task_sockfd = thread_pool->task_queue.front();
            thread_pool->task_queue.pop();
            lock_of_task_queue.unlock();
            parse_sta_code = thread_pool->http_parse(task_sockfd, parse_info);
            thread_pool->http_response(task_sockfd, (const Parse_Info *)parse_info, parse_sta_code);
            epoll.mod_fd(task_sockfd, EPOLLOUT);
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
     if(strcasecmp(read_str+tmp_idx, "/index.html")!=0&&strcasecmp(read_str+tmp_idx, "/images/image1.jpg")!=0) return Not_Found;
     parse_info->request_content = (char *)((strcasecmp(read_str+tmp_idx, "/index.html")==0)?("/index.html"):("/images/image1.jpg"));
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
     all_client[to_parse_sockfd].read_in_content_len = 0;
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
void Thread_Pool::http_response_header_content_length(int &cur_idx, int to_response_sockfd, const Parse_Info *parse_info, Parse_Sta_Code parse_sta_code)
{
     char *write_str = all_client[to_response_sockfd].write_buf;
     char *response_content_length = new char[50];
     char *file_path = nullptr;
     struct stat file_info;
     switch(parse_sta_code)
     {
          case OK:
          file_path = new char[strlen(doc_path)+strlen(parse_info->request_content)+1];
          file_path[strlen(doc_path)+strlen(parse_info->request_content)] = '\0';
          sprintf(file_path, "%s%s", doc_path, parse_info->request_content);
          stat((const char *)file_path, &file_info);
          delete [] file_path;
          sprintf(response_content_length, "Content-Length: %ld\r\n", file_info.st_size);
          break;
          case Bad_Request:
          sprintf(response_content_length, "Content-Length: %d\r\n", strlen(error_400));
          break;
          case Forbidden:
          sprintf(response_content_length, "Content-Length: %d\r\n", strlen(error_403));
          break;
          case Not_Found:
          sprintf(response_content_length, "Content-Length: %d\r\n", strlen(error_404));
          break;
     }
     add_response(write_str+cur_idx, response_content_length);
     cur_idx += strlen(response_content_length);
     delete [] response_content_length;
}
void Thread_Pool::http_response_header_content_type(int &cur_idx, int to_response_sockfd)
{
     char *write_str = all_client[to_response_sockfd].write_buf;
     char *response_content_type = new char[50];
     sprintf(response_content_type, "Content-Type: text/html\r\n");
     add_response(write_str+cur_idx, response_content_type);
     cur_idx += strlen(response_content_type);
     delete [] response_content_type;
}
void Thread_Pool::http_response_header_connection(int &cur_idx, int to_response_sockfd)
{
     char *write_str = all_client[to_response_sockfd].write_buf;
     bool whether_keep_alive = all_client[to_response_sockfd].whether_keep_alive;
     char *response_connection = new char[50];   
     sprintf(response_connection, "Connection: %s\r\n", (whether_keep_alive==true)?("keep-alive"):("close"));
     add_response(write_str+cur_idx, response_connection);
     cur_idx += strlen(response_connection);
     delete [] response_connection;
}
void Thread_Pool::http_response_header(int &cur_idx, int to_response_sockfd, const Parse_Info *parse_info, Parse_Sta_Code parse_sta_code)
{
     http_response_header_content_length(cur_idx, to_response_sockfd, parse_info, parse_sta_code);
     http_response_header_content_type(cur_idx, to_response_sockfd);
     http_response_header_connection(cur_idx, to_response_sockfd);
}
void Thread_Pool::http_response_empty_line(int &cur_idx, int to_response_sockfd)
{
     char *write_str = all_client[to_response_sockfd].write_buf;
     add_response(write_str+cur_idx, "\r\n");
     cur_idx += 2;
}
void Thread_Pool::http_response_body(int &cur_idx, int to_response_sockfd, const Parse_Info *parse_info, Parse_Sta_Code parse_sta_code)
{
     char *write_str = all_client[to_response_sockfd].write_buf;
     char *file_path = nullptr;
     int fd = {-1};
     struct stat file_info;
     switch(parse_sta_code)
     {
          case OK:
          file_path = new char[strlen(doc_path)+strlen(parse_info->request_content)+1];
          file_path[strlen(doc_path)+strlen(parse_info->request_content)] = '\0';
          sprintf(file_path, "%s%s", doc_path, parse_info->request_content);
          fd = open((const char *)file_path, O_RDONLY);
          stat((const char *)file_path, &file_info);
          read(fd, (void *)(write_str+cur_idx), file_info.st_size);
          cur_idx += file_info.st_size;
          delete [] file_path;
          break;
          case Bad_Request:
          add_response(write_str+cur_idx, error_400);
          cur_idx += strlen(error_400);
          break;
          case Forbidden:
          add_response(write_str+cur_idx, error_403);
          cur_idx += strlen(error_403);
          break;
          case Not_Found:
          add_response(write_str+cur_idx, error_404);
          cur_idx += strlen(error_404);
          break;
     }
}
void Thread_Pool::http_response(int to_response_sockfd, const Parse_Info *parse_info, Parse_Sta_Code parse_sta_code)
{
     if(all_client[to_response_sockfd].write_buf==nullptr) 
     all_client[to_response_sockfd].write_buf = new char[all_client[to_response_sockfd].write_buf_size];
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
     all_client[to_response_sockfd].write_out_content_len = cur_idx;
     all_client[to_response_sockfd].have_write_content_len = 0;
}