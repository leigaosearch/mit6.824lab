// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

lock_server::lock_server():
  nacquire (0)
{
}

  lock_protocol::status
lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  printf("stat request from clt %d\n", clt);
  r = nacquire;
  return ret;
}

  lock_protocol::status
lock_server::acquire(int clt,lock_protocol::lockid_t lid, int &r)
{

  std::unique_lock<std::mutex> guard(table_mutex_);
  auto it = lock_table_.find(lid);
  if (it == lock_table_.end()) {
    it = lock_table_.find(lid);
    if (it == lock_table_.end()) {
      //no this kind of lock, insert.
      auto lockstatus = klocked;
      auto p = std::make_pair(lid,lockstatus);
      lock_table_.insert(p);
    }
  }
  else {
    if (lock_table_[lid] == klocked) {
      while(locked) {
        cond.wait(guard);
      }
      lock_table_[lid] = klocked;
    }

    else {
      lock_table_[lid] = klocked;
    }
  }
  locked = true;

  return 0;

}


  lock_protocol::status
lock_server::release(int clt,lock_protocol::lockid_t lid, int &r)
{

  std::unique_lock<std::mutex> guard(table_mutex_);
  auto it = lock_table_.find(lid);
  if (it != lock_table_.end()) {
    lock_table_[lid] = kfree;
    locked = false;
    cond.notify_one();

  }
  return 0;
}




