#include "../include/nsuv-inl.h"
#include "./helpers.h"

using nsuv::ns_connect;
using nsuv::ns_tcp;

#define MAX_BYTES 1024 * 1024

static ns_tcp server;
static ns_tcp client;
static ns_tcp incoming;
static int connect_cb_called;
static int close_cb_called;
static int connection_cb_called;
static int bytes_read;
static int bytes_written;


static void close_cb(ns_tcp*) {
  close_cb_called++;
}


static void connect_cb(ns_connect<ns_tcp>*, int status) {
  char ping_cstr[] = "PING";
  char empty_cstr[] = "";
  int r;
  uv_buf_t buf;
  ASSERT(status == 0);
  connect_cb_called++;

  do {
    buf = uv_buf_init(ping_cstr, 4);
    r = uv_try_write(client.base_stream(), &buf, 1);
    ASSERT((r > 0 || r == UV_EAGAIN));
    if (r > 0) {
      bytes_written += r;
      break;
    }
  } while (1);

  do {
    buf = uv_buf_init(empty_cstr, 0);
    r = uv_try_write(client.base_stream(), &buf, 1);
  } while (r != 0);
  client.close(close_cb);
}


static void alloc_cb(ns_tcp*, size_t, uv_buf_t* buf) {
  static char base[1024];

  buf->base = base;
  buf->len = sizeof(base);
}


static void read_cb(ns_tcp* tcp, ssize_t nread, const uv_buf_t*) {
  if (nread < 0) {
    tcp->close(close_cb);
    server.close(close_cb);
    return;
  }

  bytes_read += nread;
}


static void connection_cb(ns_tcp* tcp, int status) {
  ASSERT(status == 0);

  ASSERT(0 == incoming.init(tcp->get_loop()));
  ASSERT(0 == tcp->accept(&incoming));

  connection_cb_called++;
  ASSERT(0 == incoming.read_start(alloc_cb, read_cb));
}


static void start_server(void) {
  struct sockaddr_in addr;

  ASSERT(0 == uv_ip4_addr("0.0.0.0", kTestPort, &addr));

  ASSERT(0 == server.init(uv_default_loop()));
  ASSERT(0 == server.bind(SOCKADDR_CONST_CAST(&addr), 0));
  ASSERT(0 == server.listen(128, connection_cb));
}


TEST_CASE("tcp_try_write", "[tcp]") {
  ns_connect<ns_tcp> connect_req;
  struct sockaddr_in addr;

  start_server();

  ASSERT(0 == uv_ip4_addr("127.0.0.1", kTestPort, &addr));

  ASSERT(0 == client.init(uv_default_loop()));
  ASSERT(0 == client.connect(&connect_req,
                             SOCKADDR_CONST_CAST(&addr),
                             connect_cb));

  ASSERT(0 == uv_run(uv_default_loop(), UV_RUN_DEFAULT));

  ASSERT(connect_cb_called == 1);
  ASSERT(close_cb_called == 3);
  ASSERT(connection_cb_called == 1);
  ASSERT(bytes_read == bytes_written);
  ASSERT(bytes_written > 0);

  make_valgrind_happy();
}
