#include "../include/nsuv-inl.h"
#include "./catch.hpp"
#include "./helpers.h"

using nsuv::ns_work;

static int work_cb_count;
static int after_work_cb_count;
static ns_work work_req;


static void work_cb(ns_work* req, std::weak_ptr<size_t> d) {
  auto sp = d.lock();
  ASSERT(sp);
  ASSERT_EQ(42, *sp);
  ASSERT(req == &work_req);
  work_cb_count++;
}

static void after_work_cb(ns_work* req, int status, std::weak_ptr<size_t> d) {
  auto sp = d.lock();
  ASSERT(sp);
  ASSERT_EQ(42, *sp);
  ASSERT(status == 0);
  ASSERT(req == &work_req);
  after_work_cb_count++;
}


TEST_CASE("threadpool_queue_work_simple_wp", "[threadpool]") {
  std::shared_ptr<size_t> sp = std::make_shared<size_t>(42);
  int r;

  work_cb_count = 0;
  after_work_cb_count = 0;

  r = work_req.queue_work(
      uv_default_loop(), work_cb, after_work_cb, TO_WEAK(sp));
  ASSERT(r == 0);
  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  ASSERT(work_cb_count == 1);
  ASSERT(after_work_cb_count == 1);

  make_valgrind_happy();
}


TEST_CASE("threadpool_queue_work_no_after_wp", "[threadpool]") {
  std::shared_ptr<size_t> sp = std::make_shared<size_t>(42);
  int r;

  work_cb_count = 0;
  after_work_cb_count = 0;

  r = work_req.queue_work(uv_default_loop(), work_cb, TO_WEAK(sp));
  ASSERT(r == 0);
  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  ASSERT(work_cb_count == 1);
  ASSERT(after_work_cb_count == 0);

  make_valgrind_happy();
}
