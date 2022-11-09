#include "../include/nsuv-inl.h"
#include "./helpers.h"

using nsuv::ns_connect;
using nsuv::ns_tcp;
using nsuv::ns_write;

static uv_loop_t* loop;
static ns_tcp tcp_server;
static ns_tcp tcp_client;
static ns_tcp tcp_accepted;
static ns_connect<ns_tcp> connect_req;
static uv_shutdown_t shutdown_req;
static ns_write<ns_tcp> write_reqs[4];

static int client_close;
static int shutdown_before_close;

static int write_cb_called;
static int close_cb_called;
static int shutdown_cb_called;

static void connect_cb(ns_connect<ns_tcp>* req, int status);
static void write_cb(ns_write<ns_tcp>* req, int status);
static void close_cb(ns_tcp* handle);
static void shutdown_cb(uv_shutdown_t* req, int status);

static int read_size;

static char ping_cstr[] = "PING";

static void zero_global_values() {
  client_close = 0;
  shutdown_before_close = 0;
  write_cb_called = 0;
  close_cb_called = 0;
  shutdown_cb_called = 0;
  read_size = 0;
}


static void do_write(ns_tcp* handle) {
  uv_buf_t buf;
  unsigned i;
  int r;

  buf = uv_buf_init(ping_cstr, 4);
  for (i = 0; i < ARRAY_SIZE(write_reqs); i++) {
    r = handle->write(&write_reqs[i], &buf, 1, write_cb);
    ASSERT(r == 0);
  }
}


static void do_close(ns_tcp* handle) {
  if (shutdown_before_close == 1) {
    ASSERT(0 == uv_shutdown(&shutdown_req, handle->base_stream(), shutdown_cb));
    ASSERT(UV_EINVAL == handle->close_reset(close_cb));
  } else {
    ASSERT(0 == handle->close_reset(close_cb));
    ASSERT(UV_ENOTCONN ==
           uv_shutdown(&shutdown_req, handle->base_stream(), shutdown_cb));
  }

  tcp_server.close();
}

static void alloc_cb(uv_handle_t*, size_t, uv_buf_t* buf) {
  static char slab[1024];
  buf->base = slab;
  buf->len = sizeof(slab);
}

static void read_cb2(uv_stream_t* handle, ssize_t nread, const uv_buf_t*) {
  ns_tcp* stream = ns_tcp::cast(handle);
  ASSERT(stream == &tcp_client);
  if (nread == UV_EOF)
    stream->close();
}


static void connect_cb(ns_connect<ns_tcp>* conn_req, int) {
  ASSERT(conn_req == &connect_req);
  uv_read_start(tcp_client.base_stream(), alloc_cb, read_cb2);
  do_write(&tcp_client);
  if (client_close)
    do_close(&tcp_client);
}


static void write_cb(ns_write<ns_tcp>* req, int) {
  /* write callbacks should run before the close callback */
  ASSERT(close_cb_called == 0);
  ASSERT(req->handle() == &tcp_client);
  write_cb_called++;
}


static void close_cb(ns_tcp* handle) {
  if (client_close)
    ASSERT(handle == &tcp_client);
  else
    ASSERT(handle == &tcp_accepted);

  close_cb_called++;
}


static void shutdown_cb(uv_shutdown_t* req, int) {
  if (client_close)
    ASSERT(req->handle == tcp_client.base_stream());
  else
    ASSERT(req->handle == tcp_accepted.base_stream());

  shutdown_cb_called++;
}


static void read_cb(uv_stream_t* handle, ssize_t nread, const uv_buf_t*) {
  ns_tcp* stream = ns_tcp::cast(handle);
  ASSERT(stream == &tcp_accepted);
  if (nread < 0) {
    stream->close();
  } else {
    read_size += nread;
    if (read_size == 16 && client_close == 0)
      do_close(&tcp_accepted);
  }
}


static void connection_cb(ns_tcp* server, int status) {
  ASSERT(status == 0);

  ASSERT(0 == tcp_accepted.init(loop));
  ASSERT(0 == uv_accept(server->base_stream(), tcp_accepted.base_stream()));

  uv_read_start(tcp_accepted.base_stream(), alloc_cb, read_cb);
}


static void start_server(uv_loop_t* loop, ns_tcp* handle) {
  struct sockaddr_in addr;
  int r;

  ASSERT(0 == uv_ip4_addr("127.0.0.1", kTestPort, &addr));

  r = handle->init(loop);
  ASSERT(r == 0);

  r = handle->bind(SOCKADDR_CONST_CAST(&addr), 0);
  ASSERT(r == 0);

  r = handle->listen(128, connection_cb);
  ASSERT(r == 0);
}


static void do_connect(uv_loop_t* loop, ns_tcp* tcp_client) {
  struct sockaddr_in addr;
  int r;

  ASSERT(0 == uv_ip4_addr("127.0.0.1", kTestPort, &addr));

  r = tcp_client->init(loop);
  ASSERT(r == 0);

  r = tcp_client->connect(&connect_req, SOCKADDR_CONST_CAST(&addr), connect_cb);
  ASSERT(r == 0);
}


/* Check that pending write requests have their callbacks
 * invoked when the handle is closed.
 */
TEST_CASE("tcp_close_reset_client", "[tcp]") {
  int r;

  zero_global_values();

  loop = uv_default_loop();

  start_server(loop, &tcp_server);

  client_close = 1;
  shutdown_before_close = 0;

  do_connect(loop, &tcp_client);

  ASSERT(write_cb_called == 0);
  ASSERT(close_cb_called == 0);
  ASSERT(shutdown_cb_called == 0);

  r = uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(r == 0);

  ASSERT(write_cb_called == 4);
  ASSERT(close_cb_called == 1);
  ASSERT(shutdown_cb_called == 0);

  make_valgrind_happy();
}

TEST_CASE("tcp_close_reset_client_after_shutdown", "[tcp]") {
  int r;

  zero_global_values();

  loop = uv_default_loop();

  start_server(loop, &tcp_server);

  client_close = 1;
  shutdown_before_close = 1;

  do_connect(loop, &tcp_client);

  ASSERT(write_cb_called == 0);
  ASSERT(close_cb_called == 0);
  ASSERT(shutdown_cb_called == 0);

  r = uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(r == 0);

  ASSERT(write_cb_called == 4);
  ASSERT(close_cb_called == 0);
  ASSERT(shutdown_cb_called == 1);

  make_valgrind_happy();
}

TEST_CASE("tcp_close_reset_accepted", "[tcp]") {
  int r;

  zero_global_values();

  loop = uv_default_loop();

  start_server(loop, &tcp_server);

  client_close = 0;
  shutdown_before_close = 0;

  do_connect(loop, &tcp_client);

  ASSERT(write_cb_called == 0);
  ASSERT(close_cb_called == 0);
  ASSERT(shutdown_cb_called == 0);

  r = uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(r == 0);

  ASSERT(write_cb_called == 4);
  ASSERT(close_cb_called == 1);
  ASSERT(shutdown_cb_called == 0);

  make_valgrind_happy();
}

TEST_CASE("tcp_close_reset_accepted_after_shutdown", "[tcp]") {
  int r;

  zero_global_values();

  loop = uv_default_loop();

  start_server(loop, &tcp_server);

  client_close = 0;
  shutdown_before_close = 1;

  do_connect(loop, &tcp_client);

  ASSERT(write_cb_called == 0);
  ASSERT(close_cb_called == 0);
  ASSERT(shutdown_cb_called == 0);

  r = uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(r == 0);

  ASSERT(write_cb_called == 4);
  ASSERT(close_cb_called == 0);
  ASSERT(shutdown_cb_called == 1);

  make_valgrind_happy();
}
