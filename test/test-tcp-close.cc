#include "../include/nsuv-inl.h"
#include "./catch.hpp"
#include "./helpers.h"

using nsuv::ns_connect;
using nsuv::ns_tcp;
using nsuv::ns_write;

constexpr size_t num_write_reqs = 32;

static ns_tcp tcp_handle;
static ns_connect<ns_tcp> connect_req;

static int write_cb_called;
static int close_cb_called;

static void connect_cb(ns_connect<ns_tcp>* req, int status);
static void write_cb(ns_write<ns_tcp>* req, int status);
static void close_cb(ns_tcp* handle);


static void connect_cb(ns_connect<ns_tcp>*, int status) {
  ns_write<ns_tcp>* req;

  REQUIRE(0 == status);
  for (size_t i = 0; i < num_write_reqs; i++) {
    char* ping = new char[4]();  // "PING"
    uv_buf_t buf = uv_buf_init(ping, 4);
    req = new ns_write<ns_tcp>();
    REQUIRE(nullptr != req);
    REQUIRE(0 == tcp_handle.write(req, &buf, 1, write_cb));
  }

  tcp_handle.close(close_cb);
}


static void write_cb(ns_write<ns_tcp>* req, int status) {
  /* write callbacks should run before the close callback */
  REQUIRE(0 == status);
  REQUIRE(0 == close_cb_called);
  REQUIRE(req->handle() == &tcp_handle);
  write_cb_called++;
  for (auto buf : req->bufs()) {
    delete[] buf.base;
  }
  delete req;
}


static void close_cb(ns_tcp* handle) {
  REQUIRE(handle == &tcp_handle);
  close_cb_called++;
}


static void connection_cb(ns_tcp*, int status) {
  REQUIRE(0 == status);
}


static void start_server(uv_loop_t* loop, ns_tcp* handle) {
  struct sockaddr_in addr;
  REQUIRE(0 == uv_ip4_addr("127.0.0.1", kTestPort, &addr));
  REQUIRE(0 == handle->init(loop));
  REQUIRE(0 ==
      handle->bind(reinterpret_cast<const struct sockaddr*>(&addr), 0));
  REQUIRE(0 == handle->listen(128, connection_cb));
  handle->unref();
}


/* Check that pending write requests have their callbacks
 * invoked when the handle is closed.
 */
TEST_CASE("tcp_close", "[tcp]") {
  struct sockaddr_in addr;
  ns_tcp tcp_server;

  REQUIRE(0 == uv_ip4_addr("127.0.0.1", kTestPort, &addr));

  /* We can't use the echo server, it doesn't handle ECONNRESET. */
  start_server(uv_default_loop(), &tcp_server);

  REQUIRE(0 == tcp_handle.init(uv_default_loop()));
  REQUIRE(0 == tcp_handle.connect(
      &connect_req,
      reinterpret_cast<const struct sockaddr*>(&addr),
      connect_cb));
  REQUIRE(0 == write_cb_called);
  REQUIRE(0 == close_cb_called);

  REQUIRE(0 == uv_run(uv_default_loop(), UV_RUN_DEFAULT));

  INFO("" << write_cb_called << " of " << num_write_reqs << "write reqs seen");

  REQUIRE(write_cb_called == num_write_reqs);
  REQUIRE(1 == close_cb_called);

  make_valgrind_happy();
}
