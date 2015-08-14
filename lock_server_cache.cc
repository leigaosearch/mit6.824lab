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
    tprintf("lid not exsit %s acquire request lock %d\n", id.c_str(), lid);
    auto plock = new lock_cache;
    plock->state = LOCKED;
    plock->ownerid = id;
    cachelocks.insert(std::make_pair(lid, plock));
    ret = lock_protocol::OK;
  } else if (cachelocks[lid]->state == FREE) {
    tprintf("lock free %s acquire request lock %d\n", id.c_str(), lid);
    cachelocks[lid]->state = LOCKED;
    cachelocks[lid]->ownerid = id;
    ret = lock_protocol::OK;
  } else if ((cachelocks[lid]->retryingid.compare(id) == 0) && (cachelocks[lid]->state == RETRYING)) { //get retrying acquire.
    tprintf("client %s lock %d RETRYING\n", id.c_str(), lid);
    //cachelocks[lid]->waitinglist.push(id);
        cachelocks[lid]->state = LOCKED;
        cachelocks[lid]->ownerid = id;
        if (cachelocks[lid]->waitinglist.size() > 0) {
          //call revoke
             tprintf("%s,%d waitinglist >0\n", id.c_str(), lid);
             revokelist.push_back(std::make_pair(cachelocks[lid]->ownerid,lid));
             revokecv.notify_one();
        }  
    } else {
             tprintf("cachelocks[lid]->retryingid =%s cachelocks[lid]->state= %d  \n", cachelocks[lid]->retryingid.c_str(), cachelocks[lid]->state);
             tprintf("client %s lock %d was locked return RETRY %d  \n", id.c_str(), lid,cachelocks[lid]->state);
             cachelocks[lid]->waitinglist.push(id);
             revokelist.push_back(std::make_pair(cachelocks[lid]->ownerid,lid));
             revokecv.notify_one();
             ret = lock_protocol::RETRY;
    }
  return ret;
}

int 
lock_server_cache::release(lock_protocol::lockid_t lid, std::string id, 
         int &r)
{
  tprintf("get client %s %d release request\n", id.c_str(), lid);
  std::unique_lock<std::mutex> lock(retryqueuemutex);
  //if(cachelocks[lid]->state == LOCKED) {
  cachelocks[lid]->state = FREE;
  //}

  if (cachelocks[lid]->waitinglist.size() > 0) {
    cachelocks[lid]->state = RETRYING;
   
    auto clientid = cachelocks[lid]->waitinglist.front();
    tprintf(" %s  %d change to RETRYING\n", clientid.c_str(), lid);
    cachelocks[lid]->waitinglist.pop();
    retryqueue.push(std::make_pair(clientid,lid));
    cachelocks[lid]->retryingid = clientid;
    retrycv.notify_one();   
  }
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
  tprintf("server retry thread begin\n");
  while(1) {
    std::unique_lock<std::mutex> lock(retryqueuemutex);
    rlock_protocol::status ret;
    retrycv.wait(lock);
    tprintf("server retry thread cv get notified\n");
    while (!retryqueue.empty()) {
      auto e = retryqueue.front();
      auto lid = e.second;
      handle h(e.first);
      int r;
      if (h.safebind()) 
        auto ret = h.safebind()->call(rlock_protocol::retry, lid, r);
          //xxx todo
      if (!h.safebind() || ret != rlock_protocol::OK) {
                  tprintf("retry RPC failed\n");
        }
      retryqueue.pop();
        //cachelocks[lid]->status = RETRYING;
      }

      
  }

}

void lock_server_cache::revokethread() {
  tprintf("server revoke thread begin\n");
  int r;
  while (1) {
    std::this_thread::sleep_for(std::chrono::microseconds(2));
    std::unique_lock<std::mutex> lock(revokelistmutex);
    //tprintf("server revoke mutex\n");
    rlock_protocol::status ret;
    tprintf("server revoke wait\n");
    //std::this_thread::sleep_for(std::chrono::seconds(2));
    revokecv.wait(lock);
   // tprintf("server revoke return\n");
    for (auto it = revokelist.begin(); it!=revokelist.end();) {
      auto e = *it;
      auto lid = e.second;
      if((cachelocks[lid]->state != REVOKING)&&(cachelocks[lid]->state != FREE)) {
        auto cliid = cachelocks[lid]->ownerid;
        tprintf("server call  %s revoke %d\n",  cliid.c_str(),lid);
        cachelocks[lid]->state = REVOKING;
        it =revokelist.erase(it);
        handle h(cliid);
        if (h.safebind())
          auto ret = h.safebind()->call(rlock_protocol::revoke, lid, r);
        if (!h.safebind() || ret != rlock_protocol::OK) 
          tprintf("revoke RPC failed\n");
      }
      else {
        //printf("revoking %d\n", it->second);
        it++;
      }
    }

  }

}
