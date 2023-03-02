#include "../include/nsuv-inl.h"
#include "./helpers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using nsuv::ns_connect;
using nsuv::ns_tcp;
using nsuv::ns_write;

static ns_tcp server;
static ns_tcp client;
static ns_tcp incoming;
static int connect_cb_called;
static int close_cb_called;
static int connection_cb_called;
static ns_write<ns_tcp> write_req;

static char hello[] = "HELLO!";


static void close_cb(ns_tcp*) {
  close_cb_called++;
}

static void write_cb(ns_write<ns_tcp>*, int status) {
  ASSERT(status == 0);
}

static void conn_alloc_cb(uv_handle_t*, size_t, uv_buf_t*) {
  /* Do nothing, read_cb should be called with UV_ENOBUFS. */
}

static void conn_read_cb(uv_stream_t*,
                         ssize_t nread,
                         const uv_buf_t* buf) {
  ASSERT(nread == UV_ENOBUFS);
  ASSERT_NULL(buf->base);
  ASSERT(buf->len == 0);

  incoming.close(close_cb);
  client.close(close_cb);
  server.close(close_cb);
}

static void connect_cb(ns_connect<ns_tcp>* req, int status) {
  int r;
  uv_buf_t buf;

  ASSERT(status == 0);
  connect_cb_called++;

  buf = uv_buf_init(hello, sizeof(hello));
  r = req->handle()->write(&write_req, &buf, 1, write_cb);
  ASSERT(r == 0);
}


static void connection_cb(ns_tcp* tcp, int status) {
  ASSERT(status == 0);

  ASSERT(0 == uv_tcp_init(tcp->get_loop(), &incoming));
  ASSERT(0 == tcp->accept(&incoming));
  ASSERT(0 == uv_read_start(incoming.base_stream(),
                            conn_alloc_cb,
                            conn_read_cb));

  connection_cb_called++;
}


static void start_server(void) {
  struct sockaddr_in addr;

  ASSERT(0 == uv_ip4_addr("0.0.0.0", kTestPort, &addr));

  ASSERT(0 == server.init(uv_default_loop()));
  ASSERT(0 == server.bind(SOCKADDR_CAST(&addr), 0));
  ASSERT(0 == server.listen(128, connection_cb));
}


TEST_CASE("tcp_alloc_cb_fail", "[tcp]") {
  ns_connect<ns_tcp> connect_req;
  struct sockaddr_in addr;

  start_server();

  ASSERT(0 == uv_ip4_addr("127.0.0.1", kTestPort, &addr));

  ASSERT(0 == client.init(uv_default_loop()));
  ASSERT(0 == client.connect(&connect_req,
                             (struct sockaddr*) &addr,
                             connect_cb));

  ASSERT(0 == uv_run(uv_default_loop(), UV_RUN_DEFAULT));

  ASSERT(connect_cb_called == 1);
  ASSERT(connection_cb_called == 1);
  ASSERT(close_cb_called == 3);

  make_valgrind_happy();
}
