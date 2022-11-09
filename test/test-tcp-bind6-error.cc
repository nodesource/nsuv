#include "../include/nsuv-inl.h"
#include "./helpers.h"

#include <stdio.h>
#include <stdlib.h>

using nsuv::ns_tcp;

static int close_cb_called = 0;


static void close_cb(ns_tcp* handle) {
  ASSERT_NOT_NULL(handle);
  close_cb_called++;
}


TEST_CASE("tcp_bind6_error_addrinuse", "[tcp]") {
  struct sockaddr_in6 addr;
  ns_tcp server1, server2;
  int r;

  close_cb_called = 0;

  if (!can_ipv6())
    RETURN_SKIP("IPv6 not supported");

  ASSERT(0 == uv_ip6_addr("::", kTestPort, &addr));

  r = server1.init(uv_default_loop());
  ASSERT(r == 0);
  r = server1.bind(SOCKADDR_CONST_CAST(&addr), 0);
  ASSERT(r == 0);

  r = server2.init(uv_default_loop());
  ASSERT(r == 0);
  r = server2.bind(SOCKADDR_CONST_CAST(&addr), 0);
  ASSERT(r == 0);

  r = server1.listen(128, nullptr);
  ASSERT(r == 0);
  r = server2.listen(128, nullptr);
  ASSERT(r == UV_EADDRINUSE);

  server1.close(close_cb);
  server2.close(close_cb);

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  ASSERT(close_cb_called == 2);

  make_valgrind_happy();
}


TEST_CASE("tcp_bind6_error_addrnotavail", "[tcp]") {
  struct sockaddr_in6 addr;
  ns_tcp server;
  int r;

  close_cb_called = 0;

  if (!can_ipv6())
    RETURN_SKIP("IPv6 not supported");

  ASSERT(0 == uv_ip6_addr("4:4:4:4:4:4:4:4", kTestPort, &addr));

  r = server.init(uv_default_loop());
  ASSERT(r == 0);
  r = server.bind(SOCKADDR_CONST_CAST(&addr), 0);
  ASSERT(r == UV_EADDRNOTAVAIL);

  server.close(close_cb);

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  ASSERT(close_cb_called == 1);

  make_valgrind_happy();
}


TEST_CASE("tcp_bind6_error_fault", "[tcp]") {
  char garbage[] =
      "blah blah blah blah blah blah blah blah blah blah blah blah";
  struct sockaddr_in6* garbage_addr;
  ns_tcp server;
  int r;

  close_cb_called = 0;

  if (!can_ipv6())
    RETURN_SKIP("IPv6 not supported");

  garbage_addr = reinterpret_cast<struct sockaddr_in6*>(&garbage);

  r = server.init(uv_default_loop());
  ASSERT(r == 0);
  r = server.bind(SOCKADDR_CONST_CAST(garbage_addr), 0);
  ASSERT(r == UV_EINVAL);

  server.close(close_cb);

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  ASSERT(close_cb_called == 1);

  make_valgrind_happy();
}

/* Notes: On Linux uv_bind6(server, NULL) will segfault the program.  */

TEST_CASE("tcp_bind6_error_inval", "[tcp]") {
  struct sockaddr_in6 addr1;
  struct sockaddr_in6 addr2;
  ns_tcp server;
  int r;

  close_cb_called = 0;

  if (!can_ipv6())
    RETURN_SKIP("IPv6 not supported");

  ASSERT(0 == uv_ip6_addr("::", kTestPort, &addr1));
  ASSERT(0 == uv_ip6_addr("::", kTestPort2, &addr2));

  r = server.init(uv_default_loop());
  ASSERT(r == 0);
  r = server.bind(SOCKADDR_CONST_CAST(&addr1), 0);
  ASSERT(r == 0);
  r = server.bind(SOCKADDR_CONST_CAST(&addr2), 0);
  ASSERT(r == UV_EINVAL);

  server.close(close_cb);

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  ASSERT(close_cb_called == 1);

  make_valgrind_happy();
}


TEST_CASE("tcp_bind6_localhost_ok", "[tcp]") {
  struct sockaddr_in6 addr;
  ns_tcp server;
  int r;

  close_cb_called = 0;

  if (!can_ipv6())
    RETURN_SKIP("IPv6 not supported");

  ASSERT(0 == uv_ip6_addr("::1", kTestPort, &addr));

  r = server.init(uv_default_loop());
  ASSERT(r == 0);
  r = server.bind(SOCKADDR_CONST_CAST(&addr), 0);
  ASSERT(r == 0);

  make_valgrind_happy();
}
