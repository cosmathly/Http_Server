#include "Log.h"
extern mylock log_lock;
Log::Log():cur_all_record{}
{
    log_fd = open("Log.txt", O_WRONLY|O_CREAT|O_APPEND);
    int ret = pthread_create(&write_log_tid, nullptr, save_log, (void *)this);
    if(ret!=0) 
    { 
      std::cout<<strerror(ret)<<std::endl; 
      exit(-1); 
    }
}
Log::~Log() { whether_stop = true; close(log_fd); }
void Log::write_log(const Record &record)
{
    const char *record_str = record.c_str();
    write(log_fd, (const void *)record_str, strlen(record_str));
}
void * Log::save_log(void *arg)
{
    Log *cur_log = (Log *)arg;
    int have_write_log_num = {0};
    while(cur_log->whether_stop==false)
    {
        sleep(5);
        log_lock.lock();
        while(cur_log->cur_all_record.empty()==false&&have_write_log_num<100)
        {
            cur_log->write_log(cur_log->cur_all_record.front());
            cur_log->cur_all_record.pop();
            ++have_write_log_num;
        }
        have_write_log_num = 0;
        log_lock.unlock();
    }
}