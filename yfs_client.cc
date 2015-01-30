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



int limited_rand(int limit)
{
  int r, d = RAND_MAX / limit;
  limit *= d;
  do { r = rand(); } while (r >= limit);
  return r / d;
}

yfs_client::inum yfs_client::new_inum(bool isfile)
{
  yfs_client::inum finum;
  int rand_num = limited_rand(0x7FFFFFFF);
  if (isfile) 
    finum = rand_num | 0x0000000080000000;
  else 
    finum = rand_num & 0x000000007FFFFFFF;
  printf("new_inum %016llx \n",finum);
  return finum;
}

#if 0
yfs_client::inum yfs_client::new_inum(bool isdir){
    int r = rand() * rand();
    if(isdir == false){
        r |=  0x80000000;
    }
    else
        r &= 0x7FFFFFFF;
    return r;
}
bool rrr = false;
yfs_client::inum yfs_client::new_inum(bool file){
  if(!rrr){
    srand(time(NULL));
    rrr= true;
  }
  unsigned int i = rand();
  if (file){
    i = i | 0x80000000;    
  } else {
    i = i & 0x7FFFFFFF;
  }
  //   printf("inum danach: %i",i);
  return i;
}
#endif

yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
  ec = new extent_client(extent_dst);
  lc = new lock_client(lock_dst);
  const inum root = 0x00000001;
  std::string buf("/root/" + filename(root));
  ec->put(root, buf);
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

  //printf("getfile %016llx\n", inum);
  extent_protocol::attr a;
  lc->acquire(inum);
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
  lc->release(inum);
  return r;
}
int 
yfs_client::setattr(inum inum, fileinfo &fin) {
  int r = OK;
  std::string file_buf;
  lc->acquire(inum);
  if (ec->get(inum, file_buf) != extent_protocol::OK) {
    lc->release(inum);
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
    lc->release(inum);
    return r;
  }
  r = OK;
  lc->release(inum);
  return r;

}

  int
yfs_client::getdir(inum inum, dirinfo &din)
{
  int r = OK;
  // You modify this function for Lab 3
  // - hold and release the directory lock

  extent_protocol::attr a;
  lc->acquire(inum);
  if (ec->getattr(inum, a) != extent_protocol::OK) {
    r = IOERR;
    printf("getdir error and got rlease \n");
    goto release;
  }
  din.atime = a.atime;
  din.mtime = a.mtime;
  din.ctime = a.ctime;

release:
  lc->release(inum);
  printf("getdir return OK \n");
  return r;
}


int yfs_client::getdirdata(inum inum, std::string & content){

  int r = yfs_client::OK;
  printf("getdirdata %016llx\n", inum);

  lc->acquire(inum);
  if (ec->get(inum, content) != extent_protocol::OK) {
    r = IOERR;
    goto release;
  }
  printf("getdirdata %016llx -> sz %d\n", inum, content.size() );
release:
  lc->release(inum);
  return r;
}

int yfs_client::put(inum inum, std::string content){
  int r = OK;
  printf("put %016llx\n", inum);
  lc->acquire(inum);
  if (ec->put(inum, content) != extent_protocol::OK) {

    printf("put error\n", inum);
    r = IOERR;
    goto release;
  }
  printf("put %016llx -> sz %d\n", inum, content.size() );

release:
  lc->release(inum);
  return r;  
}

  yfs_client::status
yfs_client::read(inum inum, std::string &buf, off_t offset, size_t nbytes)
{
  int r = OK;
  lc->acquire(inum);
  extent_protocol::attr a;
  if (ec->getattr(inum, a) != extent_protocol::OK) {
    r = IOERR;
    printf("get attr error when yfs read\n");
    lc->release(inum);
    return r;
  }

  size_t filesize = a.size;
  if(offset > filesize) {
    r = OK;
    lc->release(inum);
    return r;
  }
  if ((offset + nbytes) > filesize) {
    nbytes = filesize -offset;
  }
  std::string buf1;
  if (ec->get(inum, buf1) == extent_protocol::OK) {
    buf = buf1.substr(offset,nbytes);
    std::cout << "Read buf:" << buf.size() << " file size:" << filesize << " offset" <<offset<<std::endl;;
    std::cout << "get Buf content" << buf << std::endl;
    r = OK;
    lc->release(inum);
    return r;
  }
  else 
    std::cout << "Read buf failed"<<std::endl;
  lc->release(inum);
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
#if 0
  fileinfo fin;
  if (getfile(inum, fin) != OK) {
    r = IOERR;
    std::cout << "write return IOERR\n";
    return r;
  }
#endif
  lc->acquire(inum);
  extent_protocol::attr a;
  if (ec->getattr(inum, a) != extent_protocol::OK) {
    r = IOERR;
    lc->release(inum);
    return r;
  }
  int finsize = a.size;
  if (ec->get(inum, file_buf) != extent_protocol::OK) {
    std::cout <<"yfs write get content \n"<<std::endl;
    r = IOERR;
    lc->release(inum);
    return r;
  }
  std::cout << "write file buf size" << file_buf.size() << "\n";
  if (off > finsize) {
    std::cout << "off = "<< off <<"\tfile size = "<<finsize<<"\n";
    file_buf.append((off - finsize), '\0');
    file_buf.append(to_write);
    std::cout << "added  tfile size = "<<file_buf.size() << "\n";
  }
  else {
    std::string padding = "";
    if(off + nbytes < file_buf.size()){
      padding = file_buf.substr(off + nbytes);
    }
    file_buf = file_buf.substr(0, off) + to_write + padding;

  }


  if (ec->put(inum, file_buf) != extent_protocol::OK) {
    std::cout <<" write and put error \n"<<std::endl;
    r = IOERR;
    lc->release(inum);
    return r;
  }
  lc->release(inum);

  r = OK;

  return r;
}
int yfs_client::lookup(inum p_inum, const char *name, inum &c_inum) {
  int r = OK, count = 0;
  std::string p_buf, inum_buf;
  char *cstr, *p;
  inum curr_inum;
  // Read Parent Dir and check if name already exists
  lc->acquire(p_inum);
  if (ec->get(p_inum, p_buf) != extent_protocol::OK) {
    printf("p_inum name no entry  %016llx\n", p_inum);
    r = NOENT;
    goto release;
  }


  bool found = true;
  auto firstfound = std::string::npos;
  auto sencondfound = std::string::npos; 
  do {
    firstfound = p_buf.find(name);
  } while(firstfound != std::string::npos && secondfound != std::string::npos);






  cstr = new char[p_buf.size()+1];
  strcpy(cstr, p_buf.c_str());
  printf("check its name\n");
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
  lc->release(p_inum);
  return r;
}
yfs_client::status
yfs_client::remove(inum num) {
  lc->acquire(num);
  ec->remove(num);
  lc->release(num);
}

yfs_client::status
yfs_client::create(yfs_client::inum parent, const char * name, inum &fnum, bool isfile) {
  int r = NOENT;
  if( isdir(parent) ) {
    std::string content;
    lc->acquire(parent);
    //get parent infor
    if (ec->get(parent, content) != extent_protocol::OK) {
      std::cout << "get parent info: *******error" << std::endl;
      lc->release(parent);
      r = IOERR;
      return r;
    }
    std::string sname(name);
    std::cout << "parent: " << content <<"name: " << sname << std::endl;
    sname = "/"+sname+"/";
    int npos;
    if((npos = content.find(sname)) != std::string::npos) {
        std::cout << "exist..............." <<std::endl;
        r =  EXIST;
        lc->release(parent);
        return r;
#if 0
      if((npos == 0) || (content[npos-1] == '/') ) {
        std::cout << "exist..............." <<std::endl;
        r =  EXIST;
        lc->release(parent);
        return r;
      }
#endif
    }
    yfs_client::inum filenum = new_inum(isfile);
    printf("random number = %d\n",filenum);
    content.append("/");
    content.append(name);
    content.append("/");
    content.append(filename(filenum)); //(value, string&, base)
    printf("  fuseserver_mkdir: name and id %s\n", content.c_str());
    yfs_client::inum ii = parent;
    // - Add a <name, ino> entry into @parent.
    ec->put(ii,content);
    // - Create an empty extent for ino.
    if(isfile) {
        ec->put(filenum,std::string(""));
    }
    else {
      ec->put(filenum,sname+filename(filenum)+"/");
    }
    lc->release(parent);

    //set entry parameters
    fnum = filenum;
    r = OK;
  }
  return r;
}


  yfs_client::status
yfs_client::unlink(yfs_client::inum parent, const char *name)
{

  int r = OK;
  std::string content;
  lc->acquire(parent);
  if (ec->get(parent, content) != extent_protocol::OK) {
    r = IOERR;
    lc->release(parent);
    return r;
  }
  auto  found = content.find(name);
  if (found != std::string::npos) {
    std::string strnum;
    std::string newcontent;
    auto firstspace = content.find('/',found);
    auto secondspace = content.find('/',firstspace+1);
    if(secondspace == std::string::npos) { 
      strnum = content.substr(firstspace+1);
    }
    else 
      strnum = content.substr(firstspace+1, secondspace-firstspace-1);
    auto num = n2i(strnum);
    ec->remove(num);
    std::string a(name);
    printf("Previous content %s\n",content.c_str());
    newcontent = content.substr(0,found)+content.substr(secondspace);
    printf("New content %s\n",newcontent.c_str());
    if (ec->put(parent, newcontent) != extent_protocol::OK) {
      r = IOERR;
      lc->release(parent);
      return r;
    }

  }
  lc->release(parent);
  return r;

}
