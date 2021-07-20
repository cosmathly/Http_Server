#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <iostream>
#include <signal.h>
#include <strings.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include "Reactor.h"
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
void client::init(int sockfd_for_client, const struct sockaddr_in &cur_client_addr)
{
     sockfd = sockfd_for_client;
     addr = cur_client_addr;
     set_port_reuse(sockfd_for_client);
     epoll.add_fd(sockfd_for_client, true);
}
void client::close_conn()
{
     --client_num;
     close(sockfd);
     if(read_buf!=nullptr) { delete [] read_buf; read_buf = nullptr; }
     if(write_buf!=nullptr) { delete [] write_buf; write_buf = nullptr; }
     epoll.del_fd(sockfd);    
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
            if(errno==EAGAIN||errno==EWOULDBLOCK) return true;
            else { close_conn(); return false; }
         }
         else if(bytes_of_read==0) { close_conn(); return false; }
         else read_in_content_len += bytes_of_read;
     }
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
            else { close_conn(); break; }
         }
         have_write_content_len += bytes_of_write;
         if(have_write_content_len==write_out_content_len) 
         {
            if(whether_keep_alive==false) { close_conn(); break ; }
            else { epoll.mod_fd(sockfd, EPOLLIN); break; }
         }
     }
}
void set_nonblocking(int fd)
{
    int old_flag = fcntl(fd, F_GETFL);
    int new_flag = old_flag|O_NONBLOCK;
    fcntl(fd, F_SETFL, new_flag);
}
myepoll::myepoll(int _epoll_fd_):epoll_fd{_epoll_fd_} { }
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
     epoll = {epoll_create(1)};
     all_ready_event = new epoll_event[max_event_num];
     all_client = new client[max_client_num];
     change_sigaction(SIGPIPE, SIG_IGN);
     listen_fd = socket(AF_INET, SOCK_STREAM, 0);
     struct sockaddr_in server_addr;
     server_addr.sin_addr.s_addr = INADDR_ANY;
     server_addr.sin_family = AF_INET;
     server_addr.sin_port = htons(port);
     set_port_reuse(listen_fd);
     bind(listen_fd, (const struct sockaddr *)&server_addr, sizeof(server_addr));
     listen(listen_fd, 5);
     epoll.add_fd(listen_fd, false);
}
void server_clean()
{
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
       if(cur_ready_events&(EPOLLERR|EPOLLHUP|EPOLLRDHUP)) all_client[cur_ready_fd].close_conn();
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
         if(ready_fd_number==-1&&ready_fd_number!=EINTR)
         {
             perror("epoll_wait");
             break;
         }
         for(int i = 0; i < ready_fd_number; ++i) handle_ready_event(i);
    }
    server_clean();
    return 0;
}