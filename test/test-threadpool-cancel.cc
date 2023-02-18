#include "../include/nsuv-inl.h"
#include "./catch.hpp"
#include "./helpers.h"

using nsuv::ns_addrinfo;
using nsuv::ns_random;
using nsuv::ns_timer;
using nsuv::ns_work;

#define INIT_CANCEL_INFO(ci, what)                                            \
  do {                                                                        \
    (ci)->reqs = (what);                                                      \
    (ci)->nreqs = ARRAY_SIZE(what);                                           \
    (ci)->stride = sizeof((what)[0]);                                         \
  }                                                                           \
  while (0)

struct cancel_info {
  void* reqs;
  unsigned nreqs;
  unsigned stride;
  ns_timer timer_handle;
};

struct random_info {
  ns_random random_req;
  char buf[1];
};

static unsigned fs_cb_called;
static unsigned done_cb_called;
static unsigned done2_cb_called;
static unsigned timer_cb_called;
static ns_work pause_reqs[4];
static uv_sem_t pause_sems[ARRAY_SIZE(pause_reqs)];


static void work_cb(ns_work* req) {
  uv_sem_wait(pause_sems + (req - pause_reqs));
}


static void done_cb(ns_work* req, int) {
  uv_sem_destroy(pause_sems + (req - pause_reqs));
}


static void saturate_threadpool(void) {
  uv_loop_t* loop;
  char buf[64];
  size_t i;

  snprintf(buf,
           sizeof(buf),
           "UV_THREADPOOL_SIZE=%lu",
           static_cast<uint64_t>(ARRAY_SIZE(pause_reqs)));
  putenv(buf);

  loop = uv_default_loop();
  for (i = 0; i < ARRAY_SIZE(pause_reqs); i += 1) {
    ASSERT(0 == uv_sem_init(pause_sems + i, 0));
    ASSERT(0 == pause_reqs[i].queue_work(loop, work_cb, done_cb));
  }
}


static void unblock_threadpool() {
  size_t i;

  for (i = 0; i < ARRAY_SIZE(pause_reqs); i += 1)
    uv_sem_post(pause_sems + i);
}


static void fs_cb(uv_fs_t* req) {
  ASSERT(req->result == UV_ECANCELED);
  uv_fs_req_cleanup(req);
  fs_cb_called++;
}


static void getaddrinfo_cb(ns_addrinfo*,
                           int status,
                           struct addrinfo* res) {
  ASSERT(status == UV_EAI_CANCELED);
  ASSERT_NULL(res);
}


static void getnameinfo_cb(uv_getnameinfo_t*,
                           int status,
                           const char* hostname,
                           const char* service) {
  ASSERT(status == UV_EAI_CANCELED);
  ASSERT_NULL(hostname);
  ASSERT_NULL(service);
}


static void work2_cb(ns_work*) {
  FAIL("work2_cb called");
}


static void done2_cb(ns_work*, int status) {
  ASSERT(status == UV_ECANCELED);
  done2_cb_called++;
}


static void timer_cb(ns_timer*, struct cancel_info* ci) {
  uv_req_t* req;
  unsigned i;

  for (i = 0; i < ci->nreqs; i++) {
    req = reinterpret_cast<uv_req_t*>(
      reinterpret_cast<char*>(ci->reqs) + i * ci->stride);
    ASSERT(0 == uv_cancel(req));
  }

  ci->timer_handle.close();
  unblock_threadpool();
  timer_cb_called++;
}


static void nop_done_cb(ns_work*, int status) {
  ASSERT(status == UV_ECANCELED);
  done_cb_called++;
}


static void nop_random_cb(ns_random*,
                          int status,
                          void* buf,
                          size_t len,
                          random_info* ri) {
  ASSERT(status == UV_ECANCELED);
  ASSERT(buf == ri->buf);
  ASSERT(len == sizeof(ri->buf));

  done_cb_called++;
}


TEST_CASE("threadpool_cancel_getaddrinfo", "[threadpool]") {
  ns_addrinfo reqs[4];
  struct cancel_info ci;
  struct addrinfo hints;
  uv_loop_t* loop;
  int r;

  timer_cb_called = 0;

  INIT_CANCEL_INFO(&ci, reqs);
  loop = uv_default_loop();
  saturate_threadpool();

  r = reqs[0].get(loop, getaddrinfo_cb, "fail", nullptr, nullptr);
  ASSERT(r == 0);

  r = reqs[1].get(loop, getaddrinfo_cb, nullptr, "fail", nullptr);
  ASSERT(r == 0);

  r = reqs[2].get(loop, getaddrinfo_cb, "fail", "fail", nullptr);
  ASSERT(r == 0);

  r = reqs[3].get(loop, getaddrinfo_cb, "fail", nullptr, &hints);
  ASSERT(r == 0);

  ASSERT(0 == ci.timer_handle.init(loop));
  ASSERT(0 == ci.timer_handle.start(timer_cb, 10, 0, &ci));
  ASSERT(0 == uv_run(loop, UV_RUN_DEFAULT));
  ASSERT(1 == timer_cb_called);

  make_valgrind_happy();
}


TEST_CASE("threadpool_cancel_getnameinfo", "[threadpool]") {
  uv_getnameinfo_t reqs[4];
  struct sockaddr_in addr4;
  struct cancel_info ci;
  uv_loop_t* loop;
  int r;

  timer_cb_called = 0;

  r = uv_ip4_addr("127.0.0.1", 80, &addr4);
  ASSERT(r == 0);

  INIT_CANCEL_INFO(&ci, reqs);
  loop = uv_default_loop();
  saturate_threadpool();

  r = uv_getnameinfo(
      loop, reqs + 0, getnameinfo_cb, SOCKADDR_CONST_CAST(&addr4), 0);
  ASSERT(r == 0);

  r = uv_getnameinfo(
      loop, reqs + 1, getnameinfo_cb, SOCKADDR_CONST_CAST(&addr4), 0);
  ASSERT(r == 0);

  r = uv_getnameinfo(
      loop, reqs + 2, getnameinfo_cb, SOCKADDR_CONST_CAST(&addr4), 0);
  ASSERT(r == 0);

  r = uv_getnameinfo(
      loop, reqs + 3, getnameinfo_cb, SOCKADDR_CONST_CAST(&addr4), 0);
  ASSERT(r == 0);

  ASSERT(0 == ci.timer_handle.init(loop));
  ASSERT(0 == ci.timer_handle.start(timer_cb, 10, 0, &ci));
  ASSERT(0 == uv_run(loop, UV_RUN_DEFAULT));
  ASSERT(1 == timer_cb_called);

  make_valgrind_happy();
}


TEST_CASE("threadpool_cancel_random", "[threadpool]") {
  struct random_info req;
  uv_loop_t* loop;

  done_cb_called = 0;

  saturate_threadpool();
  loop = uv_default_loop();
  ASSERT(0 == req.random_req.get(loop,
                                 &req.buf,
                                 sizeof(req.buf),
                                 0,
                                 nop_random_cb,
                                 &req));
  ASSERT(0 == req.random_req.cancel());
  ASSERT(0 == done_cb_called);
  unblock_threadpool();
  ASSERT(0 == uv_run(loop, UV_RUN_DEFAULT));
  ASSERT(1 == done_cb_called);

  make_valgrind_happy();
}


TEST_CASE("threadpool_cancel_work", "[threadpool]") {
  struct cancel_info ci;
  ns_work reqs[16];
  uv_loop_t* loop;
  unsigned i;

  timer_cb_called = 0;
  done2_cb_called = 0;

  INIT_CANCEL_INFO(&ci, reqs);
  loop = uv_default_loop();
  saturate_threadpool();

  for (i = 0; i < ARRAY_SIZE(reqs); i++)
    ASSERT(0 == reqs[i].queue_work(loop, work2_cb, done2_cb));

  ASSERT(0 == ci.timer_handle.init(loop));
  ASSERT(0 == ci.timer_handle.start(timer_cb, 10, 0, &ci));
  ASSERT(0 == uv_run(loop, UV_RUN_DEFAULT));
  ASSERT(1 == timer_cb_called);
  ASSERT(ARRAY_SIZE(reqs) == done2_cb_called);

  make_valgrind_happy();
}


TEST_CASE("threadpool_cancel_fs", "[threadpool]") {
  struct cancel_info ci;
  uv_fs_t reqs[26];
  uv_loop_t* loop;
  unsigned n;
  uv_buf_t iov;

  fs_cb_called = 0;
  timer_cb_called = 0;

  INIT_CANCEL_INFO(&ci, reqs);
  loop = uv_default_loop();
  saturate_threadpool();
  iov = uv_buf_init(nullptr, 0);

  /* Needs to match ARRAY_SIZE(fs_reqs). */
  n = 0;
  ASSERT(0 == uv_fs_chmod(loop, reqs + n++, "/", 0, fs_cb));
  ASSERT(0 == uv_fs_chown(loop, reqs + n++, "/", 0, 0, fs_cb));
  ASSERT(0 == uv_fs_close(loop, reqs + n++, 0, fs_cb));
  ASSERT(0 == uv_fs_fchmod(loop, reqs + n++, 0, 0, fs_cb));
  ASSERT(0 == uv_fs_fchown(loop, reqs + n++, 0, 0, 0, fs_cb));
  ASSERT(0 == uv_fs_fdatasync(loop, reqs + n++, 0, fs_cb));
  ASSERT(0 == uv_fs_fstat(loop, reqs + n++, 0, fs_cb));
  ASSERT(0 == uv_fs_fsync(loop, reqs + n++, 0, fs_cb));
  ASSERT(0 == uv_fs_ftruncate(loop, reqs + n++, 0, 0, fs_cb));
  ASSERT(0 == uv_fs_futime(loop, reqs + n++, 0, 0, 0, fs_cb));
  ASSERT(0 == uv_fs_link(loop, reqs + n++, "/", "/", fs_cb));
  ASSERT(0 == uv_fs_lstat(loop, reqs + n++, "/", fs_cb));
  ASSERT(0 == uv_fs_mkdir(loop, reqs + n++, "/", 0, fs_cb));
  ASSERT(0 == uv_fs_open(loop, reqs + n++, "/", 0, 0, fs_cb));
  ASSERT(0 == uv_fs_read(loop, reqs + n++, 0, &iov, 1, 0, fs_cb));
  ASSERT(0 == uv_fs_scandir(loop, reqs + n++, "/", 0, fs_cb));
  ASSERT(0 == uv_fs_readlink(loop, reqs + n++, "/", fs_cb));
  ASSERT(0 == uv_fs_realpath(loop, reqs + n++, "/", fs_cb));
  ASSERT(0 == uv_fs_rename(loop, reqs + n++, "/", "/", fs_cb));
  ASSERT(0 == uv_fs_mkdir(loop, reqs + n++, "/", 0, fs_cb));
  ASSERT(0 == uv_fs_sendfile(loop, reqs + n++, 0, 0, 0, 0, fs_cb));
  ASSERT(0 == uv_fs_stat(loop, reqs + n++, "/", fs_cb));
  ASSERT(0 == uv_fs_symlink(loop, reqs + n++, "/", "/", 0, fs_cb));
  ASSERT(0 == uv_fs_unlink(loop, reqs + n++, "/", fs_cb));
  ASSERT(0 == uv_fs_utime(loop, reqs + n++, "/", 0, 0, fs_cb));
  ASSERT(0 == uv_fs_write(loop, reqs + n++, 0, &iov, 1, 0, fs_cb));
  ASSERT(n == ARRAY_SIZE(reqs));

  ASSERT(0 == ci.timer_handle.init(loop));
  ASSERT(0 == ci.timer_handle.start(timer_cb, 10, 0, &ci));
  ASSERT(0 == uv_run(loop, UV_RUN_DEFAULT));
  ASSERT(n == fs_cb_called);
  ASSERT(1 == timer_cb_called);


  make_valgrind_happy();
}


TEST_CASE("threadpool_cancel_single", "[threadpool]") {
  uv_loop_t* loop;
  ns_work req;

  done_cb_called = 0;

  saturate_threadpool();
  loop = uv_default_loop();
  ASSERT(0 == req.queue_work(loop, [](ns_work*) { abort(); }, nop_done_cb));
  ASSERT(0 == req.cancel());
  ASSERT(0 == done_cb_called);
  unblock_threadpool();
  ASSERT(0 == uv_run(loop, UV_RUN_DEFAULT));
  ASSERT(1 == done_cb_called);

  make_valgrind_happy();
}
