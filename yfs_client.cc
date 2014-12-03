// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
  ec = new extent_client(extent_dst);

}

  yfs_client::inum
yfs_client::n2i(std::string n)
{
  std::istringstream ist(n);
  unsigned long long finum;
  ist >> finum;
  return finum;
}

  std::string
yfs_client::filename(inum inum)
{
  std::ostringstream ost;
  ost << inum;
  return ost.str();
}

  bool
yfs_client::isfile(inum inum)
{
  if(inum & 0x80000000)
    return true;
  return false;
}

  bool
yfs_client::isdir(inum inum)
{
  return ! isfile(inum);
}

  int
yfs_client::getfile(inum inum, fileinfo &fin)
{
  int r = OK;

  printf("getfile %016llx\n", inum);
  extent_protocol::attr a;
  if (ec->getattr(inum, a) != extent_protocol::OK) {
    printf("IO ERROR\n");
    r = IOERR;
    goto release;
  }

  fin.atime = a.atime;
  fin.mtime = a.mtime;
  fin.ctime = a.ctime;
  fin.size = a.size;
  printf("getfile %016llx -> sz %llu\n", inum, fin.size);

release:

  return r;
}
int 
yfs_client::setattr(inum inum, fileinfo &fin) {
  int r = OK;
  std::string file_buf;
  extent_protocol::attr a;
  if (ec->getattr(inum, a) != extent_protocol::OK) {
    r = IOERR;
  }
  if (fin.size == 0) {
    if (ec->put(inum, file_buf) != extent_protocol::OK) {
      r = IOERR;
      return r;
    }
  } else {
    if (ec->put(inum, file_buf) != extent_protocol::OK) {
      r = IOERR;
      return r;
    }
  }
  if (fin.size < a.size) {
    file_buf.assign((a.size - fin.size), '\0');
    r = write(inum, file_buf, fin.size, (a.size - fin.size));
  } else if (fin.size > a.size) {
    r = write(inum, file_buf, a.size, (fin.size - a.size));

  }
  r = OK;

}

  int
yfs_client::getdir(inum inum, dirinfo &din)
{
  int r = OK;

  printf("getdir %016llx\n", inum);
  extent_protocol::attr a;
  if (ec->getattr(inum, a) != extent_protocol::OK) {
    r = IOERR;
    printf("getdir error and got rlease \n");
    goto release;
  }
  din.atime = a.atime;
  din.mtime = a.mtime;
  din.ctime = a.ctime;

release:
    printf("getdir return OK \n");
  return r;
}


int yfs_client::getdirdata(inum inum, std::string & content){

  int r = yfs_client::OK;
  printf("getdirdata %016llx\n", inum);

  if (ec->get(inum, content) != extent_protocol::OK) {
    r = IOERR;
    goto release;
  }
  printf("getdirdata %016llx -> sz %d\n", inum, content.size() );
release:
  return r;
}

int yfs_client::put(inum inum, std::string content){
  int r = OK;
  printf("put %016llx\n", inum);
  if (ec->put(inum, content) != extent_protocol::OK) {
    r = IOERR;
    goto release;
  }
  printf("put %016llx -> sz %u\n", inum, content.size() );

release:
  return r;  
}

  yfs_client::status
yfs_client::read(inum inum, std::string &buf, size_t nbytes, off_t offset)
{
  int r = OK;
  fileinfo fin;
  if (getfile(inum,fin) != OK) {
    r = IOERR;
    return r;
  }
  size_t size = fin.size;
  if(offset > fin.size) {
    buf = '\0';
    r = OK;
    return r;
  }
  if ((offset + nbytes) > fin.size) {
    size = fin.size -offset;
  }
  std::string buf1;
  if (ec->get(inum, buf1) == extent_protocol::OK) {
    buf1.resize(offset,size);
    buf = buf1;
    return r;
  }
  else 
    return IOERR;

}

yfs_client::status
yfs_client::write(inum inum, std::string buf, size_t nbytes, off_t off)
{
  std::string file_buf = buf;
  int r = OK;
  size_t size = 0;
  fileinfo fin;
  if (getfile(inum, fin) != OK) {
    r = IOERR;
    return r;
  }
  if (off > fin.size) {
    file_buf.assign((off - fin.size), '\0');
    size = size + off - fin.size;
    off = fin.size;
    buf.resize(size);
  }
  if (ec->put(inum, file_buf) != extent_protocol::OK) {
    r = IOERR;
    return r;
  }

  r = OK;

  return r;
}
int yfs_client::lookup(inum p_inum, const char *name, inum &c_inum) {
  int r = OK, count = 0;
  std::string p_buf, inum_buf;
  char *cstr, *p;
  inum curr_inum;
// Read Parent Dir and check if name already exists
  if (ec->get(p_inum, p_buf) != extent_protocol::OK) {
      printf("yfs_client:lookup, get NOENT for %016llx\n", p_inum);
     r = NOENT;
     goto release;
  }
  cstr = new char[p_buf.size()+1];
  strcpy(cstr, p_buf.c_str());
  p=strtok (cstr, "/");
  while (p!=NULL) {
    // Skip its own dir name & inum
    if(count!=1 && count%2 == 1) {
	if((strlen(p) == strlen(name)) && (!strncmp(p, name, strlen(name)))) {
         delete[] cstr;
         r = OK;
	 c_inum = curr_inum;
         goto release;
      }
    }
    else {
      inum_buf = p;
      curr_inum = n2i(inum_buf);
    }
  p=strtok(NULL,"/");
  count++;
  }
  delete[] cstr;
  r = NOENT; 
  release:
    return r;
}
