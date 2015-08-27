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
  std::lock_guard<std::mutex> lockcache(cachemutex);
  extent_protocol::status ret = extent_protocol::OK;
  extent_protocol::attr attr;
  if (extent_cache.count(eid) <= 0) {
    auto pvalue = new extent_cachevalue();
    ret = cl->call(extent_protocol::getattr, eid, attr);
 
    pvalue->attribute.size = attr.size;
    pvalue->attribute.atime = attr.atime;
    pvalue->attribute.mtime = attr.mtime;
    pvalue->attribute.ctime = attr.ctime;
    ret = cl->call(extent_protocol::get, eid, buf);
    pvalue->content = buf;
    pvalue->isdirty = false;
    extent_cache.insert(std::make_pair(eid, pvalue)); 
  } else {
  
     extent_cache[eid]->attribute.atime = time(NULL);
     buf = extent_cache[eid]->content; 
  }
  printf("get %d content = %s\n",eid,buf.c_str());
  return ret;
}

extent_protocol::status
extent_client::getattr(extent_protocol::extentid_t eid, 
		       extent_protocol::attr &attr)
{
  extent_protocol::status ret = extent_protocol::OK;
  std::lock_guard<std::mutex> lockcache(cachemutex);
  if (extent_cache.count(eid) == 0) {
    auto pvalue = new extent_cachevalue();
    ret = cl->call(extent_protocol::getattr, eid, attr);
    pvalue->attribute.size = attr.size;
    pvalue->attribute.atime = attr.atime;
    pvalue->attribute.mtime = attr.mtime;
    pvalue->attribute.ctime = attr.ctime;
    std::string buf;
    ret = cl->call(extent_protocol::get, eid, buf);
    pvalue->content = buf;
    pvalue->isdirty = false;
    extent_cache.insert(std::make_pair(eid, pvalue)); 
  } else {
     attr.size = extent_cache[eid]->attribute.size;
     attr.atime =  extent_cache[eid]->attribute.atime;
     attr.mtime = extent_cache[eid]->attribute.mtime;
     attr.ctime = extent_cache[eid]->attribute.ctime;
     extent_cache[eid]->attribute.atime = time(NULL); 
    
  }
  printf("get %d attr = %s\n", eid, extent_cache[eid]->content.c_str());
  return ret;
}

extent_protocol::status
extent_client::put(extent_protocol::extentid_t eid, std::string buf)
{
  extent_protocol::status ret = extent_protocol::OK;
  int r;
   std::lock_guard<std::mutex> lockcache(cachemutex);
  if(extent_cache.count(eid) > 0) {
  extent_cache[eid]->content = buf;
  extent_cache[eid]->attribute.size = buf.size();
  extent_cache[eid]->attribute.atime = time(NULL);
  extent_cache[eid]->attribute.mtime = time(NULL);
  extent_cache[eid]->attribute.ctime = time(NULL);
  extent_cache[eid]->isdirty = true;
 
  //ret = cl->call(extent_protocol::put, eid, buf, r);
  } else {
    auto pvalue = new extent_cachevalue();
    pvalue->attribute.size =  buf.size();
    pvalue->attribute.atime = time(NULL);
    pvalue->attribute.mtime = time(NULL);
    pvalue->attribute.ctime = time(NULL);
    pvalue->content = buf;
    pvalue->isdirty = true;
    extent_cache.insert(std::make_pair(eid, pvalue)); 
  }
  printf("put str = %s\n",buf.c_str());
  return ret;
}

extent_protocol::status
extent_client::remove(extent_protocol::extentid_t eid)
{
  extent_protocol::status ret = extent_protocol::OK;
  
  int r;
  ret = cl->call(extent_protocol::remove, eid, r);
  if (ret == extent_protocol::OK) {
    std::lock_guard<std::mutex> lockcache(cachemutex);
    if (extent_cache.count(eid) > 0) {
      delete extent_cache[eid];
      extent_cache.erase(eid);
    }
  }
  printf("remove\n");
  return ret;
}
extent_protocol::status
extent_client::flush(extent_protocol::extentid_t eid) {
    printf("call flush\n");
    extent_protocol::status ret = extent_protocol::OK;
     int r;
    //give up the lock
    std::lock_guard<std::mutex> lockcache(cachemutex);
    if (extent_cache.count(eid) > 0) {
      if(extent_cache[eid]->isdirty) {
        //call put attribute
      
       //ret = cl->call(extent_protocol::put, eid, extent_cache[eid]->attribute, r);
       printf("call flush put %s\n",extent_cache[eid]->content.c_str());
       ret = cl->call(extent_protocol::put, eid, extent_cache[eid]->content, r);
      }
      printf("remvoe cache\n");
      delete(extent_cache[eid]);
      extent_cache.erase(eid);
    } else {
      printf(" not in extent cache\n");
    }
   
    return ret;
} 