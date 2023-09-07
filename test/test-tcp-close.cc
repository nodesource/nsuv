#include "../include/nsuv-inl.h"
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

  ASSERT_EQ(0, status);
  for (size_t i = 0; i < num_write_reqs; i++) {
    char* ping = new char[4]();  // "PING"
    uv_buf_t buf = uv_buf_init(ping, 4);
    req = new ns_write<ns_tcp>();
    ASSERT_NOT_NULL(req);
    ASSERT_EQ(0, tcp_handle.write(req, &buf, 1, write_cb));
  }

  tcp_handle.close(close_cb);
}


static void write_cb(ns_write<ns_tcp>* req, int status) {
  const uv_buf_t* bufs;
  size_t nbufs;
  /* write callbacks should run before the close callback */
  ASSERT_EQ(0, status);
  ASSERT_EQ(0, close_cb_called);
  ASSERT_EQ(req->handle(), &tcp_handle);
  write_cb_called++;
  bufs = req->bufs();
  nbufs = req->size();
  for (size_t i = 0; i < nbufs; i++) {
    delete[] bufs[i].base;
  }
  delete req;
}


static void close_cb(ns_tcp* handle) {
  ASSERT_EQ(handle, &tcp_handle);
  close_cb_called++;
}


static void connection_cb(ns_tcp*, int status) {
  ASSERT_EQ(0, status);
}


static void start_server(uv_loop_t* loop, ns_tcp* handle) {
  struct sockaddr_in addr;
  ASSERT_EQ(0, uv_ip4_addr("127.0.0.1", kTestPort, &addr));
  ASSERT_EQ(0, handle->init(loop));
  ASSERT_EQ(0,
      handle->bind(reinterpret_cast<const struct sockaddr*>(&addr), 0));
  ASSERT_EQ(0, handle->listen(128, connection_cb));
  handle->unref();
}


/* Check that pending write requests have their callbacks
 * invoked when the handle is closed.
 */
TEST_CASE("tcp_close", "[tcp]") {
  struct sockaddr_in addr;
  ns_tcp tcp_server;

  ASSERT_EQ(0, uv_ip4_addr("127.0.0.1", kTestPort, &addr));

  /* We can't use the echo server, it doesn't handle ECONNRESET. */
  start_server(uv_default_loop(), &tcp_server);

  ASSERT_EQ(0, tcp_handle.init(uv_default_loop()));
  ASSERT_EQ(0, tcp_handle.connect(
      &connect_req,
      reinterpret_cast<const struct sockaddr*>(&addr),
      connect_cb));
  ASSERT_EQ(0, write_cb_called);
  ASSERT_EQ(0, close_cb_called);

  ASSERT_EQ(0, uv_run(uv_default_loop(), UV_RUN_DEFAULT));

  INFO("" << write_cb_called << " of " << num_write_reqs << "write reqs seen");

  ASSERT_EQ(write_cb_called, num_write_reqs);
  ASSERT_EQ(1, close_cb_called);

  make_valgrind_happy();
}
