// this is the extent server

#ifndef extent_server_h
#define extent_server_h

#include <string>
#include <map>
#include <memory>
#include "extent_protocol.h"

class extent_server {

 public:
  extent_server();

  int put(extent_protocol::extentid_t id, std::string, int &);
  int get(extent_protocol::extentid_t id, std::string &);
  int getattr(extent_protocol::extentid_t id, extent_protocol::attr &);
  int remove(extent_protocol::extentid_t id, int &);
 private:
  struct extent_value {
    std::string data;
    extent_protocol::attr attr;
  };
  std::map<extent_protocol::extentid_t, std::shared_ptr<extent_value>> extentobjs;
};

#endif 







