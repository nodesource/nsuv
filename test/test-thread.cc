#include "../nsuv-inl.h"
#include "./catch.hpp"
#include "./helpers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h> /* memset */

#ifdef __POSIX__
#include <pthread.h>
#endif

using nsuv::ns_thread;

struct getaddrinfo_req {
  size_t counter;
  size_t count_up;
  uv_loop_t* loop;
  uv_getaddrinfo_t handle;
};


struct fs_req {
  size_t counter;
  size_t count_up;
  uv_loop_t* loop;
  uv_fs_t handle;
};


static void getaddrinfo_do(struct getaddrinfo_req* req);
static void getaddrinfo_cb(uv_getaddrinfo_t* handle,
                           int status,
                           struct addrinfo* res);
static void fs_do(struct fs_req* req);
static void fs_cb(uv_fs_t* handle);

static int thread_called;
static uv_key_t tls_key;


static void getaddrinfo_do(struct getaddrinfo_req* req) {
  CHECK(0 == uv_getaddrinfo(req->loop,
                            &req->handle,
                            getaddrinfo_cb,
                            "localhost",
                            nullptr,
                            nullptr));
}


static void getaddrinfo_cb(uv_getaddrinfo_t* handle,
                           int status,
                           struct addrinfo* res) {
  struct getaddrinfo_req* req;
  CHECK(status == 0);
  req = container_of(handle, struct getaddrinfo_req, handle);
  uv_freeaddrinfo(res);
  req->count_up++;
  if (--req->counter)
    getaddrinfo_do(req);
}


static void fs_cb(uv_fs_t* handle) {
  struct fs_req* req = container_of(handle, struct fs_req, handle);
  uv_fs_req_cleanup(handle);
  req->count_up++;
  if (--req->counter)
    fs_do(req);
}


static void fs_do(struct fs_req* req) {
  CHECK(0 == uv_fs_stat(req->loop, &req->handle, ".", fs_cb));
}


static void do_work(ns_thread*, int* did_run) {
  constexpr size_t reqs_size = 4;
  struct getaddrinfo_req getaddrinfo_reqs[reqs_size];
  struct fs_req fs_reqs[reqs_size];
  uv_loop_t loop;

  CHECK(0 == uv_loop_init(&loop));

  for (size_t i = 0; i < reqs_size; i++) {
    struct getaddrinfo_req* req = getaddrinfo_reqs + i;
    req->counter = reqs_size;
    req->loop = &loop;
    req->count_up = 0;
    getaddrinfo_do(req);
  }

  for (size_t i = 0; i < reqs_size; i++) {
    struct fs_req* req = fs_reqs + i;
    req->counter = reqs_size;
    req->loop = &loop;
    req->count_up = 0;
    fs_do(req);
  }

  CHECK(0 == uv_run(&loop, UV_RUN_DEFAULT));
  CHECK(0 == uv_loop_close(&loop));

  for (size_t i = 0; i < reqs_size; i++) {
    CHECK(0 == getaddrinfo_reqs[i].counter);
    CHECK(reqs_size == getaddrinfo_reqs[i].count_up);
    CHECK(0 == fs_reqs[i].counter);
    CHECK(reqs_size == fs_reqs[i].count_up);
  }

  *did_run += 1;
}


TEST_CASE("threadpool_multiple_event_loops", "[thread]") {
  constexpr size_t kThreadCount = 8;
  ns_thread threads[kThreadCount];
  int did_run[kThreadCount] = { 0 };

  for (size_t i = 0; i < kThreadCount; i++) {
    REQUIRE(0 == threads[i].create(do_work, &did_run[i]));
  }

  for (size_t i = 0; i < kThreadCount; i++) {
    REQUIRE(0 == threads[i].join());
    REQUIRE(1 == did_run[i]);
  }
}


static void thread_entry(ns_thread*, size_t* arg) {
  CHECK(*arg == 42);
  thread_called++;
}


TEST_CASE("thread_create", "[thread]") {
  ns_thread thread;
  size_t arg[] = { 42 };
  REQUIRE(0 == thread.create(thread_entry, arg));
  REQUIRE(0 == thread.join());
  REQUIRE(thread_called == 1);
}


static void tls_thread(ns_thread* arg) {
  CHECK(nullptr == uv_key_get(&tls_key));
  uv_key_set(&tls_key, arg);
  CHECK(arg == uv_key_get(&tls_key));
  uv_key_set(&tls_key, nullptr);
  CHECK(nullptr == uv_key_get(&tls_key));
}


TEST_CASE("thread_local_storage", "[thread]") {
  char name[] = "main";
  ns_thread threads[2];
  REQUIRE(0 == uv_key_create(&tls_key));
  REQUIRE(nullptr == uv_key_get(&tls_key));
  uv_key_set(&tls_key, name);
  REQUIRE(name == uv_key_get(&tls_key));
  REQUIRE(0 == threads[0].create(tls_thread));
  REQUIRE(0 == threads[1].create(tls_thread));
  REQUIRE(0 == threads[0].join());
  REQUIRE(0 == threads[1].join());
  uv_key_delete(&tls_key);
}


static void thread_check_stack(ns_thread*, uv_thread_options_t* arg) {
#if defined(__APPLE__)
  size_t expected;
  expected = arg == nullptr ? 0 :
    (reinterpret_cast<uv_thread_options_t*>(arg))->stack_size;
  /* 512 kB is the default stack size of threads other than the main thread
   * on MacOS. */
  if (expected == 0)
    expected = 512 * 1024;
  CHECK(pthread_get_stacksize_np(pthread_self()) >= expected);
#elif defined(__linux__) && defined(__GLIBC__)
  size_t expected;
  struct rlimit lim;
  size_t stack_size;
  pthread_attr_t attr;
  CHECK(0 == getrlimit(RLIMIT_STACK, &lim));
  if (lim.rlim_cur == RLIM_INFINITY)
    lim.rlim_cur = 2 << 20;  /* glibc default. */
  CHECK(0 == pthread_getattr_np(pthread_self(), &attr));
  CHECK(0 == pthread_attr_getstacksize(&attr, &stack_size));
  expected = arg == nullptr ? 0 :
    (reinterpret_cast<uv_thread_options_t*>(arg))->stack_size;
  if (expected == 0)
    expected = (size_t)lim.rlim_cur;
  CHECK(stack_size >= expected);
  CHECK(0 == pthread_attr_destroy(&attr));
#endif
}


TEST_CASE("thread_stack_size", "[thread]") {
  ns_thread thread;
  uv_thread_options_t* arg = nullptr;
  REQUIRE(0 == thread.create(thread_check_stack, arg));
  REQUIRE(0 == thread.join());
}


TEST_CASE("thread_stack_size_explicit", "[thread]") {
  ns_thread thread;
  uv_thread_options_t options;

  options.flags = UV_THREAD_HAS_STACK_SIZE;
  options.stack_size = 1024 * 1024;
  REQUIRE(0 == thread.create_ex(&options, thread_check_stack, &options));
  REQUIRE(0 == thread.join());

  options.stack_size = 8 * 1024 * 1024;  // larger than most default os sizes
  REQUIRE(0 == thread.create_ex(&options, thread_check_stack, &options));
  REQUIRE(0 == thread.join());

  options.stack_size = 0;
  REQUIRE(0 == thread.create_ex(&options, thread_check_stack, &options));
  REQUIRE(0 == thread.join());

#ifdef PTHREAD_STACK_MIN
  options.stack_size = PTHREAD_STACK_MIN - 42;  // unaligned size
  REQUIRE(0 == thread.create_ex(&options, thread_check_stack, &options));
  REQUIRE(0 == thread.join());

  options.stack_size = PTHREAD_STACK_MIN / 2 - 42;  // unaligned size
  REQUIRE(0 == thread.create_ex(&options, thread_check_stack, &options));
  REQUIRE(0 == thread.join());
#endif

  // unaligned size, should be larger than PTHREAD_STACK_MIN
  options.stack_size = 1234567;
  REQUIRE(0 == thread.create_ex(&options, thread_check_stack, &options));
  REQUIRE(0 == thread.join());
}
