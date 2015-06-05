// the caching lock server implementation

#include "lock_server_cache.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <map>
#include "lang/verify.h"
#include "handle.h"
#include "tprintf.h"
#include <chrono>

lock_server_cache::lock_server_cache():lock_server(){
  rpcs *rlsrpc = new rpcs(0);
  tprintf("contstruct\n");
  rk = std::thread(&lock_server_cache::revokethread,this);
  tprintf("contstruct revokethread\n");
  rt = std::thread(&lock_server_cache::retrythread,this);

}


int lock_server_cache::acquire(lock_protocol::lockid_t lid, std::string id, 
                               int & r)
{
  lock_protocol::status ret = lock_protocol::OK;
  std::unique_lock<std::mutex> lock(lock_cache_mutex);
  tprintf("get client %s acquire request lock %d\n", id.c_str(), lid);
  if (cachelocks.count(lid) == 0) {
    auto plock = new lock_cache;
    plock->state = LOCKED;
    plock->ownerid = id;
    cachelocks.insert(std::make_pair(lid, plock));
    ret = lock_protocol::OK;
  } else if (cachelocks[lid]->state == FREE) {
    cachelocks[lid]->state = LOCKED;
    cachelocks[lid]->ownerid = id;
    ret = lock_protocol::OK;
  } else if (cachelocks[lid]->state == LOCKED) {
    cachelocks[lid]->waitinglist.push(id);
    revokequeue.push(std::make_pair(cachelocks[lid]->ownerid,lid));
    revokecv.notify_one();
    ret = lock_protocol::RETRY;
  }
  return ret;
}

int 
lock_server_cache::release(lock_protocol::lockid_t lid, std::string id, 
         int &r)
{
  tprintf("get client %s release request lock %d\n", id.c_str(), lid);
  std::unique_lock<std::mutex> lock(retryqueuemutex);
  if(cachelocks[lid]->state == LOCKED) {
    cachelocks[lid]->state = FREE;
  }

  retryqueue.push(std::make_pair(id,lid));
  retrycv.notify_one();
  //notify list
  lock_protocol::status ret = lock_protocol::OK;
  return ret;
}

lock_protocol::status
lock_server_cache::stat(lock_protocol::lockid_t lid, int &r)
{
  tprintf("stat request\n");
  r = nacquire;
  return lock_protocol::OK;
}

void lock_server_cache::retrythread() {
  printf("server retry thread begin\n");
  while(1) {
    std::unique_lock<std::mutex> lock(retryqueuemutex);
    rlock_protocol::status ret;
    retrycv.wait(lock);
    while (!retryqueue.empty()) {
      auto e = retryqueue.front();
      auto lid = e.second;
      auto q = cachelocks[lid]->waitinglist;
      auto qe = q.front();
      q.pop();
      handle h(qe);
      int r;
      printf("server call  retry %d\n",  lid);
      if (h.safebind()) 
        auto ret = h.safebind()->call(rlock_protocol::retry, lid, r);
        //xxx todo
      if (!h.safebind() || ret != rlock_protocol::OK) 
                tprintf("retry RPC failed\n");
      
    }
  }

}

void lock_server_cache::revokethread() {
  tprintf("server revoke thread begin\n");
  int r;
  while (1) {
    //std::this_thread::sleep_for(std::chrono::seconds(2));
    std::unique_lock<std::mutex> lock(revokequeuemutex);
    tprintf("server revoke mutex\n");
    rlock_protocol::status ret;
    tprintf("server revoke wait\n");
    //std::this_thread::sleep_for(std::chrono::seconds(2));
    revokecv.wait(lock);
    tprintf("server revoke return\n");
    while (!revokequeue.empty()) {
      auto e = revokequeue.front();
      auto cliid = e.first;
      auto lid = e.second;
      printf("server call  revoke %d\n",  lid);
      revokequeue.pop();
      handle h(cliid);
      if (h.safebind())
        auto ret = h.safebind()->call(rlock_protocol::revoke, lid, r);
      if (!h.safebind() || ret != rlock_protocol::OK) 
                tprintf("revoke RPC failed\n");
    }

  }

}
