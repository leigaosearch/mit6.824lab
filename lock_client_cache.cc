// RPC stubs for clients to talk to lock_server, and cache the cachelocks
// see lock_client.cache.h for protocol details.

#include "lock_client_cache.h"
#include "rpc.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include "tprintf.h"


lock_client_cache::lock_client_cache(std::string xdst, 
    class lock_release_user *_lu)
: lock_client(xdst), lu(_lu)
{
  rpcs *rlsrpc = new rpcs(0);
  rlsrpc->reg(rlock_protocol::revoke, this, &lock_client_cache::revoke_handler);
  rlsrpc->reg(rlock_protocol::retry, this, &lock_client_cache::retry_handler);
  const char *hname;
  hname = "127.0.0.1";
  std::ostringstream host;
  host << hname << ":" << rlsrpc->port();
  id = host.str();
  tprintf("contstruct\n");
  rk = std::thread(&lock_client_cache::revokethread,this);
  tprintf("contstruct revokethread\n");
  rt = std::thread (&lock_client_cache::retrythread,this);
}

  lock_protocol::status
lock_client_cache::acquire(lock_protocol::lockid_t lid)
{
  int r;
  tprintf("%s,enter acquire\n",id.c_str());
  //std::unique_lock<std::mutex> lockcache(cachelocksmutex);
  tprintf("before new a lock %d count %d\n", lid, cachelocks.count(lid));
  if (cachelocks.count(lid) <= 0) {
    tprintf("new a lock %d\n", lid);
    auto plock = new CacheLock;
    plock->status = NONE;
    cachelocks.insert(std::pair<lock_protocol::lockid_t, CacheLock*>(lid,plock));
    tprintf("new a lock %d count %d\n", lid, cachelocks.count(lid));
  }
  tprintf("%s enter acquire loop status = %d\n", id.c_str(), cachelocks[lid]->status);
  lock_protocol::status ret ;
  
  //printf("enter lock.lock finished\n");
  while(1) {
    std::unique_lock<std::mutex> locka(cachelocks[lid]->m);
    if(cachelocks[lid]->status == NONE) {
      cachelocks[lid]->status = ACQUIRING;
      tprintf("%s cl->call(lock_protocol::acquire, %d, lid, %dr\n",id.c_str(), lid);
      ret = cl->call(lock_protocol::acquire, lid, id, r);
      if (ret == lock_protocol::OK) {
        cachelocks[lid]->status = LOCKED;
        tprintf("%s enter acquire ret OK status %d\n",id.c_str(), cachelocks[lid]->status);
        break;
      } else if (ret == lock_protocol::RETRY) {
        tprintf("% senter acquire ret RETRY\n",id.c_str());
          cachelocks[lid]->lockcv.wait(locka, []{return true;});
          tprintf("%s %d cachelocks[lid]->status =%d\n",id.c_str(), lid,cachelocks[lid]->status);
          if(cachelocks[lid]->status == FREE) {
            tprintf("changed: %s %d cachelocks[lid]->status =%d\n",id.c_str(), lid,cachelocks[lid]->status);
            cachelocks[lid]->status = LOCKED ;
            break;
          }
          else continue;
      }
    } else if (cachelocks[lid]->status == FREE) {
      cachelocks[lid]->status = LOCKED;
      tprintf("%s status = %d ret OK and locked\n", id.c_str(), cachelocks[lid]->status);
      break;
    } else {
      //cachelocks[lid]->cv.wait(lock, []{return true;});
      tprintf("%s  waiting cv is %d...............\n", id.c_str(),cachelocks[lid]->status);
      cachelocks[lid]->lockcv.wait(locka);
      tprintf("%s ACQUIRING status is %d...............\n", id.c_str(),cachelocks[lid]->status);
      if (FREE == cachelocks[lid]->status) {
        cachelocks[lid]->status = LOCKED;
        break;            
      }
      else continue;
  }
 }
 return lock_protocol::OK;
}

  lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid)
{  
  int r, ret;
  tprintf("%s release %d\n",id.c_str(),lid);
  //std::unique_lock<std::mutex> lockcache(cachelocksmutex);
  tprintf("lockcache(cachelocksmutex)\n");
  std::unique_lock<std::mutex> lock(cachelocks[lid]->m);
  tprintf(" lock(cachelocks[lid]->m\n");
  if (cachelocks[lid]->status == RELEASING) {
    // tell revoke thread to call release the lock
    cachelocks[lid]->revokecv.notify_all();
    tprintf("%s release  %d OK\n",id.c_str(),lid);
  } else if (cachelocks[lid]->status == LOCKED) {
    tprintf("%s release sucess  %d OK\n",id.c_str(),lid);
    cachelocks[lid]->status = FREE;
    cachelocks[lid]->lockcv.notify_all();
    
  }
  else {
    cachelocks[lid]->status = FREE;
    cachelocks[lid]->lockcv.notify_all();
    tprintf("%s release error %d\n",id.c_str(),cachelocks[lid]->status);
  }
  tprintf("%s release return %d\n",id.c_str(),lid);
  return lock_protocol::OK;
}
//Your client code will need to remember the fact that the revoke has arrived, and release the lock as soon as you are done with it. The same situation can arise with retry RPCs, which can arrive at the client before the corresponding acquire returns the RETRY failure code.
  rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid, 
    int &)
{
  int ret = rlock_protocol::OK;
  printf("%s get revoke rpc  %d OK\n",id.c_str(),lid);
  // send a signal to release the lock
  std::unique_lock<std::mutex> lock(revokequeuemutex);
  revokequeue.push(lid);
  revokequeuecv.notify_all();
  return ret;
}

// This method should be a continuous loop, waiting to be notified of
// freed cachelocks that have been revoked by the server, so that it can
// send a release RPC.
// Release rpc send
void lock_client_cache::revokethread() {
  printf("%s revokethread begin\n",id.c_str());
  int r;
  while(1) {
    std::unique_lock<std::mutex> lock(revokequeuemutex);
    revokequeuecv.wait(lock,[]{return true;});
   
    while (!revokequeue.empty()) {
      auto lid = revokequeue.front();
      std::unique_lock<std::mutex> mylock(cachelocks[lid]->m);
      if(cachelocks[lid]->status == LOCKED) {
        cachelocks[lid]->status = RELEASING;
        cachelocks[lid]->revokecv.wait(mylock);
      }
      

      tprintf("\n %s lid %d call release ipc\n",id.c_str(),lid);
      lock_protocol::status ret = cl->call(lock_protocol::release, lid, id, r);
      if (ret ==  lock_protocol::OK) {
        revokequeue.pop();
        cachelocks[lid]->status = NONE;
        tprintf("\n %s lid %d free\n",id.c_str(),lid);
      }



    }
  }
}

  rlock_protocol::status
lock_client_cache::retry_handler(lock_protocol::lockid_t lid, 
    int &)
{
  tprintf("id = %s,%d retry\n",id.c_str(),lid);
  int ret = rlock_protocol::OK;
  std::unique_lock<std::mutex> lock(retryqueuemutex);
  // get the retry will acquire again.
  retryqueue.push(lid);
  retryqueuecv.notify_all();
  
  return ret;
}


void lock_client_cache::retrythread() {
  int r;
  printf("retrythread begin");
  while(1) {
    std::unique_lock<std::mutex> lock(retryqueuemutex);
    retryqueuecv.wait(lock,[]{return true;});
    while(!retryqueue.empty()) {
      auto lid =  retryqueue.front();
      tprintf("%s lid %d retry\n",id.c_str(),lid);
      lock_protocol::status ret = cl->call(lock_protocol::acquire, lid, id, r);
      tprintf("%s retry %d thread call acquire\n",id.c_str(),lid);
      tprintf("%s retry %d thread call acquire ret = %d\n",id.c_str(),lid,ret);
      cachelocks[lid]->status = FREE;
      cachelocks[lid]->lockcv.notify_all();
      retryqueue.pop();

    }
  }
}
