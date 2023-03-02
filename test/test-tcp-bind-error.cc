#include "../include/nsuv-inl.h"
#include "./helpers.h"

#include <stdio.h>
#include <stdlib.h>

using nsuv::ns_tcp;
using nsuv::ns_write;
using nsuv::ns_connect;

static int connect_cb_called = 0;
static int close_cb_called = 0;
static char ping_cstr[] = "PING";


static void close_cb(ns_tcp* handle) {
  ASSERT_NOT_NULL(handle);
  close_cb_called++;
}


static void connect_cb(ns_connect<ns_tcp>* req, int status) {
  ASSERT(status == UV_EADDRINUSE);
  req->handle()->close(close_cb);
  connect_cb_called++;
}


TEST_CASE("tcp_bind_error_addrinuse_connect", "[tcp]") {
  struct sockaddr_in addr;
  int addrlen;
  ns_connect<ns_tcp> req;
  ns_tcp conn;

  // TODO(trevnorris): tcp4_echo_server is not yet implemented.
  RETURN_OK();

  close_cb_called = 0;

  /* 127.0.0.1:<kTestPort> is already taken by tcp4_echo_server running in
   * another process. uv_tcp_bind() and uv_tcp_connect() should still succeed
   * (greatest common denominator across platforms) but the connect callback
   * should receive an UV_EADDRINUSE error.
   */
  ASSERT(0 == conn.init(uv_default_loop()));
  ASSERT(0 == uv_ip4_addr("127.0.0.1", kTestPort, &addr));
  ASSERT(0 == conn.bind(SOCKADDR_CONST_CAST(&addr), 0));

  ASSERT(0 == uv_ip4_addr("127.0.0.1", kTestPort2, &addr));
  ASSERT(0 == conn.connect(&req, SOCKADDR_CONST_CAST(&addr), connect_cb));

  addrlen = sizeof(addr);
  ASSERT(UV_EADDRINUSE == conn.getsockname(SOCKADDR_CAST(&addr), &addrlen));

  ASSERT(0 == uv_run(uv_default_loop(), UV_RUN_DEFAULT));
  ASSERT(connect_cb_called == 1);
  ASSERT(close_cb_called == 1);

  make_valgrind_happy();
}


TEST_CASE("tcp_bind_error_addrinuse_listen", "[tcp]") {
  struct sockaddr_in addr;
  ns_tcp server1, server2;
  int r;

  close_cb_called = 0;

  ASSERT(0 == uv_ip4_addr("0.0.0.0", kTestPort, &addr));
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


TEST_CASE("tcp_bind_error_addrnotavail_1", "[tcp]") {
  struct sockaddr_in addr;
  ns_tcp server;
  int r;

  close_cb_called = 0;

  ASSERT(0 == uv_ip4_addr("127.255.255.255", kTestPort, &addr));

  r = server.init(uv_default_loop());
  ASSERT(r == 0);

  /* It seems that Linux is broken here - bind succeeds. */
  r = server.bind(SOCKADDR_CONST_CAST(&addr), 0);
  ASSERT((r == 0 || r == UV_EADDRNOTAVAIL));

  server.close(close_cb);

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  ASSERT(close_cb_called == 1);

  make_valgrind_happy();
}


TEST_CASE("tcp_bind_error_addrnotavail_2", "[tcp]") {
  struct sockaddr_in addr;
  ns_tcp server;
  int r;

  close_cb_called = 0;

  ASSERT(0 == uv_ip4_addr("4.4.4.4", kTestPort, &addr));

  r = server.init(uv_default_loop());
  ASSERT(r == 0);
  r = server.bind(SOCKADDR_CONST_CAST(&addr), 0);
  ASSERT(r == UV_EADDRNOTAVAIL);

  server.close(close_cb);

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  ASSERT(close_cb_called == 1);

  make_valgrind_happy();
}


TEST_CASE("tcp_bind_error_fault", "[tcp]") {
  char garbage[] =
      "blah blah blah blah blah blah blah blah blah blah blah blah";
  struct sockaddr_in* garbage_addr;
  ns_tcp server;
  int r;

  close_cb_called = 0;

  garbage_addr = reinterpret_cast<struct sockaddr_in*>(&garbage);

  r = server.init(uv_default_loop());
  ASSERT(r == 0);
  r = server.bind(SOCKADDR_CONST_CAST(garbage_addr), 0);
  ASSERT(r == UV_EINVAL);

  server.close(close_cb);

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  ASSERT(close_cb_called == 1);

  make_valgrind_happy();
}

/* Notes: On Linux uv_bind(server, NULL) will segfault the program.  */

TEST_CASE("tcp_bind_error_inval", "[tcp]") {
  struct sockaddr_in addr1;
  struct sockaddr_in addr2;
  ns_tcp server;
  int r;

  close_cb_called = 0;

  ASSERT(0 == uv_ip4_addr("0.0.0.0", kTestPort, &addr1));
  ASSERT(0 == uv_ip4_addr("0.0.0.0", kTestPort2, &addr2));

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


TEST_CASE("tcp_bind_localhost_ok", "[tcp]") {
  struct sockaddr_in addr;
  ns_tcp server;
  int r;

  ASSERT(0 == uv_ip4_addr("127.0.0.1", kTestPort, &addr));

  r = server.init(uv_default_loop());
  ASSERT(r == 0);
  r = server.bind(SOCKADDR_CONST_CAST(&addr), 0);
  ASSERT(r == 0);

  make_valgrind_happy();
}


TEST_CASE("tcp_bind_invalid_flags", "[tcp]") {
  struct sockaddr_in addr;
  ns_tcp server;
  int r;

  ASSERT(0 == uv_ip4_addr("127.0.0.1", kTestPort, &addr));

  r = server.init(uv_default_loop());
  ASSERT(r == 0);
  r = server.bind(SOCKADDR_CONST_CAST(&addr), UV_TCP_IPV6ONLY);
  ASSERT(r == UV_EINVAL);

  make_valgrind_happy();
}


TEST_CASE("tcp_listen_without_bind", "[tcp]") {
  int r;
  ns_tcp server;

  r = server.init(uv_default_loop());
  ASSERT(r == 0);
  r = server.listen(128, nullptr);
  ASSERT(r == 0);

  make_valgrind_happy();
}


TEST_CASE("tcp_bind_writable_flags", "[tcp]") {
  struct sockaddr_in addr;
  ns_tcp server;
  uv_buf_t buf;
  ns_write<ns_tcp> write_req;
  uv_shutdown_t shutdown_req;
  int r;

  close_cb_called = 0;

  ASSERT(0 == uv_ip4_addr("0.0.0.0", kTestPort, &addr));
  r = server.init(uv_default_loop());
  ASSERT(r == 0);
  r = server.bind(SOCKADDR_CONST_CAST(&addr), 0);
  ASSERT(r == 0);
  r = server.listen(128, nullptr);
  ASSERT(r == 0);

  ASSERT(0 == server.is_writable());
  ASSERT(0 == server.is_readable());

  buf = uv_buf_init(ping_cstr, 4);
  r = server.write(&write_req, &buf, 1, nullptr);
  ASSERT(r == UV_EPIPE);
  r = uv_shutdown(&shutdown_req, server.base_stream(), nullptr);
  ASSERT(r == UV_ENOTCONN);
  r = uv_read_start(server.base_stream(),
                    (uv_alloc_cb) abort,
                    (uv_read_cb) abort);
  ASSERT(r == UV_ENOTCONN);

  server.close(close_cb);

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  ASSERT(close_cb_called == 1);

  make_valgrind_happy();
}
