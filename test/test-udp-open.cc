#include "../include/nsuv-inl.h"
#include "./helpers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
# include <unistd.h>
# include <sys/socket.h>
# include <sys/un.h>
#endif

using nsuv::ns_udp;
using nsuv::ns_udp_send;

static int send_cb_called = 0;
static int close_cb_called = 0;

static ns_udp_send send_req;

static char ping_str[] = "PING";


static void startup(void) {
#ifdef _WIN32
    struct WSAData wsa_data;
    int r = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    ASSERT(r == 0);
#endif
}


static uv_os_sock_t create_udp_socket(void) {
  uv_os_sock_t sock;

  sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
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


static void alloc_cb(uv_handle_t*,
                     size_t suggested_size,
                     uv_buf_t* buf) {
  static char slab[65536];
  ASSERT(suggested_size <= sizeof(slab));
  buf->base = slab;
  buf->len = sizeof(slab);
}


static void close_cb(ns_udp* handle) {
  ASSERT_NOT_NULL(handle);
  close_cb_called++;
}


static void recv_cb(uv_udp_t* handle,
                    ssize_t nread,
                    const uv_buf_t* buf,
                    const struct sockaddr* addr,
                    unsigned flags) {
  int r;

  if (nread < 0) {
    FAIL("unexpected error");
  }

  if (nread == 0) {
    /* Returning unused buffer. Don't count towards sv_recv_cb_called */
    ASSERT_NULL(addr);
    return;
  }

  ASSERT(flags == 0);

  ASSERT_NOT_NULL(addr);
  ASSERT(nread == 4);
  ASSERT(memcmp(ping_str, buf->base, nread) == 0);

  r = uv_udp_recv_stop(handle);
  ASSERT(r == 0);

  ns_udp::cast(handle)->close(close_cb);
}


static void send_cb(ns_udp_send* req, int status) {
  ASSERT_NOT_NULL(req);
  ASSERT(status == 0);

  send_cb_called++;
  req->handle()->close(close_cb);
}


TEST_CASE("udp_open", "[udp]") {
  struct sockaddr_in addr;
  uv_buf_t buf = uv_buf_init(ping_str, 4);
  ns_udp client, client2;
  uv_os_sock_t sock;
  int r;

  ASSERT(0 == uv_ip4_addr("127.0.0.1", kTestPort, &addr));

  startup();
  sock = create_udp_socket();

  r = client.init(uv_default_loop());
  ASSERT(r == 0);

  r = uv_udp_open(&client, sock);
  ASSERT(r == 0);

  r = client.bind(SOCKADDR_CONST_CAST(&addr), 0);
  ASSERT(r == 0);

  r = uv_udp_recv_start(&client, alloc_cb, recv_cb);
  ASSERT(r == 0);

  r = client.send(&send_req, &buf, 1, SOCKADDR_CONST_CAST(&addr), send_cb);
  ASSERT(r == 0);

#ifndef _WIN32
  {
    r = client2.init(uv_default_loop());
    ASSERT(r == 0);

    r = uv_udp_open(&client2, sock);
    ASSERT(r == UV_EEXIST);

    client2.close();
  }
#else  /* _WIN32 */
  reinterpret_cast<void>(client2);
#endif

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  ASSERT(send_cb_called == 1);
  ASSERT(close_cb_called == 1);

  ASSERT(client.uv_handle()->send_queue_size == 0);

  make_valgrind_happy();
}


TEST_CASE("udp_open_twice", "[udp]") {
  ns_udp client;
  uv_os_sock_t sock1, sock2;
  int r;

  startup();
  sock1 = create_udp_socket();
  sock2 = create_udp_socket();

  r = client.init(uv_default_loop());
  ASSERT(r == 0);

  r = uv_udp_open(&client, sock1);
  ASSERT(r == 0);

  r = uv_udp_open(&client, sock2);
  ASSERT(r == UV_EBUSY);
  close_socket(sock2);

  client.close();
  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  make_valgrind_happy();
}

TEST_CASE("udp_open_bound", "[udp]") {
  struct sockaddr_in addr;
  ns_udp client;
  uv_os_sock_t sock;
  int r;

  ASSERT(0 == uv_ip4_addr("127.0.0.1", kTestPort, &addr));

  startup();
  sock = create_udp_socket();

  r = bind(sock, SOCKADDR_CAST(&addr), sizeof(addr));
  ASSERT(r == 0);

  r = client.init(uv_default_loop());
  ASSERT(r == 0);

  r = uv_udp_open(&client, sock);
  ASSERT(r == 0);

  r = uv_udp_recv_start(&client, alloc_cb, recv_cb);
  ASSERT(r == 0);

  client.close();
  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  make_valgrind_happy();
}

TEST_CASE("udp_open_connect", "[udp]") {
  struct sockaddr_in addr;
  uv_buf_t buf = uv_buf_init(ping_str, 4);
  ns_udp client;
  ns_udp server;
  uv_os_sock_t sock;
  int r;

  send_cb_called = 0;
  close_cb_called = 0;

  ASSERT(0 == uv_ip4_addr("127.0.0.1", kTestPort, &addr));

  startup();
  sock = create_udp_socket();

  r = client.init(uv_default_loop());
  ASSERT(r == 0);

  r = connect(sock, (const struct sockaddr*) &addr, sizeof(addr));
  ASSERT(r == 0);

  r = uv_udp_open(&client, sock);
  ASSERT(r == 0);

  r = server.init(uv_default_loop());
  ASSERT(r == 0);

  r = server.bind(SOCKADDR_CONST_CAST(&addr), 0);
  ASSERT(r == 0);

  r = uv_udp_recv_start(&server, alloc_cb, recv_cb);
  ASSERT(r == 0);

  r = client.send(&send_req, &buf, 1, nullptr, send_cb);
  ASSERT(r == 0);

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  CHECK(send_cb_called == 1);
  CHECK(close_cb_called == 2);

  CHECK(client.uv_handle()->send_queue_size == 0);

  make_valgrind_happy();
}

#ifndef _WIN32
TEST_CASE("udp_send_unix", "[udp]") {
  /* Test that "uv_udp_send()" supports sending over
     a "sockaddr_un" address. */
  struct sockaddr_un addr;
  ns_udp handle;
  ns_udp_send req;
  uv_loop_t* loop;
  uv_buf_t buf = uv_buf_init(ping_str, 4);
  int fd;
  int r;

  loop = uv_default_loop();

  memset(&addr, 0, sizeof addr);
  addr.sun_family = AF_UNIX;
  ASSERT(strlen(kTestPipename) < sizeof(addr.sun_path));
  memcpy(addr.sun_path, kTestPipename, strlen(kTestPipename));

  fd = socket(AF_UNIX, SOCK_STREAM, 0);
  ASSERT(fd >= 0);

  unlink(kTestPipename);
  ASSERT(0 == bind(fd, (const struct sockaddr*)&addr, sizeof addr));
  ASSERT(0 == listen(fd, 1));

  r = handle.init(loop);
  ASSERT(r == 0);
  r = uv_udp_open(&handle, fd);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);

  r = handle.send(&req, &buf, 1, SOCKADDR_CONST_CAST(&addr));
  ASSERT(r == 0);

  handle.close();
  uv_run(loop, UV_RUN_DEFAULT);
  close(fd);
  unlink(kTestPipename);

  make_valgrind_happy();
}
#endif
