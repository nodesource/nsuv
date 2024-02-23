#include "../include/nsuv-inl.h"
#include "./helpers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h> /* memset */

#ifdef __POSIX__
#include <pthread.h>
#endif

using nsuv::ns_thread;

static int thread_called;
static uv_key_t tls_key;


static void thread_entry(ns_thread* thread, std::weak_ptr<size_t> d) {
  auto sp = d.lock();
  ASSERT(sp);
  CHECK(!thread->equal(uv_thread_self()));
  CHECK(*sp == 42);
  thread_called++;
}


TEST_CASE("thread_create_wp", "[thread]") {
  std::shared_ptr<size_t> sp = std::make_shared<size_t>(42);
  ns_thread thread;
  ASSERT_EQ(0, thread.create(thread_entry, TO_WEAK(sp)));
  ASSERT_EQ(0, thread.join());
  ASSERT_EQ(1, thread_called);
  ASSERT(thread.equal(uv_thread_self()));
}


static void tls_thread(ns_thread* arg, std::weak_ptr<size_t> d) {
  auto sp = d.lock();
  ASSERT(sp);
  ASSERT_EQ(42, *sp);
  CHECK(nullptr == uv_key_get(&tls_key));
  uv_key_set(&tls_key, arg);
  CHECK(arg == uv_key_get(&tls_key));
  uv_key_set(&tls_key, nullptr);
  CHECK(nullptr == uv_key_get(&tls_key));
}


TEST_CASE("thread_local_storage_wp", "[thread]") {
  std::shared_ptr<size_t> sp = std::make_shared<size_t>(42);
  char name[] = "main";
  ns_thread threads[2];
  ASSERT_EQ(0, uv_key_create(&tls_key));
  ASSERT_NULL(uv_key_get(&tls_key));
  uv_key_set(&tls_key, name);
  ASSERT_EQ(name, uv_key_get(&tls_key));
  ASSERT_EQ(0, threads[0].create(tls_thread, TO_WEAK(sp)));
  ASSERT_EQ(0, threads[1].create(tls_thread, TO_WEAK(sp)));
  ASSERT_EQ(0, threads[0].join());
  ASSERT_EQ(0, threads[1].join());
  uv_key_delete(&tls_key);
}


static void thread_check_stack(ns_thread*,
                               std::weak_ptr<uv_thread_options_t> d) {
  auto arg = d.lock();
  ASSERT(arg);
#if defined(__APPLE__)
  size_t expected;
  expected = arg == nullptr ? 0 : arg->stack_size;
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
  expected = arg == nullptr ? 0 : arg->stack_size;
  if (expected == 0)
    expected = (size_t)lim.rlim_cur;
  CHECK(stack_size >= expected);
  CHECK(0 == pthread_attr_destroy(&attr));
#endif
}


TEST_CASE("thread_stack_size_explicit_wp", "[thread]") {
  std::shared_ptr<uv_thread_options_t> options =
    std::make_shared<uv_thread_options_t>();
  std::weak_ptr<uv_thread_options_t> wp = options;
  ns_thread thread;

  options->flags = UV_THREAD_HAS_STACK_SIZE;
  options->stack_size = 1024 * 1024;
  ASSERT_EQ(0, thread.create_ex(options.get(), thread_check_stack, wp));
  ASSERT_EQ(0, thread.join());

  options->stack_size = 8 * 1024 * 1024;  // larger than most default os sizes
  ASSERT_EQ(0, thread.create_ex(options.get(), thread_check_stack, wp));
  ASSERT_EQ(0, thread.join());

  options->stack_size = 0;
  ASSERT_EQ(0, thread.create_ex(options.get(), thread_check_stack, wp));
  ASSERT_EQ(0, thread.join());

#ifdef PTHREAD_STACK_MIN
  options->stack_size = PTHREAD_STACK_MIN - 42;  // unaligned size
  ASSERT_EQ(0, thread.create_ex(options.get(), thread_check_stack, wp));
  ASSERT_EQ(0, thread.join());

  options->stack_size = PTHREAD_STACK_MIN / 2 - 42;  // unaligned size
  ASSERT_EQ(0, thread.create_ex(options.get(), thread_check_stack, wp));
  ASSERT_EQ(0, thread.join());
#endif

  // unaligned size, should be larger than PTHREAD_STACK_MIN
  options->stack_size = 1234567;
  ASSERT_EQ(0, thread.create_ex(options.get(), thread_check_stack, wp));
  ASSERT_EQ(0, thread.join());
}
