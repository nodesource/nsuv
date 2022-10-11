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

static int cl_send_cb_called;
static int sv_recv_cb_called;
static int close_cb_called;

static char ping_str[] = "PING";
static char pang_str[] = "PANG";


static void alloc_cb(uv_handle_t* arg,
                     size_t suggested_size,
                     uv_buf_t* buf) {
  static char slab[65536];
  CHECK_HANDLE(ns_udp::cast(arg));
  ASSERT(suggested_size <= sizeof(slab));
  buf->base = slab;
  buf->len = sizeof(slab);
}


static void close_cb(ns_udp* handle) {
  CHECK_HANDLE(handle);
  ASSERT(1 == handle->is_closing());
  close_cb_called++;
}


static void cl_send_cb(ns_udp_send* req, int status) {
  ASSERT_NOT_NULL(req);
  ASSERT(status == 0);
  CHECK_HANDLE(req->handle());

  cl_send_cb_called++;
}


static void sv_recv_cb(uv_udp_t* arg,
                       ssize_t nread,
                       const uv_buf_t* rcvbuf,
                       const struct sockaddr* addr,
                       unsigned flags) {
  ns_udp* handle = ns_udp::cast(arg);

  if (nread < 0) {
    FAIL("unexpected error");
  }

  if (nread == 0) {
    /* Returning unused buffer. Don't count towards sv_recv_cb_called */
    ASSERT_NULL(addr);
    return;
  }

  CHECK_HANDLE(handle);
  ASSERT(flags == 0);

  ASSERT_NOT_NULL(addr);
  ASSERT(nread == 4);
  ASSERT((memcmp(ping_str, rcvbuf->base, nread) == 0 ||
         memcmp(pang_str, rcvbuf->base, nread) == 0));

  if (++sv_recv_cb_called == 2) {
    server.close(close_cb);
    client.close(close_cb);
  }
}


TEST_CASE("udp_send_immediate", "[udp]") {
  struct sockaddr_in addr;
  ns_udp_send req1, req2;
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

  /* client sends "PING", then "PANG" */
  buf = uv_buf_init(ping_str, 4);

  r = client.send(&req1, &buf, 1, SOCKADDR_CONST_CAST(&addr), cl_send_cb);
  ASSERT(r == 0);

  buf = uv_buf_init(pang_str, 4);

  r = client.send(&req2, &buf, 1, SOCKADDR_CONST_CAST(&addr), cl_send_cb);
  ASSERT(r == 0);

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  ASSERT(cl_send_cb_called == 2);
  ASSERT(sv_recv_cb_called == 2);
  ASSERT(close_cb_called == 2);

  make_valgrind_happy();
}
