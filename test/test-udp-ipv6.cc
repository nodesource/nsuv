#include "../include/nsuv-inl.h"
#include "./helpers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__NetBSD__)
#include <sys/sysctl.h>
#endif

using nsuv::ns_udp;
using nsuv::ns_udp_send;
using nsuv::ns_timer;

#define CHECK_HANDLE(handle)                                  \
  ASSERT((reinterpret_cast<uv_udp_t*>(handle) == &server      \
      || reinterpret_cast<uv_udp_t*>(handle) == &client       \
      || reinterpret_cast<uv_timer_t*>(handle) == &timeout))

#define CHECK_REQ(req) \
  ASSERT((req) == &req_);

static ns_udp client;
static ns_udp server;
static ns_udp_send req_;
static char data[10];
static ns_timer timeout;

static int send_cb_called;
static int recv_cb_called;
static int close_cb_called;
static uint16_t client_port;

#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__NetBSD__)
static int can_ipv6_ipv4_dual(void) {
  int v6only;
  size_t size = sizeof(int);

  if (sysctlbyname("net.inet6.ip6.v6only", &v6only, &size, nullptr, 0))
    return 0;

  return v6only != 1;
}
#endif


static void alloc_cb(uv_handle_t* handle,
                     size_t,
                     uv_buf_t* buf) {
  static char slab[65536];
  CHECK_HANDLE(handle);
  buf->base = slab;
  buf->len = sizeof(slab);
}


static void close_cb(ns_udp* handle) {
  CHECK_HANDLE(handle);
  close_cb_called++;
}


static void close_cb(ns_timer* handle) {
  CHECK_HANDLE(handle);
  close_cb_called++;
}


static void send_cb(ns_udp_send* req, int status) {
  CHECK_REQ(req);
  CHECK_HANDLE(req->handle());
  ASSERT(status == 0);
  send_cb_called++;
}

static int is_from_client(const struct sockaddr* addr) {
  const struct sockaddr_in6* addr6;
  char dst[256];
  int r;

  /* Debugging output, and filter out unwanted network traffic */
  if (addr != nullptr) {
    ASSERT(addr->sa_family == AF_INET6);
    addr6 = (struct sockaddr_in6*) addr;
    r = uv_inet_ntop(addr->sa_family, &addr6->sin6_addr, dst, sizeof(dst));
    if (addr6->sin6_port != client_port)
      return 0;
    if (r != 0 || strcmp(dst, "::ffff:127.0.0.1"))
      return 0;
  }
  return 1;
}


static void ipv6_recv_fail(uv_udp_t*,
                           ssize_t nread,
                           const uv_buf_t*,
                           const struct sockaddr* addr,
                           unsigned) {
  if (!is_from_client(addr) || (nread == 0 && addr == nullptr))
    return;
  FAIL("this function should not have been called");
}


static void ipv6_recv_ok(uv_udp_t* arg,
                         ssize_t nread,
                         const uv_buf_t* buf,
                         const struct sockaddr* addr,
                         unsigned) {
  ns_udp* handle = ns_udp::cast(arg);

  CHECK_HANDLE(handle);

  if (!is_from_client(addr) || (nread == 0 && addr == nullptr))
    return;

  ASSERT(nread == 9);
  ASSERT(!memcmp(buf->base, data, 9));
  recv_cb_called++;
}


static void timeout_cb(ns_timer*) {
  server.close(close_cb);
  client.close(close_cb);
  timeout.close(close_cb);
}


static void do_test(uv_udp_recv_cb recv_cb, int bind_flags) {
  struct sockaddr_in6 addr6;
  struct sockaddr_in addr;
  int addr6_len;
  int addr_len;
  uv_buf_t buf;
  char dst[256];
  int r;

  close_cb_called = 0;
  send_cb_called = 0;
  recv_cb_called = 0;

  ASSERT(0 == uv_ip6_addr("::0", kTestPort, &addr6));

  r = server.init(uv_default_loop());
  ASSERT(r == 0);

  r = server.bind(SOCKADDR_CONST_CAST(&addr6), bind_flags);
  ASSERT(r == 0);

  addr6_len = sizeof(addr6);
  ASSERT(server.getsockname(SOCKADDR_CAST(&addr6), &addr6_len) == 0);
  ASSERT(uv_inet_ntop(
      addr6.sin6_family, &addr6.sin6_addr, dst, sizeof(dst)) == 0);

  r = uv_udp_recv_start(&server, alloc_cb, recv_cb);
  ASSERT(r == 0);

  r = client.init(uv_default_loop());
  ASSERT(r == 0);

  ASSERT(0 == uv_ip4_addr("127.0.0.1", kTestPort, &addr));
  ASSERT(uv_inet_ntop(addr.sin_family, &addr.sin_addr, dst, sizeof(dst)) == 0);

  /* Create some unique data to send */
  ASSERT(9 == snprintf(data, sizeof(data), "PING%5u", uv_os_getpid() & 0xFFFF));
  buf = uv_buf_init(data, 9);

  r = client.send(&req_, &buf, 1, SOCKADDR_CONST_CAST(&addr), send_cb);
  ASSERT(r == 0);

  addr_len = sizeof(addr);
  ASSERT(client.getsockname(SOCKADDR_CAST(&addr), &addr_len) == 0);
  ASSERT(uv_inet_ntop(addr.sin_family, &addr.sin_addr, dst, sizeof(dst)) == 0);
  client_port = addr.sin_port;

  r = timeout.init(uv_default_loop());
  ASSERT(r == 0);

  r = timeout.start(timeout_cb, 500, 0);
  ASSERT(r == 0);

  CHECK(close_cb_called == 0);
  CHECK(send_cb_called == 0);
  CHECK(recv_cb_called == 0);

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  CHECK(close_cb_called == 3);

  make_valgrind_happy();
}


TEST_CASE("udp_dual_stack", "[udp]") {
#if defined(__CYGWIN__) || defined(__MSYS__)
  /* FIXME: Does Cygwin support this?  */
  RETURN_SKIP("FIXME: This test needs more investigation on Cygwin");
#endif

  if (!can_ipv6())
    RETURN_SKIP("IPv6 not supported");

#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__NetBSD__)
  if (!can_ipv6_ipv4_dual())
    RETURN_SKIP("IPv6-IPv4 dual stack not supported");
#elif defined(__OpenBSD__)
  RETURN_SKIP("IPv6-IPv4 dual stack not supported");
#endif

  do_test(ipv6_recv_ok, 0);

  ASSERT(recv_cb_called == 1);
  ASSERT(send_cb_called == 1);
}


TEST_CASE("udp_ipv6_only", "[udp]") {
  if (!can_ipv6())
    RETURN_SKIP("IPv6 not supported");

  do_test(ipv6_recv_fail, UV_UDP_IPV6ONLY);

  ASSERT(recv_cb_called == 0);
  ASSERT(send_cb_called == 1);
}
