
#include "extent_server_cache.h"
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "handle.h"
extent_server_cache::extent_server_cache()
{
  im = new inode_manager();
}

int extent_server_cache::create(uint32_t type, std::string clientID, extent_protocol ::extentid_t &id)
{
  // alloc a new inode and return inum
  printf("extent_server: create inode\n");
  id = im->alloc_inode(type);

  //lab4 code
  if (cache_manager.find(id) == cache_manager.end())
    cache_manager[id] = new Entry();

  cache_manager[id]->caching_clients.insert(clientID);

  return extent_protocol ::OK;
}

int extent_server_cache::put(extent_protocol ::extentid_t id, std::string buf, std::string clientID, int &)
{

  id &= 0x7fffffff;
  int r;
  //lab4 code
  const char *cbuf = buf.c_str();
  int size = buf.size();
  im->write_file(id, cbuf, size);

  if (cache_manager.find(id) == cache_manager.end())
    cache_manager[id] = new Entry();

  cache_manager[id]->caching_clients.insert(clientID);

  if (cache_manager[id]->refreshing_clients.find(clientID) != cache_manager[id]->refreshing_clients.end())
  {

    cache_manager[id]->refreshing_clients.erase(clientID);
  }
  return extent_protocol ::OK;
}

int extent_server_cache::get(extent_protocol ::extentid_t id, std::string clientID, std::string &buf)
{
  id &= 0x7fffffff;

  int size = 0;
  char *cbuf = NULL;

  //lab4 code
  im->read_file(id, &cbuf, &size);
  if (size == 0)
    buf = "";
  else
  {
    buf.assign(cbuf, size);
    free(cbuf);
  }
  //int r;
  printf("extent_server: get %lld %s %s \n", id, clientID.c_str(), buf.c_str());

  std::cout << buf << std::endl;
  if (cache_manager.find(id) == cache_manager.end())
    cache_manager[id] = new Entry();

  cache_manager[id]->caching_clients.insert(clientID);
  if (cache_manager[id]->refreshing_clients.find(clientID) == cache_manager[id]->refreshing_clients.end())
  {
    cache_manager[id]->refreshing_clients.erase(clientID);
  }
  if (cache_manager[id]->state == FLUSHING)
  {

    printf("extent_server: get while flushing %lld %s\n", id, clientID.c_str());
    return extent_protocol::RETRY;
  }
  return extent_protocol ::OK;
}

int extent_server_cache::getattr(extent_protocol ::extentid_t id, std::string clientID, extent_protocol ::str_and_attr &d)
{
  printf("extent_server: getattr %lld\n", id);
  //getattr
  id &= 0x7fffffff;

  extent_protocol ::attr attr = d.attr;
  memset(&attr, 0, sizeof(attr));
  im->getattr(id, attr);
  d.attr = attr;
  //get
  int size = 0;
  char *cbuf = NULL;

  //lab4 code
  im->read_file(id, &cbuf, &size);
  if (size == 0)
    d.content = "";
  else
  {
    d.content.assign(cbuf, size);
    free(cbuf);
  }

  //lab4 code

  if (cache_manager.find(id) == cache_manager.end())
    cache_manager[id] = new Entry();

  cache_manager[id]->caching_clients.insert(clientID);
  return extent_protocol ::OK;
}

int extent_server_cache::remove(extent_protocol ::extentid_t id, std::string clientID, int &)
{
  printf("extent_server: write %lld\n", id);

  id &= 0x7fffffff;
  im->remove_file(id); //no need to refresh because ther must be a flush in the yfs
  int ret = signal_clear(id,clientID);
  return  ret ;
}

#include <arpa/inet.h>
int extent_server_cache::flush(extent_protocol ::extentid_t id, std::string buf, std::string clientID, int &)
{
  id &= 0x7fffffff;
  int r;
  //lab4 code
  const char *cbuf = buf.c_str();
  int size = buf.size();
  im->write_file(id, cbuf, size);
  printf("extent_server: flush %lld from %s\n ", id, clientID.c_str());

  std::cout << buf << std::endl;
  if (cache_manager.find(id) == cache_manager.end())
  {
    //do nothing
  }
  else
  {
    signal_clear(id,clientID);
    cache_manager[id]->caching_clients.insert(clientID);
  }

  return extent_protocol ::OK;
}

int extent_server_cache::signal_clear(extent_protocol ::extentid_t id,  std::string clientID )
{  
  int r;
  if (cache_manager.find(id) == cache_manager.end())
  {
    //do nothing
  }
  else
  {
    cache_manager[id]->caching_clients.erase(clientID);
    std::set<std::string>::iterator it = cache_manager[id]->caching_clients.begin();
    for (; it != cache_manager[id]->caching_clients.end(); it++)
    {
      std::string receiver = *it;                                          //clientid;
      handle(receiver).safebind()->call(rextent_protocol::refresh, id, r); //tell it to refresh cache
    }
    cache_manager[id]->refreshing_clients = cache_manager[id]->caching_clients; 
  } 
  return extent_protocol ::OK;
}
