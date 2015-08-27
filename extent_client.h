// extent client interface.

#ifndef extent_client_h
#define extent_client_h

#include <string>
#include <map>
#include "extent_protocol.h"
#include "rpc.h"
#include <thread>
#include <mutex>

class extent_client {
  struct extent_cachevalue{
     extent_protocol::attr attribute;
     std::string content;
     bool iscontentcached;
     bool isattrcached;
     bool isdirty;   
  } ;
  
 
 private:
  rpcc *cl;
  std::map<extent_protocol::extentid_t, extent_cachevalue*> extent_cache;
  std::mutex cachemutex;
 public:
  extent_client(std::string dst);

  extent_protocol::status get(extent_protocol::extentid_t eid, 
			      std::string &buf);
  extent_protocol::status getattr(extent_protocol::extentid_t eid, 
				  extent_protocol::attr &a);
  extent_protocol::status put(extent_protocol::extentid_t eid, std::string buf);
  extent_protocol::status remove(extent_protocol::extentid_t eid);
  extent_protocol::status flush(extent_protocol::extentid_t eid);
};

#endif 

