

#ifndef extent_client_cache_h
#define extent_client_cache_h

#include <string>
#include "extent_protocol.h"
//#include "extent_server_cache.h"

struct data
{
  const char *cbuf;
  int size;
};
enum state{GET,REFRESH,NOTHING,REFRESHED};
class extent_client_cache
{
private: 
  static int last_port;
  std::string id;
  int extent_port;
  rpcc *cl;
  std::string buffer;
  state rpc_state;
  std::map<extent_protocol::extentid_t, data *> file_cache;
 
  std::map<extent_protocol::extentid_t, extent_protocol::attr> attr_cache;

public:
  extent_client_cache();

  extent_client_cache(std::string dst);

  extent_protocol::status create(uint32_t type, extent_protocol::extentid_t &eid);
  extent_protocol::status get(extent_protocol::extentid_t eid,
                              std::string &buf);
  extent_protocol::status getattr(extent_protocol::extentid_t eid,
                                  extent_protocol::attr &a);
  extent_protocol::status put(extent_protocol::extentid_t eid, std::string buf);
  extent_protocol::status remove(extent_protocol::extentid_t eid);

  rextent_protocol::status flush(extent_protocol::extentid_t eid,int &);

  rextent_protocol::status refresh(extent_protocol::extentid_t eid,int &);
};

#endif
