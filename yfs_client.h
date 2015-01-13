#ifndef yfs_client_h
#define yfs_client_h

#include <string>
//#include "yfs_protocol.h"
#include "extent_client.h"
#include <vector>

#include "lock_protocol.h"
#include "lock_client.h"

class yfs_client {
  extent_client *ec;
  lock_client *lc;
 public:

  typedef unsigned long long inum;
  enum xxstatus { OK, RPCERR, NOENT, IOERR, EXIST };
  typedef int status;

  struct fileinfo {
    unsigned long long size;
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirinfo {
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirent {
    std::string name;
    yfs_client::inum inum;
  };

 private:
 public:
  static inum n2i(std::string);

  static std::string filename(inum);
  yfs_client(std::string, std::string);
  bool isfile(inum);
  bool isdir(inum);
  int getfile(inum, fileinfo &);
  int getdir(inum, dirinfo &);
  int getdirdata(inum, std::string &);
  int put(inum,std::string); 
  int setattr(inum inum, fileinfo &fin);
  status read(inum inum, std::string &buf, off_t offset, size_t nbytes);
  status write(inum inum, std::string buf,  off_t offset, size_t nbytes);
  status remove(inum inum);
  int lookup(inum p_inum, const char *name, inum &c_inum);
};

#endif 
