// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include "lock_client.h"
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
  return finum ;
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
  // You modify this function for Lab 3
  // - hold and release the file lock

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
  if (ec->get(inum, file_buf) != extent_protocol::OK) {
    r = IOERR;
    return r;
  }
  if (fin.size < file_buf.size()) {
    file_buf = file_buf.substr(0,fin.size);
  } else if (fin.size > file_buf.size()) {
    file_buf.append((fin.size - file_buf.size()), '\0');
  }
  if (ec->put(inum,file_buf) != extent_protocol::OK) {
    r = IOERR;
    return r;
  }
  r = OK;
  return r;

}

  int
yfs_client::getdir(inum inum, dirinfo &din)
{
  int r = OK;
  // You modify this function for Lab 3
  // - hold and release the directory lock

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

    printf("put error\n", inum);
    r = IOERR;
    goto release;
  }
  printf("put %016llx -> sz %d\n", inum, content.size() );

release:
  return r;  
}

  yfs_client::status
yfs_client::read(inum inum, std::string &buf, off_t offset, size_t nbytes)
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
    buf = buf1.substr(offset,size);
    std::cout << "Read buf:" << buf.size() << " size:" << size << " off" <<offset<<std::endl;;
    r = OK;
    return r;
  }
  else 
    std::cout << "Read buf failed"<<std::endl;
  return IOERR;

}

// Write @size bytes from @buf to file @ino, starting
// at byte offset @off in the file.
yfs_client::status
yfs_client::write(inum inum, std::string buf, off_t off, size_t nbytes)
{
  std::string file_buf;
  std::string to_write = buf;
  to_write.resize((int)nbytes);
  std::cout << "To write buf:" << buf.size() << " size:" << nbytes << " off" <<off<<std::endl;;
  int r = OK;
  fileinfo fin;
  if (getfile(inum, fin) != OK) {
    r = IOERR;
    std::cout << "write return IOERR\n";
    return r;
  }
  if (ec->get(inum, file_buf) != extent_protocol::OK) {
    std::cout <<"get content \n"<<std::endl;
    r = IOERR;
    return r;
  }
  std::cout << "write file size" << fin.size << "\n";
  std::cout << "write file buf size" << file_buf.size() << "\n";
  if (off > fin.size) {
    std::cout << "off = "<< off <<"\tfile size = "<<fin.size<<"\n";
    file_buf.append((off - fin.size), '\0');
    file_buf.append(to_write);
    std::cout << "added  tfile size = "<<file_buf.size() << "\n";
  }
  else {
          std::string padding = "";
        if(off + nbytes < file_buf.size()){
          std::cout<<padding<<std::endl;
          padding = file_buf.substr(off + nbytes);
        }
        file_buf = file_buf.substr(0, off) + to_write + padding;

  }


  if (ec->put(inum, file_buf) != extent_protocol::OK) {
    std::cout <<" write and put error \n"<<std::endl;
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
      printf("p_inum name alread exist %016llx\n", p_inum);
     r = NOENT;
     goto release;
  }
  cstr = new char[p_buf.size()+1];
  strcpy(cstr, p_buf.c_str());
  printf("check its name\n");
  p=strtok (cstr, " ");
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
  p=strtok(NULL," ");
  count++;
  }
  delete[] cstr;
  r = NOENT; 
  release:
    return r;
}
yfs_client::status
yfs_client::remove(inum inum) {
  ec->remove(inum);
}
