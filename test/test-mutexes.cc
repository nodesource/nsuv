#include "../include/nsuv-inl.h"
#include "./catch.hpp"

using nsuv::ns_mutex;

TEST_CASE("thread_mutex", "[mutex]") {
  ns_mutex mutex;
  int r;

  r = mutex.init();
  REQUIRE(r == 0);

  mutex.lock();
  REQUIRE(mutex.trylock() == UV_EBUSY);
  mutex.unlock();
  REQUIRE(mutex.trylock() == 0);
  mutex.unlock();
  mutex.destroy();
}


TEST_CASE("thread_mutex_recursive", "[mutex]") {
  ns_mutex mutex;
  int r;

  r = mutex.init_recursive();
  REQUIRE(r == 0);

  mutex.lock();
  mutex.lock();
  REQUIRE(mutex.trylock() == 0);

  mutex.unlock();
  mutex.unlock();
  mutex.unlock();
  mutex.destroy();
}
