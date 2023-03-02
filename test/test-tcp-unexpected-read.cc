#include "../include/nsuv-inl.h"
#include "./helpers.h"

using nsuv::ns_check;
using nsuv::ns_connect;
using nsuv::ns_tcp;
using nsuv::ns_timer;
using nsuv::ns_write;

static ns_check check_handle;
static ns_timer timer_handle;
static ns_tcp server_handle;
static ns_tcp client_handle;
static ns_tcp peer_handle;
static ns_write<ns_tcp> write_req;
static ns_connect<ns_tcp> connect_req;

static uint32_t ticks; /* event loop ticks */


static void check_cb(ns_check*) {
  ticks++;
}


static void timer_cb(ns_timer*) {
  check_handle.close();
  timer_handle.close();
  server_handle.close();
  client_handle.close();
  peer_handle.close();
}


static void alloc_cb(uv_handle_t*, size_t, uv_buf_t*) {
  FAIL("alloc_cb should not have been called");
}


static void read_cb(uv_stream_t*, ssize_t, const uv_buf_t*) {
  FAIL("read_cb should not have been called");
}


static void connect_cb(ns_connect<ns_tcp>* req, int status) {
  ASSERT(req->handle() == &client_handle);
  ASSERT(0 == status);
}


static void write_cb(ns_write<ns_tcp>* req, int status) {
  ASSERT(req->handle() == &peer_handle);
  ASSERT(0 == status);
}


static void connection_cb(ns_tcp* handle, int status) {
  char ping_cstr[] = "PING";
  uv_buf_t buf;

  buf = uv_buf_init(ping_cstr, 4);

  ASSERT(0 == status);
  ASSERT(0 == handle->accept(&peer_handle));
  ASSERT(0 == uv_read_start(peer_handle.base_stream(), alloc_cb, read_cb));
  ASSERT(0 == peer_handle.write(&write_req, &buf, 1, write_cb));
}


TEST_CASE("tcp_unexpected_read", "[tcp]") {
  struct sockaddr_in addr;
  uv_loop_t* loop;

  ASSERT(0 == uv_ip4_addr("127.0.0.1", kTestPort, &addr));
  loop = uv_default_loop();

  ASSERT(0 == timer_handle.init(loop));
  ASSERT(0 == timer_handle.start(timer_cb, 1000, 0));
  ASSERT(0 == check_handle.init(loop));
  ASSERT(0 == check_handle.start(check_cb));
  ASSERT(0 == server_handle.init(loop));
  ASSERT(0 == client_handle.init(loop));
  ASSERT(0 == peer_handle.init(loop));
  ASSERT(0 == server_handle.bind(SOCKADDR_CONST_CAST(&addr), 0));
  ASSERT(0 == server_handle.listen(1, connection_cb));
  ASSERT(0 == client_handle.connect(&connect_req,
                                    SOCKADDR_CONST_CAST(&addr),
                                    connect_cb));
  ASSERT(0 == uv_run(loop, UV_RUN_DEFAULT));

  /* This is somewhat inexact but the idea is that the event loop should not
   * start busy looping when the server sends a message and the client isn't
   * reading.
   */
  ASSERT(ticks <= 20);

  make_valgrind_happy();
}
