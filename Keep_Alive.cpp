#include "Keep_Alive.h"
extern Keep_Alive keep_alive;
extern mylock keep_alive_lock;
extern client *all_client;
Keep_Alive::Keep_Alive(unsigned int _time_slot):all_timing_things{}, map_helper{}
{
    time_slot = _time_slot;
    struct sigaction new_sigaction;
    new_sigaction.sa_handler = timing_things_handle;
    sigfillset(&new_sigaction.sa_mask);
    assert(sigaction(SIGALRM, &new_sigaction, nullptr)!=-1);
    alarm(time_slot);
}
void Keep_Alive::insert(time_t expire_time, int sockfd)
{
    auto it = map_helper.find(sockfd);
    if(it==map_helper.end()) 
    {
       all_timing_things.insert(std::pair<time_t, int>(expire_time, sockfd));
       map_helper.insert(std::pair<int, time_t>(sockfd, expire_time));
    }
    else return ;
}
void Keep_Alive::del(int sockfd)
{
    auto it = map_helper.find(sockfd);
    if(it==map_helper.end()) return ;
    time_t expire_time = (*it).second;
    map_helper.erase(it);
    auto lower_bound = all_timing_things.lower_bound(expire_time);
    auto upper_bound = all_timing_things.upper_bound(expire_time);
    for(auto cur_idx = lower_bound; cur_idx != upper_bound; ++cur_idx)
    if((*cur_idx).second==sockfd) { all_timing_things.erase(cur_idx); break; }
}
void Keep_Alive::timing_things_handle(int sig_num)
{
    int ret = keep_alive_lock.trylock();
    if(ret==EBUSY) { alarm(keep_alive.time_slot); return ; }
    time_t cur_time = time(nullptr);
    for(auto &val : keep_alive.all_timing_things)
    {
       if(val.first>cur_time) break;
       else all_client[val.second].close_conn(false); 
    }
    int sockfd;
    decltype(keep_alive.map_helper.begin()) it;
    while(true)
    {
       if(keep_alive.all_timing_things.empty()==true) break;
       if((*keep_alive.all_timing_things.begin()).first<=cur_time) 
       {
          sockfd = (*keep_alive.all_timing_things.begin()).second;
          keep_alive.all_timing_things.erase(keep_alive.all_timing_things.begin());
          it = keep_alive.map_helper.find(sockfd);
          if(it!=keep_alive.map_helper.end())
          keep_alive.map_helper.erase(it);
       }
       else break;
    }
    alarm(keep_alive.time_slot);
    keep_alive_lock.unlock();
}