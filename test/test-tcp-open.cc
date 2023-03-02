#include "../include/nsuv-inl.h"
#include "./helpers.h"

#ifndef _WIN32
# include <unistd.h>
#endif

using nsuv::ns_connect;
using nsuv::ns_tcp;
using nsuv::ns_timer;
using nsuv::ns_write;

static int shutdown_cb_called = 0;
static int shutdown_requested = 0;
static int connect_cb_called = 0;
static int write_cb_called = 0;
static int close_cb_called = 0;

static ns_connect<ns_tcp> connect_req;
static uv_shutdown_t shutdown_req;
static ns_write<ns_tcp> write_req;
static ns_timer tm;
static ns_tcp client;

static char ping_cstr[] = "PING";
static char p_cstr[] = "P";


static void zero_global_values() {
  shutdown_cb_called = 0;
  shutdown_requested = 0;
  connect_cb_called = 0;
  write_cb_called = 0;
  close_cb_called = 0;
}


static void startup(void) {
#ifdef _WIN32
    struct WSAData wsa_data;
    int r = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    ASSERT(r == 0);
#endif
}


static uv_os_sock_t create_tcp_socket(void) {
  uv_os_sock_t sock;

  sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
#ifdef _WIN32
  ASSERT(sock != INVALID_SOCKET);
#else
  ASSERT(sock >= 0);
#endif

#ifndef _WIN32
  {
    /* Allow reuse of the port. */
    int yes = 1;
    int r = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
    ASSERT(r == 0);
  }
#endif

  return sock;
}


static void close_socket(uv_os_sock_t sock) {
  int r;
#ifdef _WIN32
  r = closesocket(sock);
#else
  r = close(sock);
#endif
  ASSERT(r == 0);
}


static void alloc_cb(ns_tcp*, size_t suggested_size, uv_buf_t* buf) {
  static char slab[65536];
  ASSERT(suggested_size <= sizeof(slab));
  buf->base = slab;
  buf->len = sizeof(slab);
}


static void close_cb(ns_tcp* handle) {
  ASSERT_NOT_NULL(handle);
  close_cb_called++;
}


static void shutdown_cb(uv_shutdown_t* req, int status) {
  ASSERT(req == &shutdown_req);
  ASSERT(status == 0);

  /* Now we wait for the EOF */
  shutdown_cb_called++;
}


static void read_cb(ns_tcp* tcp, ssize_t nread, const uv_buf_t* buf) {
  ASSERT_NOT_NULL(tcp);

  if (nread >= 0) {
    ASSERT(nread == 4);
    ASSERT(memcmp("PING", buf->base, nread) == 0);
  } else {
    ASSERT(nread == UV_EOF);
    tcp->close(close_cb);
  }
}


static void read1_cb(ns_tcp* tcp, ssize_t nread, const uv_buf_t* buf) {
  int i;
  ASSERT_NOT_NULL(tcp);

  if (nread >= 0) {
    for (i = 0; i < nread; ++i)
      ASSERT(buf->base[i] == 'P');
  } else {
    ASSERT(nread == UV_EOF);
    printf("GOT EOF\n");
    tcp->close(close_cb);
  }
}


static void write_cb(ns_write<ns_tcp>* req, int status) {
  ASSERT_NOT_NULL(req);

  if (status) {
    fprintf(stderr, "uv_write error: %s\n", uv_strerror(status));
    ASSERT(0);
  }

  write_cb_called++;
}


static void write1_cb(ns_write<ns_tcp>* req, int status) {
  uv_buf_t buf;
  int r;

  ASSERT_NOT_NULL(req);
  if (status) {
    ASSERT(shutdown_cb_called);
    return;
  }

  if (shutdown_requested)
    return;

  buf = uv_buf_init(p_cstr, 1);
  r = req->handle()->write(&write_req, &buf, 1, write1_cb);
  ASSERT(r == 0);

  write_cb_called++;
}


static void timer_cb(ns_timer*) {
  int r;

  /* Shutdown on drain. */
  r = uv_shutdown(&shutdown_req, client.base_stream(), shutdown_cb);
  ASSERT(r == 0);
  shutdown_requested++;
}


static void connect_cb(ns_connect<ns_tcp>* req, int status) {
  uv_buf_t buf = uv_buf_init(ping_cstr, 4);
  ns_tcp* stream;
  int r;

  ASSERT(req == &connect_req);
  ASSERT(status == 0);

  stream = req->handle();
  connect_cb_called++;

  r = stream->write(&write_req, &buf, 1, write_cb);
  ASSERT(r == 0);

  /* Shutdown on drain. */
  r = uv_shutdown(&shutdown_req, stream->base_stream(), shutdown_cb);
  ASSERT(r == 0);

  /* Start reading */
  r = stream->read_start(alloc_cb, read_cb);
  ASSERT(r == 0);
}


static void connect1_cb(ns_connect<ns_tcp>* req, int status) {
  uv_buf_t buf;
  ns_tcp* stream;
  int r;

  ASSERT(req == &connect_req);
  ASSERT(status == 0);

  stream = req->handle();
  connect_cb_called++;

  r = tm.init(uv_default_loop());
  ASSERT(r == 0);

  r = tm.start(timer_cb, 2000, 0);
  ASSERT(r == 0);

  buf = uv_buf_init(p_cstr, 1);
  r = stream->write(&write_req, &buf, 1, write1_cb);
  ASSERT(r == 0);

  /* Start reading */
  r = stream->read_start(alloc_cb, read1_cb);
  ASSERT(r == 0);
}


TEST_CASE("tcp_open", "[tcp]") {
  struct sockaddr_in addr;
  uv_os_sock_t sock;
  int r;
  ns_tcp client2;

  // TODO(trevnorris): requires tcp4_echo_server
  RETURN_OK();

  zero_global_values();

  ASSERT(0 == uv_ip4_addr("127.0.0.1", kTestPort, &addr));

  startup();
  sock = create_tcp_socket();

  r = client.init(uv_default_loop());
  ASSERT(r == 0);

  r = client.open(sock);
  ASSERT(r == 0);

  r = client.connect(&connect_req, SOCKADDR_CONST_CAST(&addr), connect_cb);
  ASSERT(r == 0);

#ifndef _WIN32
  {
    r = client2.init(uv_default_loop());
    ASSERT(r == 0);

    r = client2.open(sock);
    ASSERT(r == UV_EEXIST);

    client2.close();
  }
#else  /* _WIN32 */
  (void)client2;
#endif

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  ASSERT(shutdown_cb_called == 1);
  ASSERT(connect_cb_called == 1);
  ASSERT(write_cb_called == 1);
  ASSERT(close_cb_called == 1);

  make_valgrind_happy();
}


TEST_CASE("tcp_open_twice", "[tcp]") {
  ns_tcp client;
  uv_os_sock_t sock1, sock2;
  int r;

  zero_global_values();

  startup();
  sock1 = create_tcp_socket();
  sock2 = create_tcp_socket();

  r = client.init(uv_default_loop());
  ASSERT(r == 0);

  r = client.open(sock1);
  ASSERT(r == 0);

  r = client.open(sock2);
  ASSERT(r == UV_EBUSY);
  close_socket(sock2);

  client.close();
  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  make_valgrind_happy();
}


TEST_CASE("tcp_open_bound", "[tcp]") {
  struct sockaddr_in addr;
  ns_tcp server;
  uv_os_sock_t sock;

  zero_global_values();

  startup();
  sock = create_tcp_socket();

  ASSERT(0 == uv_ip4_addr("127.0.0.1", kTestPort, &addr));

  ASSERT(0 == server.init(uv_default_loop()));

  ASSERT(0 == bind(sock, SOCKADDR_CAST(&addr), sizeof(addr)));

  ASSERT(0 == server.open(sock));

  ASSERT(0 == server.listen(128, nullptr));

  make_valgrind_happy();
}


TEST_CASE("tcp_open_connected", "[tcp]") {
  struct sockaddr_in addr;
  ns_tcp client;
  uv_os_sock_t sock;
  uv_buf_t buf = uv_buf_init(ping_cstr, 4);

  // TODO(trevnorris): requires tcp4_echo_server
  RETURN_OK();

  zero_global_values();

  ASSERT(0 == uv_ip4_addr("127.0.0.1", kTestPort, &addr));

  startup();
  sock = create_tcp_socket();

  ASSERT(0 == connect(sock, SOCKADDR_CAST(&addr),  sizeof(addr)));

  ASSERT(0 == client.init(uv_default_loop()));

  ASSERT(0 == client.open(sock));

  ASSERT(0 == client.write(&write_req, &buf, 1, write_cb));

  ASSERT(0 == uv_shutdown(&shutdown_req, client.base_stream(), shutdown_cb));

  ASSERT(0 == client.read_start(alloc_cb, read_cb));

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  ASSERT(shutdown_cb_called == 1);
  ASSERT(write_cb_called == 1);
  ASSERT(close_cb_called == 1);

  make_valgrind_happy();
}


TEST_CASE("tcp_write_ready", "[tcp]") {
  struct sockaddr_in addr;
  uv_os_sock_t sock;
  int r;

  // TODO(trevnorris): requires tcp4_echo_server
  RETURN_OK();

  zero_global_values();

  ASSERT(0 == uv_ip4_addr("127.0.0.1", kTestPort, &addr));

  startup();
  sock = create_tcp_socket();

  r = client.init(uv_default_loop());
  ASSERT(r == 0);

  r = client.open(sock);
  ASSERT(r == 0);

  r = client.connect(&connect_req, SOCKADDR_CONST_CAST(&addr), connect1_cb);
  ASSERT(r == 0);

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  ASSERT(shutdown_cb_called == 1);
  ASSERT(shutdown_requested == 1);
  ASSERT(connect_cb_called == 1);
  ASSERT(write_cb_called > 0);
  ASSERT(close_cb_called == 1);

  make_valgrind_happy();
}
