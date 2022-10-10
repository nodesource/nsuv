#include "../include/nsuv-inl.h"
#include "./helpers.h"

using nsuv::ns_mutex;
using nsuv::ns_rwlock;
using nsuv::ns_thread;

static uv_cond_t condvar;
static ns_mutex mutex;
static ns_rwlock rwlock;
static int step;

TEST_CASE("thread_mutex", "[mutex]") {
  ns_mutex mutex;
  int r;

  r = mutex.init();
  ASSERT_EQ(r, 0);

  mutex.lock();
  ASSERT_EQ(mutex.trylock(), UV_EBUSY);
  mutex.unlock();
  ASSERT_EQ(mutex.trylock(), 0);
  mutex.unlock();
  mutex.destroy();
  ASSERT(mutex.destroyed());
}


TEST_CASE("thread_mutex_init", "[mutex]") {
  ns_mutex mutex;
  int r;

  r = mutex.init(true);
  ASSERT_EQ(r, 0);

  mutex.lock();
  ASSERT_EQ(mutex.trylock(), UV_EBUSY);
  mutex.unlock();
  ASSERT_EQ(mutex.trylock(), 0);
  mutex.unlock();
}


TEST_CASE("thread_mutex_auto", "[mutex]") {
  int r;
  ns_mutex mutex(&r);
  ASSERT_EQ(r, 0);

  mutex.lock();
  ASSERT_EQ(mutex.trylock(), UV_EBUSY);
  mutex.unlock();
  ASSERT_EQ(mutex.trylock(), 0);
  mutex.unlock();
}


TEST_CASE("thread_mutex_scoped", "[mutex]") {
  ns_mutex mutex;
  int r;

  r = mutex.init();
  ASSERT_EQ(r, 0);

  {
    ns_mutex::scoped_lock lock(&mutex);
    ASSERT_EQ(mutex.trylock(), UV_EBUSY);
    mutex.unlock();
    ASSERT_EQ(mutex.trylock(), 0);
  }
  mutex.destroy();
  ASSERT(mutex.destroyed());
}


TEST_CASE("thread_mutex_auto_scoped", "[mutex]") {
  int r;
  ns_mutex mutex(&r);
  ASSERT_EQ(r, 0);

  {
    ns_mutex::scoped_lock lock(&mutex);
    ASSERT_EQ(mutex.trylock(), UV_EBUSY);
    mutex.unlock();
    ASSERT_EQ(mutex.trylock(), 0);
  }
}


TEST_CASE("thread_mutex_const_scoped", "[mutex]") {
  ns_mutex mutex;
  int r;

  r = mutex.init();
  ASSERT_EQ(r, 0);

  {
    ns_mutex::scoped_lock lock(mutex);
    ASSERT_EQ(mutex.trylock(), UV_EBUSY);
    mutex.unlock();
    ASSERT_EQ(mutex.trylock(), 0);
  }
  mutex.destroy();
  ASSERT(mutex.destroyed());
}


TEST_CASE("thread_mutex_recursive", "[mutex]") {
  ns_mutex mutex;
  int r;

  r = mutex.init_recursive();
  ASSERT_EQ(r, 0);

  mutex.lock();
  mutex.lock();
  ASSERT_EQ(mutex.trylock(), 0);

  mutex.unlock();
  mutex.unlock();
  mutex.unlock();
  mutex.destroy();
  ASSERT(mutex.destroyed());
}


TEST_CASE("thread_mutex_recursive_auto", "[mutex]") {
  int r;
  ns_mutex mutex(&r, true);
  ASSERT_EQ(r, 0);

  mutex.lock();
  mutex.lock();
  ASSERT_EQ(mutex.trylock(), 0);

  mutex.unlock();
  mutex.unlock();
  mutex.unlock();
}


TEST_CASE("thread_mutex_recursive_scoped", "[mutex]") {
  ns_mutex mutex;
  int r;

  r = mutex.init_recursive();
  ASSERT_EQ(r, 0);

  {
    ns_mutex::scoped_lock lock1(&mutex);
    {
      ns_mutex::scoped_lock lock2(&mutex);
      ASSERT_EQ(mutex.trylock(), 0);
      mutex.unlock();
    }
  }

  mutex.destroy();
  ASSERT(mutex.destroyed());
}


TEST_CASE("thread_mutex_const_recursive_scoped", "[mutex]") {
  ns_mutex mutex;
  int r;

  r = mutex.init_recursive();
  ASSERT_EQ(r, 0);

  {
    ns_mutex::scoped_lock lock1(mutex);
    {
      ns_mutex::scoped_lock lock2(mutex);
      ASSERT_EQ(mutex.trylock(), 0);
      mutex.unlock();
    }
  }

  mutex.destroy();
  ASSERT(mutex.destroyed());
}


TEST_CASE("thread_mutex_recursive_auto_scoped", "[mutex]") {
  int r;
  ns_mutex mutex(&r, true);
  ASSERT_EQ(r, 0);

  {
    ns_mutex::scoped_lock lock1(&mutex);
    {
      ns_mutex::scoped_lock lock2(&mutex);
      ASSERT_EQ(mutex.trylock(), 0);
      mutex.unlock();
    }
  }
}


TEST_CASE("thread_rwlock", "[mutex]") {
  ns_rwlock rwlock;
  int r;

  r = rwlock.init();
  ASSERT_EQ(r, 0);

  rwlock.rdlock();
  rwlock.rdunlock();
  rwlock.wrlock();
  rwlock.wrunlock();

  {
    ns_rwlock::scoped_rdlock lock(rwlock);
  }
  {
    ns_rwlock::scoped_wrlock lock(rwlock);
  }

  rwlock.destroy();
}


/* Call when holding |mutex|. */
static void synchronize_nowait(void) {
  step += 1;
  uv_cond_signal(&condvar);
}


/* Call when holding |mutex|. */
static void synchronize(void) {
  int current;

  synchronize_nowait();
  /* Wait for the other thread.  Guard against spurious wakeups. */
  for (current = step; current == step; uv_cond_wait(&condvar, mutex.base())) {
  }
  ASSERT_EQ(step, current + 1);
}


static void thread_rwlock_trylock_peer(ns_thread*) {
  ns_mutex::scoped_lock lock(mutex);

  /* Write lock held by other thread. */
  ASSERT_EQ(UV_EBUSY, rwlock.tryrdlock());
  ASSERT_EQ(UV_EBUSY, rwlock.trywrlock());
  synchronize();

  /* Read lock held by other thread. */
  ASSERT_EQ(0, rwlock.tryrdlock());
  rwlock.rdunlock();
  ASSERT_EQ(UV_EBUSY, rwlock.trywrlock());
  synchronize();

  /* Acquire write lock. */
  ASSERT_EQ(0, rwlock.trywrlock());
  synchronize();

  /* Release write lock and acquire read lock. */
  rwlock.wrunlock();
  ASSERT_EQ(0, rwlock.tryrdlock());
  synchronize();

  rwlock.rdunlock();
  synchronize_nowait();  /* Signal main thread we're going away. */
}


TEST_CASE("thread_rwlock_trylock", "[mutex]") {
  ns_thread thread;

  ASSERT_EQ(0, uv_cond_init(&condvar));
  ASSERT_EQ(0, mutex.init());
  ASSERT_EQ(0, rwlock.init());

  mutex.lock();
  ASSERT_EQ(0, thread.create(thread_rwlock_trylock_peer));

  /* Hold write lock. */
  ASSERT_EQ(0, rwlock.trywrlock());
  synchronize();  /* Releases the mutex to the other thread. */

  /* Release write lock and acquire read lock.  Pthreads doesn't support
   * the notion of upgrading or downgrading rwlocks, so neither do we.
   */
  rwlock.wrunlock();
  ASSERT_EQ(0, rwlock.tryrdlock());
  synchronize();

  /* Release read lock. */
  rwlock.rdunlock();
  synchronize();

  /* Write lock held by other thread. */
  ASSERT_EQ(UV_EBUSY, rwlock.tryrdlock());
  ASSERT_EQ(UV_EBUSY, rwlock.trywrlock());
  synchronize();

  /* Read lock held by other thread. */
  ASSERT_EQ(0, rwlock.tryrdlock());
  rwlock.rdunlock();
  ASSERT_EQ(UV_EBUSY, rwlock.trywrlock());
  synchronize();

  ASSERT_EQ(0, thread.join());

  rwlock.destroy();
  mutex.unlock();
  mutex.destroy();
  uv_cond_destroy(&condvar);
}
