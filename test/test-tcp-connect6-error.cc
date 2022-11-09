#include "../include/nsuv-inl.h"
#include "./helpers.h"

using nsuv::ns_connect;
using nsuv::ns_tcp;

static int connect_cb_called = 0;
static int close_cb_called = 0;


static void connect_cb(ns_connect<ns_tcp>* handle, int) {
  ASSERT_NOT_NULL(handle);
  connect_cb_called++;
}


static void close_cb(ns_tcp* handle) {
  ASSERT_NOT_NULL(handle);
  close_cb_called++;
}


TEST_CASE("tcp_connect6_error_fault", "[tcp]") {
  const char garbage[] =
      "blah blah blah blah blah blah blah blah blah blah blah blah";
  const struct sockaddr_in6* garbage_addr;
  ns_tcp server;
  int r;
  ns_connect<ns_tcp> req;

  garbage_addr = reinterpret_cast<const struct sockaddr_in6*>(&garbage);

  r = server.init(uv_default_loop());
  ASSERT(r == 0);
  r = server.connect(&req, SOCKADDR_CONST_CAST(&garbage_addr), connect_cb);
  ASSERT(r == UV_EINVAL);

  server.close(close_cb);

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  ASSERT(connect_cb_called == 0);
  ASSERT(close_cb_called == 1);

  make_valgrind_happy();
}
