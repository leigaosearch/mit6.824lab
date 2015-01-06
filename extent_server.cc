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
  int num = extentattrs.count(id);
  auto t = time(NULL);
  printf("put id = %llu\n",id);
  if (num == 0) {
  printf("create = %llu\n",id);
    extent_protocol::attr a;
    a.atime = t;
    a.mtime = t;
    a.ctime = t;
    a.size = buf.size();
    extentattrs[id] = a;
    //extentobjs[id] = std::make_shared<extent_value>();
#if 0
    std::shared_ptr<extent_value> sp = std::make_shared<extent_value>();
    extentobjs.insert(std::make_pair(id,sp));
#endif
  }
  else {
    extentattrs[id].mtime = t;
    extentattrs[id].atime = t;
    extentattrs[id].ctime = t;
    extentattrs[id].size = buf.size();
  }

  files[id] = buf;
  printf("%s",buf.c_str());
  return extent_protocol::OK;
}

int extent_server::get(extent_protocol::extentid_t id, std::string &buf)
{
  int num = extentattrs.count(id);
  // You fill this in for Lab 2.
  printf("extent server get id = %llu\n",id);
  if (num == 0) {
    printf("not exist\n");
    return extent_protocol::NOENT;
  }
  else {
    buf = files[id];
    extentattrs[id].atime = time(NULL);
    return extent_protocol::OK;
  }
}

int extent_server::getattr(extent_protocol::extentid_t id, extent_protocol::attr &a)
{
  // You fill this in for Lab 2.
  // You replace this with a real implementation. We send a phony response
  // for now because it's difficult to get FUSE to do anything (including
  // unmount) if getattr fails.
  int num = extentattrs.count(id);
  // You fill this in for Lab 2.
  if (num == 0) {
    printf("**not exist\n");
    return extent_protocol::NOENT;
  }
  else {
    a.atime = time(NULL);
    a.ctime = extentattrs[id].ctime;
    a.size = extentattrs[id].size;
    a.mtime = extentattrs[id].mtime;
    return extent_protocol::OK;
  }
}

int extent_server::remove(extent_protocol::extentid_t id, int &)
{
  // You fill this in for Lab 2.
  auto it = extentattrs.find(id);
  // You fill this in for Lab 2.
  if (it == extentattrs.end()) {
    return extent_protocol::IOERR;
  }
  else {
    extentattrs.erase(it);
    files.erase(files.find(id));
    return extent_protocol::OK;
  }
}

