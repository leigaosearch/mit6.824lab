// RPC stubs for clients to talk to lock_server, and cache the locks
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
  std::thread(revokethread);
  std::thread(retrythread);
}

  lock_protocol::status
lock_client_cache::acquire(lock_protocol::lockid_t lid)
{
  int r;
  if (locks.count(lid) <= 0) {
   auto plock = new CacheLock;
   locks.insert(std::pair<lock_protocol::lockid_t, CacheLock*>(lid,plock));
  }
  std::unique_lock<std::mutex>(locks[lid]->m);
  
  while(1) {
    lock.lock();
    if(locks[lid]->status == NONE) {
      lock_protocol::status ret = cl->call(lock_protocol::acquire, id, lid, r);
      locks[lid]->status = ACQUIRING;
      if (ret == lock_protocol::OK) {
        locks[lid]->status = LOCKED;
        break;
      } else if (ret == lock_protocol::RETRY) {
        //locks[lid]->cv.wait(lock,[]{return true;});
        if (FREE == locks[lid]->status) {
          locks[lid]->status = LOCKED;
          break;
        }
        else { 
          lock.unlock();
        }continue;
      }
    } else if (locks[lid]->status == FREE) {
      locks[lid]->status = LOCKED;
      break;
    } else {
      //locks[lid]->cv.wait(lock, []{return true;});
      if (FREE == locks[lid]->status) {
          locks[lid]->status = LOCKED;
          break;
      } else {
        lock.unlock();
        continue;
      }

    }
  }
  lock.unlock();
  return lock_protocol::OK;
}

  lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid)
{
  std::unique_lock<std::mutex> guard(locks[lid]->m);
  lock.lock();
  if(locks[lid]->status == LOCKED) {
    locks[lid]->status = FREE;
    //locks[lid]->cv.notify_all();
  } else {
    
  }
  lock.unlock();

  return lock_protocol::OK;
}
//Your client code will need to remember the fact that the revoke has arrived, and release the lock as soon as you are done with it. The same situation can arise with retry RPCs, which can arrive at the client before the corresponding acquire returns the RETRY failure code.
  rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid, 
    int &)
{
  int ret = rlock_protocol::OK;
  // send a signal to release the lock
  std::unique_lock<std::mutex> lock(revokequeuemutex);
  lock.lock();
  revokequeue.push(lid);
  revokequeuecv.notify_all();
  lock.unlock();
  return ret;
}

  rlock_protocol::status
lock_client_cache::retry_handler(lock_protocol::lockid_t lid, 
    int &)
{
  int ret = rlock_protocol::OK;
  std::unique_lock<std::mutex> lock(retryqueuemutex);
  lock.lock();
  // get the retry will acquire again.
  retryqueue.push(lid);
  retryqueuecv.notify_all();
  lock.unlock()
  return ret;
}


// This method should be a continuous loop, waiting to be notified of
// freed locks that have been revoked by the server, so that it can
// send a release RPC.
void lock_client_cache::revokethread() {
  int r;
  while(1) {
    std::unique_lock<std::mutex> lock(revokequeuemutex);
    revokequeuecv.wait(lock,[]{return true;});
    while (!revokequeue.empty()) {
        auto lid = revokequeue.front();
        revokequeue.pop();
        if (locks[lid]->status == FREE) {
            lock_protocol::status ret = cl->call(lock_protocol::release, id, lid, r);
            if (ret ==  lock_protocol::OK) {
              locks[lid]->status = NONE;
              break;      
            }
        } else {
          tprintf("error in revokethread");
        }
        
    }
  }
}

void lock_client_cache::retrythread() {
  int r;
  while(1) {
    std::unique_lock<std::mutex> lock(retryqueuemutex);
    while(!retryqueue.empty()) {
      auto lid =  retryqueue.front();
      retryqueue.pop();
      lock_protocol::status ret = cl->call(lock_protocol::acquire, id, lid, r);
      if(ret ==  rlock_protocol::OK) {
        locks[lid]->status = LOCKED;
      }

    }
  }
}
