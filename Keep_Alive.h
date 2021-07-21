#ifndef _Keep_Alive_
#define _Keep_Alive_
#include <map>
#include <unordered_map>
#include "Reactor.h"
#include "Thread_Pool.h"
class Keep_Alive
{
    public:
    int time_slot;
    std::multimap<time_t, int> all_timing_things;
    std::unordered_map<int, time_t> map_helper;
    Keep_Alive(unsigned int time_slot);  
    void insert(time_t expire_time, int sockfd);
    void del(int sockfd);
    static void timing_things_handle(int sig_num);
};
#endif