// the extent server implementation

#include <sstream>
#include <memory>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "extent_server.h"

extent_server::extent_server() {}


int extent_server::put(extent_protocol::extentid_t id, std::string buf, int &)
{
  // You fill this in for Lab 2.
  //std::lock_guard<std::
  int num = extentobjs.count(id);
  auto t = time(NULL);
  printf("put id = %llu\n",id);
  if (num == 0) {
    extentobjs[id] = std::make_shared<extent_value>();
#if 0
    std::shared_ptr<extent_value> sp = std::make_shared<extent_value>();
    extentobjs.insert(std::make_pair(id,sp));
#endif
  }
    extentobjs[id]->data = buf;
    extentobjs[id]->attr.ctime = t;
    extentobjs[id]->attr.mtime = t;
    extentobjs[id]->attr.atime = t;
    extentobjs[id]->attr.size = buf.size();
  return extent_protocol::OK;
}

int extent_server::get(extent_protocol::extentid_t id, std::string &buf)
{
  int num = extentobjs.count(id);
  // You fill this in for Lab 2.
  printf("get id = %llu\n",id);
  if (num == 0) {
    return extent_protocol::NOENT;
  }
  else {
    buf = extentobjs[id]->data;
    extentobjs[id]->attr.atime = time(NULL);
    return extent_protocol::OK;
  }
}

int extent_server::getattr(extent_protocol::extentid_t id, extent_protocol::attr &a)
{
  // You fill this in for Lab 2.
  // You replace this with a real implementation. We send a phony response
  // for now because it's difficult to get FUSE to do anything (including
  // unmount) if getattr fails.
  int num = extentobjs.count(id);
  // You fill this in for Lab 2.
  if (num == 0) {
    a.size = 0;
    a.atime = 0;
    a.mtime = 0;
    a.ctime = 0;
    return extent_protocol::NOENT;
  }
  else {
    a = extentobjs[id]->attr;
    return extent_protocol::OK;
  }
}

int extent_server::remove(extent_protocol::extentid_t id, int &)
{
  // You fill this in for Lab 2.
  auto it = extentobjs.find(id);
  // You fill this in for Lab 2.
  if (it == extentobjs.end()) {
    return extent_protocol::NOENT;
  }
  else {
    extentobjs.erase(it);
    return extent_protocol::OK;
  }
}

