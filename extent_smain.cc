#include "rpc.h"
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include "extent_server_cache.h"
#include <unistd.h>
// Main loop of extent server

int
main(int argc, char *argv[])
{
  int count = 0;

  if(argc != 2){
    fprintf(stderr, "Usage: %s port\n", argv[0]);
    exit(1);
  }

  setvbuf(stdout, NULL, _IONBF, 0);

  char *count_env = getenv("RPC_COUNT");
  if(count_env != NULL){
    count = atoi(count_env);
  }

  rpcs server(atoi(argv[1]), count);
  extent_server_cache ls;

  server.reg(extent_protocol::get, &ls, &extent_server_cache::get);
  server.reg(extent_protocol::getattr, &ls, &extent_server_cache::getattr);
  server.reg(extent_protocol::put, &ls, &extent_server_cache::put);
  server.reg(extent_protocol::remove, &ls, &extent_server_cache::remove);
  server.reg(extent_protocol::create, &ls, &extent_server_cache::create);
//lab4 new add
  server.reg(extent_protocol::flush, &ls, &extent_server_cache::flush);
 
  while(1)
    sleep(1000);
}
