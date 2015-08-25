// RPC stubs for clients to talk to extent_server

#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

// The calls assume that the caller holds a lock on the extent

extent_client::extent_client(std::string dst)
{
  sockaddr_in dstsock;
  make_sockaddr(dst.c_str(), &dstsock);
  cl = new rpcc(dstsock);
  if (cl->bind() != 0) {
    printf("extent_client: bind failed\n");
  }
 
}

extent_protocol::status
extent_client::get(extent_protocol::extentid_t eid, std::string &buf)
{
  extent_protocol::status ret = extent_protocol::OK;
  extent_protocol::attr attr;
  if (extent_cache.count(eid) <= 0) {
    auto pvalue = new extent_cachevalue();
    ret = cl->call(extent_protocol::getattr, eid, attr);
    pvalue->attribute.size = attr.size;
    pvalue->attribute.atime = attr.atime;
    pvalue->attribute.mtime = attr.mtime;
    pvalue->attribute.ctime = attr.ctime;
    cl->call(extent_protocol::get, eid, buf);
    pvalue->content = buf;
    pvalue->isdirty = false;
    extent_cache.insert(std::make_pair(eid, pvalue)); 
  } else {
    extent_cache[eid]->attribute.atime = time(NULL); 
  }

  return ret;
}

extent_protocol::status
extent_client::getattr(extent_protocol::extentid_t eid, 
		       extent_protocol::attr &attr)
{
  extent_protocol::status ret = extent_protocol::OK;
  if (extent_cache.count(eid) == 0) {
    std::string buf;
    ret = cl->call(extent_protocol::getattr, eid, attr);
    auto pvalue = new extent_cachevalue();
    ret = cl->call(extent_protocol::getattr, eid, attr);
    pvalue->attribute.size = attr.size;
    pvalue->attribute.atime = attr.atime;
    pvalue->attribute.mtime = attr.mtime;
    pvalue->attribute.ctime = attr.ctime;
    cl->call(extent_protocol::get, eid, buf);
    pvalue->content = buf;
    pvalue->isdirty = false;
    extent_cache.insert(std::make_pair(eid, pvalue)); 
  } else {
   
    
    
  }
  return ret;
}

extent_protocol::status
extent_client::put(extent_protocol::extentid_t eid, std::string buf)
{
  extent_protocol::status ret = extent_protocol::OK;
  int r;
  
  extent_cache[eid]->content = buf;
  extent_cache[eid]->attribute.size = buf.size();
  extent_cache[eid]->attribute.atime = time(NULL);
  extent_cache[eid]->attribute.mtime = time(NULL);
  extent_cache[eid]->attribute.ctime = time(NULL);
  extent_cache[eid]->isdirty = true;
  
  
  // ret = cl->call(extent_protocol::put, eid, buf, r);

  return ret;
}

extent_protocol::status
extent_client::remove(extent_protocol::extentid_t eid)
{
  extent_protocol::status ret = extent_protocol::OK;
  int r;
  ret = cl->call(extent_protocol::remove, eid, r);
  return ret;
}
extent_protocol::status
extent_client::flush(extent_protocol::extentid_t eid) {
    extent_protocol::status ret = extent_protocol::OK;
     int r;
    //give up the lock
    if (extent_cache.count(eid) > 0) {
      if(extent_cache[eid]->isdirty) {
        //call put
        ret = cl->call(extent_protocol::put, eid, extent_cache[eid]->content, r);
      }
      delete(extent_cache[eid]);
      extent_cache.erase(eid);
    } else {
      printf(" not in extent cache\n");
    }
   
    return ret;
} 