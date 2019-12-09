// RPC stubs for clients to talk to extent_server

#include "rpc.h"
#include "extent_client_cache.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

int extent_client_cache::last_port = 1;
extent_client_cache::extent_client_cache()
{
}
extent_client_cache::extent_client_cache(std::string dst)
{
  sockaddr_in dstsock;

  make_sockaddr(dst.c_str(), &dstsock);
  cl = new rpcc(dstsock);

  if (cl->bind() != 0)
  {
    printf("extent_client: bind failed\n");
  }
  srand(time(NULL) ^ last_port);

  extent_port = ((rand() % 16000) | (0x1 << 10));
  std::string hname = "127.0.0.1";
  std::ostringstream host;
  host << hname << ":" << extent_port;
  id = host.str();
  last_port = extent_port;
  rpcs *rlsrpc = new rpcs(extent_port);
  rlsrpc->reg(rextent_protocol::refresh, this, &extent_client_cache::refresh);
}

// a demo to show how to use RPC
extent_protocol::status
extent_client_cache::create(uint32_t type, extent_protocol::extentid_t &id)
{
  extent_protocol::status ret = extent_protocol::OK;
  // Your lab2 part1 code goes here

  ret = cl->call(extent_protocol::create, type, this->id, id);

  std::cout << "ec:create " << id << std::endl;
  VERIFY(ret == extent_protocol::OK);

  struct extent_protocol::attr attr;
  struct timespec time1 = {0, 0};

  clock_gettime(CLOCK_REALTIME, &time1);
  attr.ctime = attr.mtime = attr.atime =
      time1.tv_sec * 1000 + time1.tv_nsec / 1000000;
  attr.size = 0;
  attr.type = type;
  this->attr_cache[id] = attr;
  return ret;
}

extent_protocol::status
extent_client_cache::get(extent_protocol::extentid_t eid, std::string &buf)
{

  extent_protocol::status ret = extent_protocol::OK;
  // Your lab2 part1 code goes here

  extent_protocol::str_and_attr d;

  std::cout << "ec get" << eid << std::endl;
  if (file_cache.find(eid) == file_cache.end())
  {
    ret = cl->call(extent_protocol::getattr, eid, id, d);//get file and attr at the same time
   
    this->file_cache[eid] = new data();
    std::string *local_buf = new std::string(d.content);
    this->file_cache[eid]->cbuf = local_buf->c_str();
    this->file_cache[eid]->size = local_buf->size();

    VERIFY(ret == extent_protocol::OK);
    attr_cache[eid] = d.attr; 
    buf = d.content;

    struct timespec time1 = {0, 0};
    clock_gettime(CLOCK_REALTIME, &time1);
    attr_cache[eid].atime =
        time1.tv_sec * 1000 + time1.tv_nsec / 1000000;
  }
  else
  {
    std::cout << "ec get:cache content size" << this->file_cache[eid]->size << std::endl;
    buf.assign(this->file_cache[eid]->cbuf, this->file_cache[eid]->size);
    std::cout << "ec get:cache content" << eid << buf << std::endl;
    // change atime

    struct timespec time1 = {0, 0};
    clock_gettime(CLOCK_REALTIME, &time1);
    attr_cache[eid].atime =
        time1.tv_sec * 1000 + time1.tv_nsec / 1000000;
  }
  return ret;
}

extent_protocol::status
extent_client_cache::getattr(extent_protocol::extentid_t eid,
                             extent_protocol::attr &attr)
{

  std::cout << "ec getattr" << eid << std::endl;
  extent_protocol::status ret = extent_protocol::OK;

  extent_protocol::str_and_attr d;

  std::string buf;
  if (attr_cache.find(eid) == attr_cache.end())
  {
    ret = cl->call(extent_protocol::getattr, eid, id, d);

    this->file_cache[eid] = new data();
    std::string *local_buf = new std::string(d.content);
    this->file_cache[eid]->cbuf = local_buf->c_str();
    this->file_cache[eid]->size = local_buf->size();

    VERIFY(ret == extent_protocol::OK);
    attr_cache[eid] = d.attr;
    attr = d.attr;

    std::cout << "ec: init cache from getattr size" << eid << local_buf->size() << std::endl;
  }
  else
  {
    attr = attr_cache[eid];
  }
  return ret;
}

extent_protocol::status
extent_client_cache::put(extent_protocol::extentid_t eid, std::string buf)
{

  std::cout << "ec put" << eid << std::endl;
  extent_protocol::status ret = extent_protocol::OK;
  if (attr_cache.find(eid) == attr_cache.end())
  {

    extent_protocol::str_and_attr d;
    ret = cl->call(extent_protocol::getattr, eid, id, d); //will corrupt data cache ,have to cover it later
    attr_cache[eid] = d.attr;
  }
  else
  {
    struct timespec time1 = {0, 0};
    clock_gettime(CLOCK_REALTIME, &time1);
    attr_cache[eid].ctime = attr_cache[eid].mtime = attr_cache[eid].atime =
        time1.tv_sec * 1000 + time1.tv_nsec / 1000000;
    attr_cache[eid].size = buf.size();
  }

  this->file_cache[eid] = new data();
  std::string *local_buf = new std::string(buf);
  this->file_cache[eid]->cbuf = local_buf->c_str();
  this->file_cache[eid]->size = local_buf->size();

  std::cout << "ec: init cache from put" << eid << buf << std::endl;
  return ret;
}

extent_protocol::status
extent_client_cache::remove(extent_protocol::extentid_t eid)
{
  extent_protocol::status ret = extent_protocol::OK;
  int r;
  ret = cl->call(extent_protocol::remove, eid, id, r);
  VERIFY(ret == extent_protocol::OK);
  if (file_cache.find(eid) != file_cache.end())
    file_cache.erase(eid);
  if (attr_cache.find(eid) != attr_cache.end())
    attr_cache.erase(eid);
  
  return ret;
}

rextent_protocol::status
extent_client_cache::flush(extent_protocol::extentid_t eid, int &)
{
  extent_protocol::status ret = extent_protocol::OK;
  int r;
  std::string buf;
  this->get(eid, buf);
  std::cout << "ec  flush inode buf size" << eid << buf.size() << std::endl;
  ret = cl->call(extent_protocol::flush, eid, buf, id, r);
  VERIFY(ret == extent_protocol::OK);

  std::cout << "ec  flush inode " << eid << " " << id << buf << std::endl;
  return ret;
}

rextent_protocol::status
extent_client_cache::refresh(extent_protocol::extentid_t eid, int &mode)
{

  extent_protocol::status ret = extent_protocol::OK;
  int r;
  std::string buf;

  std::cout << "ec  refresh " << eid << " " << id << buf << std::endl;
  if (mode == 1)
  {
    this->file_cache.clear();
    this->attr_cache.clear();
  }
  else
  {
    if (file_cache.find(eid) != file_cache.end())
      file_cache.erase(eid);
    if (attr_cache.find(eid) != attr_cache.end())
      attr_cache.erase(eid);
  }

  //update file_cache only currently
}