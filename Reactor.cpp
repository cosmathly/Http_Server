#include "Reactor.h"
myepoll epoll(epoll_create(1));
client *all_client = nullptr;
epoll_event *all_ready_event = nullptr;
int listen_fd = -1;
Thread_Pool thread_pool(4, 10000);
mylock log_lock = {};
Log log = {};
Keep_Alive keep_alive(5);
mylock keep_alive_lock = {};
const int port = {8888};
const int max_event_num = {10000};
const int max_client_num = {65536};
int client::client_num = {0};
const int client::read_buf_size = {2048};
const int client::write_buf_size = {1024};
void set_port_reuse(int fd)
{
     int reuse = {1};
     setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const void *)(&reuse), sizeof(reuse));
}
client::client() { }
client::~client() { delete [] read_buf; delete [] write_buf; }
void client::make_record(Record &cur_record)
{
     time_t cur_time = time(nullptr);
     struct tm *local_cur_time = localtime((const time_t *)(&cur_time));
     char *std_time = new char[60];
     sprintf(std_time, "%d-%d-%d %d:%d:%d ", local_cur_time->tm_year,
     local_cur_time->tm_mon, local_cur_time->tm_mday,
     local_cur_time->tm_hour, local_cur_time->tm_min, 
     local_cur_time->tm_sec);
     int std_time_len = strlen(std_time);
     for(int i = 0; i < std_time_len; ++i)
     cur_record += std_time[i];
     delete [] std_time;
     char *ip_addr = new char[30];
     inet_ntop(AF_INET, (const void *)(&addr.sin_addr.s_addr), ip_addr, (socklen_t)(sizeof(char)*30));
     int ip_addr_len = strlen(ip_addr);
     for(int i = 0; i < ip_addr_len; ++i)
     cur_record += ip_addr[i];
     cur_record += ':';
     delete [] ip_addr;
     uint16_t port_num = ntohs(addr.sin_port);
     char *port_num_str = new char[10];
     sprintf(port_num_str, "%d", port_num);
     int port_num_str_len = strlen(port_num_str);
     for(int i = 0; i < port_num_str_len; ++i)
     cur_record += port_num_str[i]; 
}
void client::init(int sockfd_for_client, const struct sockaddr_in &cur_client_addr)
{
     sockfd = sockfd_for_client;
     addr = cur_client_addr;
     set_port_reuse(sockfd_for_client);
     epoll.add_fd(sockfd_for_client, true);
     keep_alive_lock.lock();
     keep_alive.insert(time(nullptr)+(time_t)5, sockfd);
     keep_alive_lock.unlock();
     log_lock.lock();
     Record cur_record = {};
     make_record(cur_record);
     cur_record += " connection\n";
     log.cur_all_record.push(std::move(cur_record));
     log_lock.unlock();
}
void client::close_conn(bool flag)
{
     --client_num;
     close(sockfd);
     if(read_buf!=nullptr) { delete [] read_buf; read_buf = nullptr; }
     if(write_buf!=nullptr) { delete [] write_buf; write_buf = nullptr; }
     epoll.del_fd(sockfd);     
     if(flag==true)
     {   
        keep_alive_lock.lock();
        keep_alive.del(sockfd);
        keep_alive_lock.unlock();
     }
     log_lock.lock();
     Record cur_record = {};
     make_record(cur_record);
     cur_record += " close\n";
     log.cur_all_record.push(std::move(cur_record));
     log_lock.unlock();
}
bool client::cli_read()
{
     if(read_buf==nullptr) read_buf = new char[read_buf_size];
     size_t bytes_of_read;
     while(true)
     {
         bytes_of_read = read(sockfd, (void *)(read_buf+read_in_content_len), read_buf_size-read_in_content_len);
         if(bytes_of_read==-1)
         {
            if(errno==EAGAIN||errno==EWOULDBLOCK) break; 
            else { close_conn(true); return false; }
         }
         else if(bytes_of_read==0) { close_conn(true); return false; }
         else read_in_content_len += bytes_of_read;
     }
     keep_alive_lock.lock();
     keep_alive.del(sockfd);
     keep_alive_lock.unlock();
     log_lock.lock();
     Record cur_record = {};
     make_record(cur_record);
     cur_record += " epollin\n";
     log.cur_all_record.push(std::move(cur_record));
     log_lock.unlock();
     return true;
}
void client::cli_write()
{
     size_t bytes_of_write;
     while(true)
     {
         bytes_of_write = write(sockfd, (const void *)(write_buf+have_write_content_len), write_out_content_len-have_write_content_len);
         if(bytes_of_write==-1)
         {
            if(errno==EAGAIN||errno==EWOULDBLOCK) 
            {
               epoll.mod_fd(sockfd, EPOLLOUT);
               break;
            }
            else { close_conn(true); break; }
         }
         have_write_content_len += bytes_of_write;
         if(have_write_content_len==write_out_content_len) 
         {
            if(whether_keep_alive==false) { close_conn(false); break ; }
            else 
            { 
               epoll.mod_fd(sockfd, EPOLLIN); 
               keep_alive_lock.lock();
               keep_alive.insert(time(nullptr)+(time_t)5, sockfd);
               keep_alive_lock.unlock();
               break; 
            }
         }
     }
     log_lock.lock();
     Record cur_record = {};
     make_record(cur_record);
     cur_record += " epollout\n";
     log.cur_all_record.push(std::move(cur_record));
     log_lock.unlock();
}
void set_nonblocking(int fd)
{
    int old_flag = fcntl(fd, F_GETFL);
    int new_flag = old_flag|O_NONBLOCK;
    fcntl(fd, F_SETFL, new_flag);
}
myepoll::myepoll(int _epoll_fd_):epoll_fd{_epoll_fd_} { }
myepoll::~myepoll() { }
void myepoll::add_fd(int to_add_fd, bool if_set_epolloneshot)
{
     epoll_event cur_event;
     cur_event.data.fd = to_add_fd;
     cur_event.events = EPOLLIN|EPOLLET|EPOLLRDHUP;
     if(if_set_epolloneshot==true) cur_event.events |= EPOLLONESHOT;
     epoll_ctl(epoll_fd, EPOLL_CTL_ADD, to_add_fd, &cur_event);
     set_nonblocking(to_add_fd);
}
void myepoll::del_fd(int to_del_fd)
{
     epoll_ctl(epoll_fd, EPOLL_CTL_DEL, to_del_fd, nullptr);
     close(to_del_fd);
}
void myepoll::mod_fd(int to_mod_fd, uint32_t new_listen_event)
{
     epoll_event modified_event;
     modified_event.data.fd = to_mod_fd;
     modified_event.events = new_listen_event|EPOLLET|EPOLLRDHUP|EPOLLONESHOT;
     epoll_ctl(epoll_fd, EPOLL_CTL_MOD, to_mod_fd, &modified_event);
}
int myepoll::get_ready_fd(int maxevents, int timeout)
{
    return epoll_wait(epoll_fd, all_ready_event, maxevents, timeout);
}
void change_sigaction(int signum, void (*new_handler)(int))
{
     struct sigaction new_sigaction;
     new_sigaction.sa_handler = new_handler;
     sigfillset(&new_sigaction.sa_mask);
     assert(sigaction(signum, &new_sigaction, nullptr)!=-1);
}
void server_initial()
{
     all_ready_event = new epoll_event[max_event_num];
     all_client = new client[max_client_num];
     change_sigaction(SIGPIPE, SIG_IGN);
     listen_fd = socket(AF_INET, SOCK_STREAM, 0);
     struct sockaddr_in server_addr;
     inet_pton(AF_INET, "172.25.201.42", (void *)(&server_addr.sin_addr.s_addr));
     server_addr.sin_family = AF_INET;
     server_addr.sin_port = htons(port);
     set_port_reuse(listen_fd);
     bind(listen_fd, (const struct sockaddr *)&server_addr, sizeof(server_addr));
     listen(listen_fd, 5);
     epoll.add_fd(listen_fd, false);
}
void server_clean()
{
     close(listen_fd);
     delete [] all_client;
     delete [] all_ready_event;
}
void handle_ready_event(int idx)
{
     int cur_ready_fd = all_ready_event[idx].data.fd;
     uint32_t cur_ready_events = all_ready_event[idx].events;
     if(cur_ready_fd==listen_fd)
     {
       struct sockaddr_in client_addr;
       socklen_t client_addr_size = sizeof(client_addr);
       int sockfd_for_client = accept(listen_fd, (sockaddr *)(&client_addr), &client_addr_size);
       if(sockfd_for_client==-1)
       {
          perror("accept");
          return ;
       }
       if(client::client_num==max_client_num) 
       {
         close(sockfd_for_client);
         return ;
       }
       else 
       {
          ++client::client_num;
          all_client[sockfd_for_client].init(sockfd_for_client, client_addr);
          return ;
       }
     }
     else 
     {
       if(cur_ready_events&(EPOLLERR|EPOLLHUP|EPOLLRDHUP)) all_client[cur_ready_fd].close_conn(true);
       else if(cur_ready_events&EPOLLIN) { if(all_client[cur_ready_fd].cli_read()==true) thread_pool.add_task(cur_ready_fd); }
       else if(cur_ready_events&EPOLLOUT) all_client[cur_ready_fd].cli_write();
     }
}
int main(void)
{
    server_initial();
    while(true)
    {
         int ready_fd_number = epoll.get_ready_fd(max_event_num, -1);
         if(ready_fd_number==-1&&errno!=EINTR)
         {
             perror("epoll_wait");
             break;
         }
         for(int i = 0; i < ready_fd_number; ++i) handle_ready_event(i);
    }
    server_clean();
    return 0;
}