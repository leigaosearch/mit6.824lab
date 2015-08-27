// this is the lock server
// the lock client has a similar interface

#ifndef lock_server_h
#define lock_server_h

#include <string>
#include <mutex>
#include <thread>
#include <condition_variable>
#include "lock_protocol.h"
#include "lock_client.h"
#include "rpc.h"


const int klocked = 1;
const int kfree = 0;

class lock_server {

 protected:
  int nacquire;  
  std::map<lock_protocol::lockid_t,  lock_protocol::status> lock_table_;
  std::mutex table_mutex_;
  std::mutex cv_mutex;
  std::condition_variable cond;
  bool locked = false;

 public:
  lock_server();
  ~lock_server() {};
  lock_protocol::status stat(int clt, lock_protocol::lockid_t lid, int &);
  lock_protocol::status acquire(int clt,lock_protocol::lockid_t lid, int &);
  lock_protocol::status release(int clt,lock_protocol::lockid_t lid, int &);
};

#endif 







