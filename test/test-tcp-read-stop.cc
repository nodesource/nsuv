#include "../include/nsuv-inl.h"
#include "./helpers.h"

using nsuv::ns_connect;
using nsuv::ns_tcp;
using nsuv::ns_timer;
using nsuv::ns_write;

static ns_timer timer_handle;
static ns_tcp tcp_handle;
static ns_write<ns_tcp> write_req;


static void fail_cb(void) {
  FAIL("fail_cb called");
}


static void write_cb(ns_write<ns_tcp>*, int) {
  timer_handle.close();
  tcp_handle.close();
}


static void timer_cb(ns_timer*) {
  char ping_cstr[] = "PING";
  uv_buf_t buf = uv_buf_init(ping_cstr, 4);
  ASSERT(0 == tcp_handle.write(&write_req, &buf, 1, write_cb));
  ASSERT(0 == uv_read_stop(tcp_handle.base_stream()));
}


static void connect_cb(ns_connect<ns_tcp>*, int status) {
  ASSERT(0 == status);
  ASSERT(0 == timer_handle.start(timer_cb, 50, 0));
  ASSERT(0 == uv_read_start(tcp_handle.base_stream(),
                            reinterpret_cast<uv_alloc_cb>(fail_cb),
                            reinterpret_cast<uv_read_cb>(fail_cb)));
}


TEST_CASE("tcp_read_stop", "[tcp]") {
  ns_connect<ns_tcp> connect_req;
  struct sockaddr_in addr;

  // TODO(trevnorris): requires tcp4_echo_server
  RETURN_OK();

  ASSERT(0 == uv_ip4_addr("127.0.0.1", kTestPort, &addr));
  ASSERT(0 == timer_handle.init(uv_default_loop()));
  ASSERT(0 == tcp_handle.init(uv_default_loop()));
  ASSERT(0 == tcp_handle.connect(&connect_req,
                                 SOCKADDR_CONST_CAST(&addr),
                                 connect_cb));
  ASSERT(0 == uv_run(uv_default_loop(), UV_RUN_DEFAULT));
  make_valgrind_happy();
}
