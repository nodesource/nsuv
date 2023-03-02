#include "../include/nsuv-inl.h"
#include "./helpers.h"

using nsuv::ns_connect;
using nsuv::ns_tcp;
using nsuv::ns_write;

static void connection_cb(ns_tcp* server, int status);
static void connect_cb(ns_connect<ns_tcp>* req, int status);
static void write_cb(ns_write<ns_tcp>* req, int status);
static void read_cb(ns_tcp* stream, ssize_t nread, const uv_buf_t* buf);
static void alloc_cb(ns_tcp* handle, size_t suggested_size, uv_buf_t* buf);

static ns_tcp tcp_server;
static ns_tcp tcp_client;
static ns_tcp tcp_peer; /* client socket as accept()-ed by server */
static ns_connect<ns_tcp> connect_req;
static ns_write<ns_tcp> write_req;

static int write_cb_called;
static int read_cb_called;

static void connection_cb(ns_tcp* server, int status) {
  char hello_cstr[] = "hello\n";
  int r;
  uv_buf_t buf;

  ASSERT(server == &tcp_server);
  ASSERT(status == 0);

  r = tcp_peer.init(server->get_loop());
  ASSERT(r == 0);

  r = server->accept(&tcp_peer);
  ASSERT(r == 0);

  r = tcp_peer.read_start(alloc_cb, read_cb);
  ASSERT(r == 0);

  buf.base = hello_cstr;
  buf.len = 6;

  r = tcp_peer.write(&write_req, &buf, 1, write_cb);
  ASSERT(r == 0);
}


static void alloc_cb(ns_tcp*, size_t, uv_buf_t* buf) {
  static char slab[1024];
  buf->base = slab;
  buf->len = sizeof(slab);
}


static void read_cb(ns_tcp*, ssize_t nread, const uv_buf_t*) {
  if (nread < 0) {
    // fprintf(stderr, "read_cb error: %s\n", uv_err_name(nread));
    ASSERT((nread == UV_ECONNRESET || nread == UV_EOF));

    tcp_server.close();
    tcp_peer.close();
  }

  read_cb_called++;
}


static void connect_cb(ns_connect<ns_tcp>* req, int status) {
  ASSERT(req == &connect_req);
  ASSERT(status == 0);

  /* Close the client. */
  tcp_client.close();
}


static void write_cb(ns_write<ns_tcp>*, int status) {
  ASSERT(status == 0);
  write_cb_called++;
}


TEST_CASE("tcp_write_to_half_open_connection", "[tcp]") {
  struct sockaddr_in addr;
  uv_loop_t* loop;
  int r;

  ASSERT(0 == uv_ip4_addr("127.0.0.1", kTestPort, &addr));

  loop = uv_default_loop();
  ASSERT_NOT_NULL(loop);

  r = tcp_server.init(loop);
  ASSERT(r == 0);

  r = tcp_server.bind(SOCKADDR_CONST_CAST(&addr), 0);
  ASSERT(r == 0);

  r = tcp_server.listen(1, connection_cb);
  ASSERT(r == 0);

  r = tcp_client.init(loop);
  ASSERT(r == 0);

  r = tcp_client.connect(&connect_req, SOCKADDR_CONST_CAST(&addr), connect_cb);
  ASSERT(r == 0);

  r = uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(r == 0);

  ASSERT(write_cb_called > 0);
  ASSERT(read_cb_called > 0);

  make_valgrind_happy();
}
