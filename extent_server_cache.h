

#ifndef extent_server_cahce_h
#define extent_server_cahce_h

#include <string>
#include <map>
#include <set>
#include "extent_protocol.h"
#include "inode_manager.h"
//one inode could be:
enum inode_status
{
  //wait the client to flush,meanwhile all the actions are returned RETRY
  //while(ret !=extent_protocol::OK){//keep sending}
  NONE,       //no has cache of it,init state
  REFRESHING, //once flushing is done, the client to flush still has the right cache,
              //server tell clients to refresh cache,then redo the actions
  SYNC,       //once refreshing is done, all caches are synchronized
  FLUSHING,
};
struct Entry
{
  inode_status state;
  std::set<std::string> caching_clients;
  std::string flushing_client;
  std::set<std::string> refreshing_clients;
};
class extent_server_cache
{
protected:
  inode_manager *im;
  std::map<extent_protocol::extentid_t, Entry *> cache_manager;

public:
  extent_server_cache();

  int create(uint32_t type, std::string clientID, extent_protocol::extentid_t &id);
  int put(extent_protocol::extentid_t id, std::string, std::string clientID, int &);
  int get(extent_protocol::extentid_t id, std::string clientID, std::string &buf);
  int getattr(extent_protocol::extentid_t id, std::string clientID,extent_protocol::str_and_attr &);
  int remove(extent_protocol::extentid_t id, std::string clientID, int &);
  int flush(extent_protocol::extentid_t id, std::string, std::string clientID, int &);
  int signal_clear(extent_protocol ::extentid_t id,  std::string clientID );
};

#endif
