// lock client interface.

#ifndef lock_client_cache_h

#define lock_client_cache_h

#include <queue>
#include <string>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_client.h"
#include "lang/verify.h"

// Classes that inherit lock_release_user can override dorelease so that
// that they will be called when lock_client releases a lock.
// You will not need to do anything with this class until Lab 6.
class lock_release_user
{
public:
  virtual void dorelease(lock_protocol::lockid_t) = 0;
  virtual ~lock_release_user(){};
};

struct QueuingThread
{

  pthread_cond_t cv;

  QueuingThread()
  {
    pthread_cond_init(&cv, NULL);
  }
};

class lock_client_cache : public lock_client
{
private:
  class lock_release_user *lu;

  int rlock_port;
  std::string hostname;
  std::string id;

  enum Message
  {
    EMPTY,
    RETRY,
    REVOKE
  };

  enum LockState
  {
    NONE = 0,
    ACQUIRING,
    FREE,
    RELEASING,
    LOCKED,
  };

  struct QueuingThread
  {

    pthread_cond_t cv;

    QueuingThread()
    {
      pthread_cond_init(&cv, NULL);
    }
  };

  struct LockEntry
  {
    LockState state;
    Message message;
    std::list<QueuingThread *> threads;

    LockEntry()
    {
      state = NONE;
      message = EMPTY;
    }
  };

  pthread_mutex_t lockManagerLock;

  std::map<lock_protocol::lockid_t, LockEntry *> lockManager;

  lock_protocol::status blockUntilGot(LockEntry *,
                                      lock_protocol::lockid_t,
                                      QueuingThread *thisThread);

public:
  static int last_port;

  lock_client_cache(std::string xdst, class lock_release_user *l = 0);

  virtual ~lock_client_cache();

  lock_protocol::status acquire(lock_protocol::lockid_t);

  lock_protocol::status release(lock_protocol::lockid_t);

  rlock_protocol::status revoke_handler(lock_protocol::lockid_t,
                                        int &);

  rlock_protocol::status retry_handler(lock_protocol::lockid_t,
                                       int &);
};

#endif
