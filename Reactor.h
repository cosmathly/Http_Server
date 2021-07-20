#ifndef _Reactor_
#define _Reactor_
#include <sys/epoll.h>
#include <sys/socket.h>
#include "Thread_Pool.h"
typedef unsigned int uint32_t;
extern const int port;
extern const int max_event_number;
extern const int max_conn_fd_number;
extern void set_nonblocking(int fd);
extern void server_initial(int &listen_fd);
class myepoll
{
      private:
         int epoll_fd;
      public:
         myepoll(int _epoll_fd_);
         void add_fd(int to_add_fd, bool if_set_epolloneshot = true);
         void del_fd(int to_del_fd);
         void mod_fd(int to_mod_fd, uint32_t new_listen_event);
         int get_ready_fd(int maxevents, int timeout);
};
class client
{
      public:
        static int client_num;
        static const int read_buf_size;
        static const int write_buf_size;
        struct sockaddr_in addr;
        int sockfd = {-1};
        int read_in_content_len = {0};
        char *read_buf = {nullptr};
        int write_out_content_len = {0};
        int have_write_content_len = {0};
        bool whether_keep_alive = {false};
        char *write_buf = {nullptr};
        void init(int sockfd_for_client, const struct sockaddr_in &cur_client_addr);
        client();
        ~client();
        void close_conn();
        bool cli_read();
        void cli_write();
};
namespace critical_object
{
        extern myepoll epoll;
        extern client *all_client;
        extern epoll_event *all_ready_event;
        extern int listen_fd;
        extern Thread_Pool thread_pool;
}
using critical_object::epoll;
using critical_object::all_client;
using critical_object::all_ready_event;
using critical_object::listen_fd;
using critical_object::thread_pool;
#endif