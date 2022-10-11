#include "../include/nsuv-inl.h"
#include "./helpers.h"

#include <string.h>

using nsuv::ns_udp;
using nsuv::ns_udp_send;

#ifdef _WIN32
# define INVALID_FD (INVALID_HANDLE_VALUE)
#else
# define INVALID_FD (-1)
#endif


TEST_CASE("udp_create_early", "[udp]") {
  struct sockaddr_in addr;
  struct sockaddr_in sockname;
  ns_udp client;
  uv_os_fd_t fd;
  int r, namelen;

  ASSERT(0 == uv_ip4_addr("127.0.0.1", kTestPort, &addr));

  r = client.init_ex(uv_default_loop(), AF_INET);
  ASSERT(r == 0);

  r = uv_fileno(client.base_handle(), &fd);
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


TEST_CASE("udp_create_early_bad_bind", "[udp]") {
  struct sockaddr_in addr;
  ns_udp client;
  uv_os_fd_t fd;
  int r;

  if (!can_ipv6())
    RETURN_SKIP("IPv6 not supported");

  ASSERT(0 == uv_ip4_addr("127.0.0.1", kTestPort, &addr));

  r = client.init_ex(uv_default_loop(), AF_INET6);
  ASSERT(r == 0);

  r = uv_fileno(client.base_handle(), &fd);
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


TEST_CASE("udp_create_early_bad_domain", "[udp]") {
  ns_udp client;
  int r;

  r = client.init_ex(uv_default_loop(), 47);
  ASSERT(r == UV_EINVAL);

  r = client.init_ex(uv_default_loop(), 1024);
  ASSERT(r == UV_EINVAL);

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  make_valgrind_happy();
}
