#include "../include/nsuv-inl.h"
#include "./helpers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using nsuv::ns_udp;
using nsuv::ns_udp_send;

#define CHECK_HANDLE(handle) \
  ASSERT((reinterpret_cast<ns_udp*>(handle) == &server || \
        reinterpret_cast<ns_udp*>(handle) == &client))

static ns_udp server;
static ns_udp client;
static uv_buf_t buf;
static struct sockaddr_in6 lo_addr;

static int cl_send_cb_called;
static int sv_recv_cb_called;

static int close_cb_called;


static void alloc_cb(uv_handle_t* handle,
                     size_t suggested_size,
                     uv_buf_t* buf) {
  static char slab[65536];
  CHECK_HANDLE(handle);
  ASSERT_LE(suggested_size, sizeof(slab));
  buf->base = slab;
  buf->len = sizeof(slab);
}


static void close_cb(ns_udp* handle) {
  CHECK_HANDLE(handle);
  ASSERT(handle->is_closing());
  close_cb_called++;
}


static void cl_send_cb(ns_udp_send* req, int status) {
  int r;

  ASSERT_NOT_NULL(req);
  ASSERT_EQ(status, 0);
  CHECK_HANDLE(req->handle());
  if (++cl_send_cb_called == 1) {
    r = client.connect(nullptr);
    if (r) { /* do nothing */ }
    r = client.send(req, &buf, 1, nullptr, cl_send_cb);
    ASSERT_EQ(r, UV_EDESTADDRREQ);
    r = client.send(req, &buf, 1, SOCKADDR_CONST_CAST(&lo_addr), cl_send_cb);
    ASSERT_EQ(r, 0);
  }
}


static void sv_recv_cb(uv_udp_t*,
                       ssize_t nread,
                       const uv_buf_t* rcvbuf,
                       const struct sockaddr* addr,
                       unsigned) {
  if (nread > 0) {
    ASSERT_EQ(nread, 4);
    ASSERT_NOT_NULL(addr);
    ASSERT_EQ(memcmp("EXIT", rcvbuf->base, nread), 0);
    if (++sv_recv_cb_called == 4) {
      server.close(close_cb);
      client.close(close_cb);
    }
  }
}


TEST_CASE("udp_connect6", "[udp]") {
#if defined(__PASE__)
  RETURN_SKIP(
      "IBMi PASE's UDP connection can not be disconnected with AF_UNSPEC.");
#endif
  ns_udp_send req;
  struct sockaddr_in6 ext_addr;
  struct sockaddr_in6 tmp_addr;
  int r;
  int addrlen;
  char exit_str[] = "EXIT";

  if (!can_ipv6())
    RETURN_SKIP("IPv6 not supported");

  ASSERT_EQ(0, uv_ip6_addr("::", kTestPort, &lo_addr));

  r = server.init(uv_default_loop());
  ASSERT_EQ(r, 0);

  r = server.bind(SOCKADDR_CONST_CAST(&lo_addr), 0);
  ASSERT_EQ(r, 0);

  r = uv_udp_recv_start(&server, alloc_cb, sv_recv_cb);
  ASSERT_EQ(r, 0);

  r = client.init(uv_default_loop());
  ASSERT_EQ(r, 0);

  buf = uv_buf_init(exit_str, 4);

  /* connect() to INADDR_ANY fails on Windows wih WSAEADDRNOTAVAIL */
  ASSERT_EQ(0, uv_ip6_addr("::", kTestPort, &tmp_addr));
  r = client.connect(SOCKADDR_CONST_CAST(&tmp_addr));
#ifdef _WIN32
  ASSERT_EQ(r, UV_EADDRNOTAVAIL);
#else
  ASSERT_EQ(r, 0);
  r = client.connect(nullptr);
  ASSERT_EQ(r, 0);
#endif

  ASSERT_EQ(0, uv_ip6_addr("2001:4860:4860::8888", kTestPort, &ext_addr));
  ASSERT_EQ(0, uv_ip6_addr("::1", kTestPort, &lo_addr));

  r = client.connect(SOCKADDR_CONST_CAST(&lo_addr));
  ASSERT_EQ(r, 0);
  r = client.connect(SOCKADDR_CONST_CAST(&ext_addr));
  ASSERT_EQ(r, UV_EISCONN);

  addrlen = sizeof(tmp_addr);
  r = client.getpeername(SOCKADDR_CAST(&tmp_addr), &addrlen);
  ASSERT_EQ(r, 0);

  /* To send messages in connected UDP sockets addr must be nullptr */
  r = client.try_send(&buf, 1, SOCKADDR_CONST_CAST(&lo_addr));
  ASSERT_EQ(r, UV_EISCONN);
  r = client.try_send(&buf, 1, nullptr);
  ASSERT_EQ(r, 4);
  r = client.try_send(&buf, 1, SOCKADDR_CONST_CAST(&ext_addr));
  ASSERT_EQ(r, UV_EISCONN);

  r = client.connect(nullptr);
  ASSERT_EQ(r, 0);
  r = client.connect(nullptr);
  ASSERT_EQ(r, UV_ENOTCONN);

  addrlen = sizeof(tmp_addr);
  r = client.getpeername(SOCKADDR_CAST(&tmp_addr), &addrlen);
  ASSERT_EQ(r, UV_ENOTCONN);

  /* To send messages in disconnected UDP sockets addr must be set */
  r = client.try_send(&buf, 1, SOCKADDR_CONST_CAST(&lo_addr));
  ASSERT_EQ(r, 4);
  r = client.try_send(&buf, 1, nullptr);
  ASSERT_EQ(r, UV_EDESTADDRREQ);


  r = client.connect(SOCKADDR_CONST_CAST(&lo_addr));
  ASSERT_EQ(r, 0);
  r = client.send(&req, &buf, 1, SOCKADDR_CONST_CAST(&lo_addr), cl_send_cb);
  ASSERT_EQ(r, UV_EISCONN);
  r = client.send(&req, &buf, 1, nullptr, cl_send_cb);
  ASSERT_EQ(r, 0);

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  ASSERT_EQ(close_cb_called, 2);
  ASSERT_EQ(sv_recv_cb_called, 4);
  ASSERT_EQ(cl_send_cb_called, 2);

  ASSERT_EQ(client.uv_handle()->send_queue_size, 0);
  ASSERT_EQ(server.uv_handle()->send_queue_size, 0);

  make_valgrind_happy();
}
