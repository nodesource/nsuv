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

static int sv_recv_cb_called;

static int close_cb_called;

static char exit_str[] = "EXIT";


static void alloc_cb(uv_handle_t* handle,
                     size_t suggested_size,
                     uv_buf_t* buf) {
  static char slab[65536];
  CHECK_HANDLE(handle);
  ASSERT(suggested_size <= sizeof(slab));
  buf->base = slab;
  buf->len = sizeof(slab);
}


static void close_cb(ns_udp* handle) {
  CHECK_HANDLE(handle);
  ASSERT(handle->is_closing());
  close_cb_called++;
}


static void sv_recv_cb(uv_udp_t* arg,
                       ssize_t nread,
                       const uv_buf_t* rcvbuf,
                       const struct sockaddr* addr,
                       unsigned) {
  ns_udp* handle = ns_udp::cast(arg);
  ASSERT(nread > 0);

  if (nread == 0) {
    ASSERT_NULL(addr);
    return;
  }

  ASSERT(nread == 4);
  ASSERT_NOT_NULL(addr);

  ASSERT(memcmp(exit_str, rcvbuf->base, nread) == 0);
  handle->close(close_cb);
  client.close(close_cb);

  sv_recv_cb_called++;
}


TEST_CASE("udp_try_send", "[udp]") {
  struct sockaddr_in addr;
  static char buffer[64 * 1024];
  uv_buf_t buf;
  int r;

  ASSERT(0 == uv_ip4_addr("0.0.0.0", kTestPort, &addr));

  r = server.init(uv_default_loop());
  ASSERT(r == 0);

  r = server.bind(SOCKADDR_CONST_CAST(&addr), 0);
  ASSERT(r == 0);

  r = uv_udp_recv_start(&server, alloc_cb, sv_recv_cb);
  ASSERT(r == 0);

  ASSERT(0 == uv_ip4_addr("127.0.0.1", kTestPort, &addr));

  r = client.init(uv_default_loop());
  ASSERT(r == 0);

  buf = uv_buf_init(buffer, sizeof(buffer));
  r = client.try_send(&buf, 1, SOCKADDR_CONST_CAST(&addr));
  ASSERT(r == UV_EMSGSIZE);

  buf = uv_buf_init(exit_str, 4);
  r = client.try_send(&buf, 1, SOCKADDR_CONST_CAST(&addr));
  ASSERT(r == 4);

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  ASSERT(close_cb_called == 2);
  ASSERT(sv_recv_cb_called == 1);

  ASSERT(client.uv_handle()->send_queue_size == 0);
  ASSERT(server.uv_handle()->send_queue_size == 0);

  make_valgrind_happy();
}
