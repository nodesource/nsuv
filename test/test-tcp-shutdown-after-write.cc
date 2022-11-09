#include "../include/nsuv-inl.h"
#include "./helpers.h"

using nsuv::ns_tcp;
using nsuv::ns_timer;

static void write_cb(uv_write_t* req, int status);
static void shutdown_cb(uv_shutdown_t* req, int status);

static ns_tcp conn;
static ns_timer timer;
static uv_connect_t connect_req;
static uv_write_t write_req;
static uv_shutdown_t shutdown_req;

static int connect_cb_called;
static int write_cb_called;
static int shutdown_cb_called;

static int conn_close_cb_called;
static int timer_close_cb_called;


static void close_cb(uv_handle_t* handle) {
  if (handle == conn.base_handle())
    conn_close_cb_called++;
  else if (handle == timer.base_handle())
    timer_close_cb_called++;
  else
    FAIL("bad handle in close_cb");
}


static void alloc_cb(uv_handle_t*, size_t, uv_buf_t* buf) {
  static char slab[64];
  buf->base = slab;
  buf->len = sizeof(slab);
}


static void timer_cb(uv_timer_t* handle) {
  char test_cstr[] = "TEST";
  uv_buf_t buf;
  int r;

  uv_close(ns_timer::cast(handle)->base_handle(), close_cb);

  buf = uv_buf_init(test_cstr, 4);
  r = uv_write(&write_req, conn.base_stream(), &buf, 1, write_cb);
  ASSERT(r == 0);

  r = uv_shutdown(&shutdown_req, conn.base_stream(), shutdown_cb);
  ASSERT(r == 0);
}


static void read_cb(uv_stream_t*, ssize_t, const uv_buf_t*) {
}


static void connect_cb(uv_connect_t*, int status) {
  int r;

  ASSERT(status == 0);
  connect_cb_called++;

  r = uv_read_start(conn.base_stream(), alloc_cb, read_cb);
  ASSERT(r == 0);
}


static void write_cb(uv_write_t*, int status) {
  ASSERT(status == 0);
  write_cb_called++;
}


static void shutdown_cb(uv_shutdown_t*, int status) {
  ASSERT(status == 0);
  shutdown_cb_called++;
  uv_close(conn.base_handle(), close_cb);
}


TEST_CASE("tcp_shutdown_after_write", "[tcp]") {
  struct sockaddr_in addr;
  uv_loop_t* loop;
  int r;

  // TODO(trevnorris): requires tcp4_echo_server
  RETURN_OK();

  ASSERT(0 == uv_ip4_addr("127.0.0.1", kTestPort, &addr));
  loop = uv_default_loop();

  r = uv_timer_init(loop, &timer);
  ASSERT(r == 0);

  r = uv_timer_start(&timer, timer_cb, 125, 0);
  ASSERT(r == 0);

  r = uv_tcp_init(loop, &conn);
  ASSERT(r == 0);

  r = uv_tcp_connect(&connect_req,
                     &conn,
                     SOCKADDR_CONST_CAST(&addr),
                     connect_cb);
  ASSERT(r == 0);

  r = uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(r == 0);

  ASSERT(connect_cb_called == 1);
  ASSERT(write_cb_called == 1);
  ASSERT(shutdown_cb_called == 1);
  ASSERT(conn_close_cb_called == 1);
  ASSERT(timer_close_cb_called == 1);

  make_valgrind_happy();
}
