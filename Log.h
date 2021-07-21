#ifndef _Log_
#define _Log_
#include <queue>
#include <iostream>
#include <string.h>
#include "Reactor.h"
typedef std::string Record;
class Log
{
      public:
        int log_fd = {-1};
        bool whether_stop = {false};
        pthread_t write_log_tid = {0};
        std::queue<Record> cur_all_record;
        static void * save_log(void *arg);
        void write_log(const Record &record);
        Log();
        ~Log();
};
#endif