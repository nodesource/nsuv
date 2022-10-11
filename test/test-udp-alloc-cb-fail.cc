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
static int cl_recv_cb_called;

static int sv_send_cb_called;
static int sv_recv_cb_called;

static int close_cb_called;

static char ping_str[] = "PING";
static char pong_str[] = "PONG";


static void sv_alloc_cb(uv_handle_t* handle, size_t, uv_buf_t* buf) {
  static char slab[65536];
  CHECK_HANDLE(handle);
  buf->base = slab;
  buf->len = sizeof(slab);
}


static void cl_alloc_cb(uv_handle_t*, size_t, uv_buf_t*) {
  /* Do nothing, recv_cb should be called with UV_ENOBUFS. */
}


static void close_cb(ns_udp* handle) {
  CHECK_HANDLE(handle);
  ASSERT(1 == handle->is_closing());
  close_cb_called++;
}


static void cl_recv_cb(uv_udp_t* arg,
                       ssize_t nread,
                       const uv_buf_t*,
                       const struct sockaddr*,
                       unsigned flags) {
  ns_udp* handle = ns_udp::cast(arg);

  CHECK_HANDLE(handle);
  ASSERT(flags == 0);
  ASSERT(nread == UV_ENOBUFS);

  cl_recv_cb_called++;

  handle->close(close_cb);
}


static void cl_send_cb(ns_udp_send* req, int status) {
  int r;

  ASSERT_NOT_NULL(req);
  ASSERT(status == 0);
  CHECK_HANDLE(req->handle());

  r = uv_udp_recv_start(req->handle(), cl_alloc_cb, cl_recv_cb);
  ASSERT(r == 0);

  cl_send_cb_called++;
}


static void sv_send_cb(ns_udp_send* req, int status) {
  ASSERT_NOT_NULL(req);
  ASSERT(status == 0);
  CHECK_HANDLE(req->handle());

  req->handle()->close(close_cb);
  delete req;

  sv_send_cb_called++;
}


static void sv_recv_cb(uv_udp_t* arg,
                       ssize_t nread,
                       const uv_buf_t* rcvbuf,
                       const struct sockaddr* addr,
                       unsigned flags) {
  ns_udp* handle = ns_udp::cast(arg);
  ns_udp_send* req;
  uv_buf_t sndbuf;
  int r;

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
  ASSERT(!memcmp(ping_str, rcvbuf->base, nread));

  r = uv_udp_recv_stop(handle);
  ASSERT(r == 0);

  req = new (std::nothrow) ns_udp_send();
  ASSERT_NOT_NULL(req);

  sndbuf = uv_buf_init(pong_str, 4);
  r = handle->send(req, &sndbuf, 1, addr, sv_send_cb);
  ASSERT(r == 0);

  sv_recv_cb_called++;
}


TEST_CASE("udp_alloc_cb_fail", "[udp]") {
  struct sockaddr_in addr;
  ns_udp_send req;
  uv_buf_t buf;
  int r;

  ASSERT(0 == uv_ip4_addr("0.0.0.0", kTestPort, &addr));

  r = server.init(uv_default_loop());
  ASSERT(r == 0);

  r = server.bind(SOCKADDR_CONST_CAST(&addr), 0);
  ASSERT(r == 0);

  r = uv_udp_recv_start(&server, sv_alloc_cb, sv_recv_cb);
  ASSERT(r == 0);

  ASSERT(0 == uv_ip4_addr("127.0.0.1", kTestPort, &addr));

  r = client.init(uv_default_loop());
  ASSERT(r == 0);

  buf = uv_buf_init(ping_str, 4);
  r = client.send(&req, &buf, 1, SOCKADDR_CONST_CAST(&addr), cl_send_cb);
  ASSERT(r == 0);

  ASSERT(close_cb_called == 0);
  ASSERT(cl_send_cb_called == 0);
  ASSERT(cl_recv_cb_called == 0);
  ASSERT(sv_send_cb_called == 0);
  ASSERT(sv_recv_cb_called == 0);

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  ASSERT(cl_send_cb_called == 1);
  ASSERT(cl_recv_cb_called == 1);
  ASSERT(sv_send_cb_called == 1);
  ASSERT(sv_recv_cb_called == 1);
  ASSERT(close_cb_called == 2);

  make_valgrind_happy();
}
