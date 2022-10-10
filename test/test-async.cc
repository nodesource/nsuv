#include "../include/nsuv-inl.h"
#include "./catch.hpp"
#include "./helpers.h"

#include <atomic>

using nsuv::ns_thread;
using nsuv::ns_async;
using nsuv::ns_mutex;
using nsuv::ns_prepare;

struct resources {
  ns_thread* thread;
  ns_async* async;
  ns_mutex* mutex;
  ns_prepare* prepare;
};

static std::atomic<int> async_cb_called;
static int prepare_cb_called;
static int close_cb_called;


static void thread_cb(ns_thread*, resources* res) {
  for (;;) {
    res->mutex->lock();
    res->mutex->unlock();
    if (async_cb_called == 3) {
      break;
    }
    CHECK(0 == res->async->send());
    uv_sleep(0);
  }
}


template <class H_T>
static void close_cb(H_T* handle) {
  CHECK(handle != nullptr);
  close_cb_called++;
}


static void async_cb(ns_async* handle, resources* res) {
  CHECK(handle == res->async);
  res->mutex->lock();
  res->mutex->unlock();
  if (++async_cb_called == 3) {
    res->async->close(close_cb);
    res->prepare->close(close_cb);
  }
}


static void prepare_cb(ns_prepare* handle, resources* res) {
  if (prepare_cb_called++)
    return;
  CHECK(handle == res->prepare);
  CHECK(0 == res->thread->create(thread_cb, res));
  res->mutex->unlock();
}


TEST_CASE("async_operations", "[async]") {
  ns_thread thread;
  ns_mutex mutex;
  ns_prepare prepare;
  ns_async async;
  resources res = { &thread, &async, &mutex, &prepare };

  ASSERT_EQ(0, prepare.init(uv_default_loop()));
  ASSERT_EQ(0, prepare.start(prepare_cb, &res));
  ASSERT_EQ(0, async.init(uv_default_loop(), async_cb, &res));
  ASSERT_EQ(0, mutex.init());

  mutex.lock();

  ASSERT_EQ(0, uv_run(uv_default_loop(), UV_RUN_DEFAULT));
  ASSERT_LT(0, prepare_cb_called);
  ASSERT_EQ(3, async_cb_called);
  ASSERT_EQ(2, close_cb_called);
  ASSERT_EQ(0, thread.join());

  make_valgrind_happy();
}
