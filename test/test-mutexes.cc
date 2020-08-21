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
  REQUIRE(mutex.destroyed());
}

TEST_CASE("thread_mutex_init", "[mutex]") {
  ns_mutex mutex;
  int r;

  r = mutex.init(true);
  REQUIRE(r == 0);

  mutex.lock();
  REQUIRE(mutex.trylock() == UV_EBUSY);
  mutex.unlock();
  REQUIRE(mutex.trylock() == 0);
  mutex.unlock();
}


TEST_CASE("thread_mutex_auto", "[mutex]") {
  int r;
  ns_mutex mutex(&r);
  REQUIRE(r == 0);

  mutex.lock();
  REQUIRE(mutex.trylock() == UV_EBUSY);
  mutex.unlock();
  REQUIRE(mutex.trylock() == 0);
  mutex.unlock();
}


TEST_CASE("thread_mutex_scoped", "[mutex]") {
  ns_mutex mutex;
  int r;

  r = mutex.init();
  REQUIRE(r == 0);

  {
    ns_mutex::scoped_lock lock(&mutex);
    REQUIRE(mutex.trylock() == UV_EBUSY);
    mutex.unlock();
    REQUIRE(mutex.trylock() == 0);
  }
  mutex.destroy();
  REQUIRE(mutex.destroyed());
}


TEST_CASE("thread_mutex_auto_scoped", "[mutex]") {
  int r;
  ns_mutex mutex(&r);
  REQUIRE(r == 0);

  {
    ns_mutex::scoped_lock lock(&mutex);
    REQUIRE(mutex.trylock() == UV_EBUSY);
    mutex.unlock();
    REQUIRE(mutex.trylock() == 0);
  }
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
  REQUIRE(mutex.destroyed());
}


TEST_CASE("thread_mutex_recursive_auto", "[mutex]") {
  int r;
  ns_mutex mutex(&r, true);
  REQUIRE(r == 0);

  mutex.lock();
  mutex.lock();
  REQUIRE(mutex.trylock() == 0);

  mutex.unlock();
  mutex.unlock();
  mutex.unlock();
}


TEST_CASE("thread_mutex_recursive_scoped", "[mutex]") {
  ns_mutex mutex;
  int r;

  r = mutex.init_recursive();
  REQUIRE(r == 0);

  {
    ns_mutex::scoped_lock lock1(&mutex);
    {
      ns_mutex::scoped_lock lock2(&mutex);
      REQUIRE(mutex.trylock() == 0);
      mutex.unlock();
    }
  }

  mutex.destroy();
  REQUIRE(mutex.destroyed());
}


TEST_CASE("thread_mutex_recursive_auto_scoped", "[mutex]") {
  int r;
  ns_mutex mutex(&r, true);
  REQUIRE(r == 0);

  {
    ns_mutex::scoped_lock lock1(&mutex);
    {
      ns_mutex::scoped_lock lock2(&mutex);
      REQUIRE(mutex.trylock() == 0);
      mutex.unlock();
    }
  }
}
