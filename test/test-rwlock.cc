#include "../include/nsuv-inl.h"
#include "./helpers.h"

using nsuv::ns_rwlock;

TEST_CASE("rwlock", "[rwlock]") {
  ns_rwlock lock;
  int r;

  r = lock.init();
  ASSERT_EQ(r, 0);

  lock.rdlock();
  ASSERT_EQ(lock.trywrlock(), UV_EBUSY);
  ASSERT_EQ(lock.tryrdlock(), 0);
  lock.rdunlock();
  lock.rdunlock();
  ASSERT_EQ(lock.trywrlock(), 0);
  ASSERT_EQ(lock.tryrdlock(), UV_EBUSY);
  lock.wrunlock();
  ASSERT_EQ(lock.tryrdlock(), 0);
  lock.rdunlock();
  lock.destroy();
  ASSERT(lock.destroyed());
}


TEST_CASE("rwlock_init", "[rwlock]") {
  ns_rwlock lock;
  int r;

  r = lock.init(true);
  ASSERT_EQ(r, 0);

  lock.rdlock();
  ASSERT_EQ(lock.trywrlock(), UV_EBUSY);
  ASSERT_EQ(lock.tryrdlock(), 0);
  lock.rdunlock();
  lock.rdunlock();
  ASSERT_EQ(lock.trywrlock(), 0);
  ASSERT_EQ(lock.tryrdlock(), UV_EBUSY);
  lock.wrunlock();
  ASSERT_EQ(lock.tryrdlock(), 0);
  lock.rdunlock();
}


TEST_CASE("rwlock_auto", "[rwlock]") {
  int r;
  ns_rwlock lock(&r);
  ASSERT_EQ(r, 0);

  lock.rdlock();
  ASSERT_EQ(lock.trywrlock(), UV_EBUSY);
  ASSERT_EQ(lock.tryrdlock(), 0);
  lock.rdunlock();
  lock.rdunlock();
  ASSERT_EQ(lock.trywrlock(), 0);
  ASSERT_EQ(lock.tryrdlock(), UV_EBUSY);
  lock.wrunlock();
  ASSERT_EQ(lock.tryrdlock(), 0);
  lock.rdunlock();
}


TEST_CASE("rwlock_rdlock_scoped", "[rwlock]") {
  ns_rwlock lock;
  int r;

  r = lock.init();
  ASSERT_EQ(r, 0);

  {
    ns_rwlock::scoped_rdlock rdlock(&lock);
    ASSERT_EQ(lock.trywrlock(), UV_EBUSY);
    ASSERT_EQ(lock.tryrdlock(), 0);
    lock.rdunlock();
    ASSERT_EQ(lock.trywrlock(), UV_EBUSY);
  }

  ASSERT_EQ(lock.trywrlock(), 0);
  lock.wrunlock();
  lock.destroy();
  ASSERT(lock.destroyed());
}


TEST_CASE("rwlock_rdlock_const_scoped", "[rwlock]") {
  ns_rwlock lock;
  int r;

  r = lock.init();
  ASSERT_EQ(r, 0);

  {
    ns_rwlock::scoped_rdlock rdlock(lock);
    ASSERT_EQ(lock.trywrlock(), UV_EBUSY);
    ASSERT_EQ(lock.tryrdlock(), 0);
    lock.rdunlock();
    ASSERT_EQ(lock.trywrlock(), UV_EBUSY);
  }

  ASSERT_EQ(lock.trywrlock(), 0);
  lock.wrunlock();
  lock.destroy();
  ASSERT(lock.destroyed());
}


TEST_CASE("rwlock_wrlock_scoped", "[rwlock]") {
  ns_rwlock lock;
  int r;

  r = lock.init();
  ASSERT_EQ(r, 0);

  {
    ns_rwlock::scoped_wrlock wrlock(&lock);
    ASSERT_EQ(lock.trywrlock(), UV_EBUSY);
    ASSERT_EQ(lock.tryrdlock(), UV_EBUSY);
  }

  ASSERT_EQ(lock.trywrlock(), 0);
  ASSERT_EQ(lock.tryrdlock(), UV_EBUSY);
  lock.wrunlock();
  ASSERT_EQ(lock.tryrdlock(), 0);
  lock.rdunlock();
  lock.destroy();
  ASSERT(lock.destroyed());
}


TEST_CASE("rwlock_wrlock_const_scoped", "[rwlock]") {
  ns_rwlock lock;
  int r;

  r = lock.init();
  ASSERT_EQ(r, 0);

  {
    ns_rwlock::scoped_wrlock wrlock(lock);
    ASSERT_EQ(lock.trywrlock(), UV_EBUSY);
    ASSERT_EQ(lock.tryrdlock(), UV_EBUSY);
  }

  ASSERT_EQ(lock.trywrlock(), 0);
  ASSERT_EQ(lock.tryrdlock(), UV_EBUSY);
  lock.wrunlock();
  ASSERT_EQ(lock.tryrdlock(), 0);
  lock.rdunlock();
  lock.destroy();
  ASSERT(lock.destroyed());
}
