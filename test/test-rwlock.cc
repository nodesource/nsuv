#include "../include/nsuv-inl.h"
#include "./catch.hpp"

using nsuv::ns_rwlock;

TEST_CASE("rwlock", "[rwlock]") {
  ns_rwlock lock;
  int r;

  r = lock.init();
  REQUIRE(r == 0);

  lock.rdlock();
  REQUIRE(lock.trywrlock() == UV_EBUSY);
  REQUIRE(lock.tryrdlock() == 0);
  lock.rdunlock();
  lock.rdunlock();
  REQUIRE(lock.trywrlock() == 0);
  REQUIRE(lock.tryrdlock() == UV_EBUSY);
  lock.wrunlock();
  REQUIRE(lock.tryrdlock() == 0);
  lock.rdunlock();
  lock.destroy();
  REQUIRE(lock.destroyed());
}


TEST_CASE("rwlock_init", "[rwlock]") {
  ns_rwlock lock;
  int r;

  r = lock.init(true);
  REQUIRE(r == 0);

  lock.rdlock();
  REQUIRE(lock.trywrlock() == UV_EBUSY);
  REQUIRE(lock.tryrdlock() == 0);
  lock.rdunlock();
  lock.rdunlock();
  REQUIRE(lock.trywrlock() == 0);
  REQUIRE(lock.tryrdlock() == UV_EBUSY);
  lock.wrunlock();
  REQUIRE(lock.tryrdlock() == 0);
  lock.rdunlock();
}


TEST_CASE("rwlock_auto", "[rwlock]") {
  int r;
  ns_rwlock lock(&r);
  REQUIRE(r == 0);

  lock.rdlock();
  REQUIRE(lock.trywrlock() == UV_EBUSY);
  REQUIRE(lock.tryrdlock() == 0);
  lock.rdunlock();
  lock.rdunlock();
  REQUIRE(lock.trywrlock() == 0);
  REQUIRE(lock.tryrdlock() == UV_EBUSY);
  lock.wrunlock();
  REQUIRE(lock.tryrdlock() == 0);
  lock.rdunlock();
}


TEST_CASE("rwlock_rdlock_scoped", "[rwlock]") {
  ns_rwlock lock;
  int r;

  r = lock.init();
  REQUIRE(r == 0);

  {
    ns_rwlock::scoped_rdlock rdlock(&lock);
    REQUIRE(lock.trywrlock() == UV_EBUSY);
    REQUIRE(lock.tryrdlock() == 0);
    lock.rdunlock();
    REQUIRE(lock.trywrlock() == UV_EBUSY);
  }

  REQUIRE(lock.trywrlock() == 0);
  lock.wrunlock();
  lock.destroy();
  REQUIRE(lock.destroyed());
}


TEST_CASE("rwlock_rdlock_const_scoped", "[rwlock]") {
  ns_rwlock lock;
  int r;

  r = lock.init();
  REQUIRE(r == 0);

  {
    ns_rwlock::scoped_rdlock rdlock(lock);
    REQUIRE(lock.trywrlock() == UV_EBUSY);
    REQUIRE(lock.tryrdlock() == 0);
    lock.rdunlock();
    REQUIRE(lock.trywrlock() == UV_EBUSY);
  }

  REQUIRE(lock.trywrlock() == 0);
  lock.wrunlock();
  lock.destroy();
  REQUIRE(lock.destroyed());
}


TEST_CASE("rwlock_wrlock_scoped", "[rwlock]") {
  ns_rwlock lock;
  int r;

  r = lock.init();
  REQUIRE(r == 0);

  {
    ns_rwlock::scoped_wrlock wrlock(&lock);
    REQUIRE(lock.trywrlock() == UV_EBUSY);
    REQUIRE(lock.tryrdlock() == UV_EBUSY);
  }

  REQUIRE(lock.trywrlock() == 0);
  REQUIRE(lock.tryrdlock() == UV_EBUSY);
  lock.wrunlock();
  REQUIRE(lock.tryrdlock() == 0);
  lock.rdunlock();
  lock.destroy();
  REQUIRE(lock.destroyed());
}


TEST_CASE("rwlock_wrlock_const_scoped", "[rwlock]") {
  ns_rwlock lock;
  int r;

  r = lock.init();
  REQUIRE(r == 0);

  {
    ns_rwlock::scoped_wrlock wrlock(lock);
    REQUIRE(lock.trywrlock() == UV_EBUSY);
    REQUIRE(lock.tryrdlock() == UV_EBUSY);
  }

  REQUIRE(lock.trywrlock() == 0);
  REQUIRE(lock.tryrdlock() == UV_EBUSY);
  lock.wrunlock();
  REQUIRE(lock.tryrdlock() == 0);
  lock.rdunlock();
  lock.destroy();
  REQUIRE(lock.destroyed());
}
