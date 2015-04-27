// lock client interface.

#ifndef lock_client_cache_h

#define lock_client_cache_h

#include <string>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_client.h"
#include "lang/verify.h"
#include <chrono>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
// Classes that inherit lock_release_user can override dorelease so that 
// that they will be called when lock_client releases a lock.
// You will not need to do anything with this class until Lab 5.
class lock_release_user {
 public:
  virtual void dorelease(lock_protocol::lockid_t) = 0;
  virtual ~lock_release_user() {};
};

enum LOCKSTATUS {
  NONE = 0,
  FREE,
  ACQUIRING,
  RELEASING,
  LOCKED
};
struct CacheLock {
  LOCKSTATUS status;
  std::mutex m;
  std::condition_variable cv;
};

class lock_client_cache : public lock_client {
 private:
  class lock_release_user *lu;
  int rlock_port;
  std::string hostname;
  std::string id;
  std::mutex retryqueuemutex;
  std::condition_variable retryqueuecv;
  std::queue<lock_protocol::lockid_t> retryqueue;
  std::mutex revokequeuemutex;
  std::condition_variable revokequeuecv;
  std::queue<lock_protocol::lockid_t> revokequeue;
  std::map<lock_protocol::lockid_t, CacheLock> locks;  
 public:
  lock_client_cache(std::string xdst, class lock_release_user *l = 0);
  virtual ~lock_client_cache() {};
  lock_protocol::status acquire(lock_protocol::lockid_t);
  lock_protocol::status release(lock_protocol::lockid_t);
  rlock_protocol::status revoke_handler(lock_protocol::lockid_t, 
                                        int &);
  rlock_protocol::status retry_handler(lock_protocol::lockid_t, 
                                       int &);
  void retrythread();
  void revokethread();
};


#endif