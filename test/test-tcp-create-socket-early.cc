#include "../include/nsuv-inl.h"
#include "./helpers.h"

using nsuv::ns_connect;
using nsuv::ns_tcp;

#ifdef _WIN32
# define INVALID_FD (INVALID_HANDLE_VALUE)
#else
# define INVALID_FD (-1)
#endif


static void on_connect(ns_connect<ns_tcp>* req, int status) {
  ASSERT(status == 0);
  req->handle()->close();
}


static void on_connection(ns_tcp* server, int status) {
  ns_tcp* handle;
  int r;

  ASSERT(status == 0);

  handle = new (std::nothrow) ns_tcp();
  ASSERT_NOT_NULL(handle);

  r = handle->init_ex(server->get_loop(), AF_INET);
  ASSERT(r == 0);

  r = server->accept(handle);
  ASSERT(r == UV_EBUSY);

  server->close();
  handle->close_and_delete();
}


static void tcp_listener(uv_loop_t* loop, ns_tcp* server) {
  struct sockaddr_in addr;
  int r;

  ASSERT(0 == uv_ip4_addr("0.0.0.0", kTestPort, &addr));

  r = server->init(loop);
  ASSERT(r == 0);

  r = server->bind(SOCKADDR_CONST_CAST(&addr), 0);
  ASSERT(r == 0);

  r = server->listen(128, on_connection);
  ASSERT(r == 0);
}


static void tcp_connector(uv_loop_t* loop,
                          ns_tcp* client,
                          ns_connect<ns_tcp>* req) {
  struct sockaddr_in server_addr;
  int r;

  ASSERT(0 == uv_ip4_addr("127.0.0.1", kTestPort, &server_addr));

  r = client->init(loop);
  ASSERT(r == 0);

  r = client->connect(req, SOCKADDR_CONST_CAST(&server_addr), on_connect);
  ASSERT(r == 0);
}


TEST_CASE("tcp_create_early", "[tcp]") {
  struct sockaddr_in addr;
  struct sockaddr_in sockname;
  ns_tcp client;
  uv_os_fd_t fd;
  int r, namelen;

  ASSERT(0 == uv_ip4_addr("127.0.0.1", kTestPort, &addr));

  r = client.init_ex(uv_default_loop(), AF_INET);
  ASSERT(r == 0);

  r = uv_fileno(reinterpret_cast<const uv_handle_t*>(&client), &fd);
  ASSERT(r == 0);
  ASSERT(fd != INVALID_FD);

  /* Windows returns WSAEINVAL if the socket is not bound */
#ifndef _WIN32
  namelen = sizeof sockname;
  r = client.getsockname(SOCKADDR_CAST(&sockname), &namelen);
  ASSERT(r == 0);
  ASSERT(sockname.sin_family == AF_INET);
#endif

  r = client.bind(SOCKADDR_CONST_CAST(&addr), 0);
  ASSERT(r == 0);

  namelen = sizeof sockname;
  r = client.getsockname(SOCKADDR_CAST(&sockname), &namelen);
  ASSERT(r == 0);
  ASSERT(memcmp(&addr.sin_addr,
                &sockname.sin_addr,
                sizeof(addr.sin_addr)) == 0);

  client.close();
  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  make_valgrind_happy();
}


TEST_CASE("tcp_create_early_bad_bind", "[tcp]") {
  struct sockaddr_in addr;
  ns_tcp client;
  uv_os_fd_t fd;
  int r;

  if (!can_ipv6())
    RETURN_SKIP("IPv6 not supported");

  ASSERT(0 == uv_ip4_addr("127.0.0.1", kTestPort, &addr));

  r = client.init_ex(uv_default_loop(), AF_INET6);
  ASSERT(r == 0);

  r = uv_fileno(reinterpret_cast<const uv_handle_t*>(&client), &fd);
  ASSERT(r == 0);
  ASSERT(fd != INVALID_FD);

  /* Windows returns WSAEINVAL if the socket is not bound */
#ifndef _WIN32
  {
    int namelen;
    struct sockaddr_in6 sockname;
    namelen = sizeof sockname;
    r = client.getsockname(SOCKADDR_CAST(&sockname), &namelen);
    ASSERT(r == 0);
    ASSERT(sockname.sin6_family == AF_INET6);
  }
#endif

  r = client.bind(SOCKADDR_CONST_CAST(&addr), 0);
#if !defined(_WIN32) && !defined(__CYGWIN__) && !defined(__MSYS__)
  ASSERT(r == UV_EINVAL);
#else
  ASSERT(r == UV_EFAULT);
#endif

  client.close();
  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  make_valgrind_happy();
}


TEST_CASE("tcp_create_early_bad_domain", "[tcp]") {
  ns_tcp client;
  int r;

  r = client.init_ex(uv_default_loop(), 47);
  ASSERT(r == UV_EINVAL);

  r = client.init_ex(uv_default_loop(), 1024);
  ASSERT(r == UV_EINVAL);

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  make_valgrind_happy();
}


TEST_CASE("tcp_create_early_accept", "[tcp]") {
  ns_tcp client, server;
  ns_connect<ns_tcp> connect_req;

  tcp_listener(uv_default_loop(), &server);
  tcp_connector(uv_default_loop(), &client, &connect_req);

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  make_valgrind_happy();
}
