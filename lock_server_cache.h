#ifndef lock_server_cache_h
#define lock_server_cache_h

#include <string>

#include <map>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_server.h"
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>



class lock_server_cache :lock_server{
 private:
  int nacquire;
 public:
  enum lock_state {
  FREE,
  LOCKED,
  REVOKING,
  RETRYING };
  struct lock_cache {
     lock_state state;
     std::string ownerid;
     std::queue<std::string> waitinglist;
     //std::queue<std::string> retryingIDs;
  };
  std::thread rk;
  std::thread rt;
  std::mutex retryqueuemutex;
  std::mutex revokequeuemutex;
  std::condition_variable retrycv;
  std::condition_variable revokecv;
  std::queue<std::pair<std::string,lock_protocol::lockid_t>> retryqueue;
  std::queue<std::pair<std::string,lock_protocol::lockid_t>> revokequeue;

  std::map<lock_protocol::lockid_t, lock_cache*> cachelocks;
  std::mutex lock_cache_mutex;
  lock_server_cache();
  lock_protocol::status stat(lock_protocol::lockid_t, int &);
  int acquire(lock_protocol::lockid_t, std::string id, int &);
  int release(lock_protocol::lockid_t, std::string id, int &);
  void revokethread() ;
  void retrythread();
};

#endif
