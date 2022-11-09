#include "../include/nsuv-inl.h"
#include "./helpers.h"

using nsuv::ns_connect;
using nsuv::ns_tcp;
using nsuv::ns_write;

static int connect_cb_called;
static int write_cb_called;
static int close_cb_called;


static void close_cb(ns_tcp*) {
  close_cb_called++;
}


static void connect_cb(ns_connect<ns_tcp>* req, int status) {
  ASSERT(status < 0);
  connect_cb_called++;
  req->handle()->close(close_cb);
}


static void write_cb(ns_write<ns_tcp>*, int status) {
  ASSERT(status < 0);
  write_cb_called++;
}


/*
 * Try to connect to an address on which nothing listens, get ECONNREFUSED
 * (uv errno 12) and get connect_cb() called once with status != 0.
 * Related issue: https://github.com/joyent/libuv/issues/443
 */
TEST_CASE("tcp_connect_error_after_write", "[tcp]") {
  char help_cstr[] = "TEST";
  ns_connect<ns_tcp> connect_req;
  struct sockaddr_in addr;
  ns_write<ns_tcp> write_req;
  ns_tcp conn;
  uv_buf_t buf;
  int r;

#ifdef _WIN32
  fprintf(stderr, "This test is disabled on Windows for now.\n");
  fprintf(stderr, "See https://github.com/joyent/libuv/issues/444\n");
  return 0; /* windows slackers... */
#endif

  ASSERT(0 == uv_ip4_addr("127.0.0.1", kTestPort, &addr));
  buf = uv_buf_init(help_cstr, 4);

  r = conn.init(uv_default_loop());
  ASSERT(r == 0);

  r = conn.write(&write_req, &buf, 1, write_cb);
  ASSERT(r == UV_EBADF);

  r = conn.connect(&connect_req, SOCKADDR_CONST_CAST(&addr), connect_cb);
  ASSERT(r == 0);

  r = conn.write(&write_req, &buf, 1, write_cb);
  ASSERT(r == 0);

  r = uv_run(uv_default_loop(), UV_RUN_DEFAULT);
  ASSERT(r == 0);

  ASSERT(connect_cb_called == 1);
  ASSERT(write_cb_called == 1);
  ASSERT(close_cb_called == 1);

  make_valgrind_happy();
}
