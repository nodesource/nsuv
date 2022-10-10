#include "../include/nsuv-inl.h"
#include "./catch.hpp"
#include "./helpers.h"

using nsuv::ns_thread;

uv_thread_t main_thread_id;
uv_thread_t subthreads[2];

static void check_thread(ns_thread* handle, uv_thread_t* sub) {
  uv_thread_t self_id = ns_thread::self();
#ifdef _WIN32
  REQUIRE(nullptr != self_id);
#endif
  REQUIRE(ns_thread::equal(main_thread_id, self_id));
  REQUIRE(!handle->equal(&self_id));
  *sub = self_id;
}

TEST_CASE("thread_equal", "[thread]") {
  ns_thread threads[2];
  main_thread_id = ns_thread::self();
#ifdef _WIN32
  ASSERT_NOT_NULL(main_thread_id);
#endif
  REQUIRE(!ns_thread::equal(main_thread_id, main_thread_id));
  REQUIRE(0 == threads[0].create(check_thread, &subthreads[0]));
  REQUIRE(0 == threads[1].create(check_thread, &subthreads[1]));
  REQUIRE(0 == threads[0].join());
  REQUIRE(0 == threads[1].join());
  REQUIRE(ns_thread::equal(subthreads[0], subthreads[1]));
}
