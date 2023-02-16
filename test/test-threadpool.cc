#include "../include/nsuv-inl.h"
#include "./catch.hpp"
#include "./helpers.h"

using nsuv::ns_work;

static int work_cb_count;
static int after_work_cb_count;
static ns_work work_req;
static char data;


static void work_cb(ns_work* req, char* d) {
  ASSERT(req == &work_req);
  ASSERT(d == &data);
  work_cb_count++;
}

static void work_cb(ns_work* req) {
  ASSERT(req == &work_req);
  work_cb_count++;
}

static void after_work_cb(ns_work* req, int status, char* d) {
  ASSERT(status == 0);
  ASSERT(req == &work_req);
  ASSERT(d == &data);
  after_work_cb_count++;
}

static void after_work_cb(ns_work* req, int status) {
  ASSERT(status == 0);
  ASSERT(req == &work_req);
  after_work_cb_count++;
}

static void after_work_cb2(ns_work*, int) {
  FAIL("should not have been called");
}


TEST_CASE("threadpool_queue_work_simple", "[threadpool]") {
  int r;

  work_cb_count = 0;
  after_work_cb_count = 0;

  r = work_req.queue_work(uv_default_loop(), work_cb, after_work_cb, &data);
  ASSERT(r == 0);
  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  ASSERT(work_cb_count == 1);
  ASSERT(after_work_cb_count == 1);

  make_valgrind_happy();
}


TEST_CASE("threadpool_queue_work_simple2", "[threadpool]") {
  int r;

  work_cb_count = 0;
  after_work_cb_count = 0;

  r = work_req.queue_work(uv_default_loop(), work_cb, after_work_cb);
  ASSERT(r == 0);
  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  ASSERT(work_cb_count == 1);
  ASSERT(after_work_cb_count == 1);

  make_valgrind_happy();
}


TEST_CASE("threadpool_queue_work_no_after", "[threadpool]") {
  int r;

  work_cb_count = 0;
  after_work_cb_count = 0;

  r = work_req.queue_work(uv_default_loop(), work_cb, &data);
  ASSERT(r == 0);
  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  ASSERT(work_cb_count == 1);
  ASSERT(after_work_cb_count == 0);

  make_valgrind_happy();
}


TEST_CASE("threadpool_queue_work_no_after2", "[threadpool]") {
  int r;

  work_cb_count = 0;
  after_work_cb_count = 0;

  r = work_req.queue_work(uv_default_loop(), work_cb);
  ASSERT(r == 0);
  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  ASSERT(work_cb_count == 1);
  ASSERT(after_work_cb_count == 0);

  make_valgrind_happy();
}


TEST_CASE("threadpool_queue_work_einval", "[threadpool]") {
  int r;

  work_cb_count = 0;
  after_work_cb_count = 0;

  r = work_req.queue_work(uv_default_loop(), nullptr, after_work_cb2);
  ASSERT(r == UV_EINVAL);

  r = work_req.queue_work(uv_default_loop(), nullptr);
  ASSERT(r == UV_EINVAL);

  // I hope no one would do this, but we need to cover it anyways.
  r = work_req.queue_work(uv_default_loop(),
                          static_cast<void(*)(ns_work*, char*)>(nullptr),
                          &data);
  ASSERT(r == UV_EINVAL);

  r = work_req.queue_work(uv_default_loop(),
                          static_cast<void(*)(ns_work*, char*)>(nullptr),
                          static_cast<void(*)(ns_work*, int, char*)>(nullptr),
                          &data);
  ASSERT(r == UV_EINVAL);

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  ASSERT(work_cb_count == 0);
  ASSERT(after_work_cb_count == 0);

  make_valgrind_happy();
}
