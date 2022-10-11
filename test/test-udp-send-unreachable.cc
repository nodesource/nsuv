#include "../include/nsuv-inl.h"
#include "./helpers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using nsuv::ns_timer;
using nsuv::ns_udp;
using nsuv::ns_udp_send;

#define CHECK_HANDLE(handle) \
  ASSERT((reinterpret_cast<ns_udp*>(handle) == &client || \
        reinterpret_cast<ns_udp*>(handle) == &client2))

static ns_udp client;
static ns_udp client2;
static ns_timer timer;

static int send_cb_called;
static int recv_cb_called;
static int close_cb_called;
static int alloc_cb_called;
static int timer_cb_called;
static int can_recverr;

static char ping_str[] = "PING";
static char pang_str[] = "PANG";


static void alloc_cb(uv_handle_t* arg,
                     size_t suggested_size,
                     uv_buf_t* buf) {
  ns_udp* handle = ns_udp::cast(arg);
  static char slab[65536];
  CHECK_HANDLE(handle);
  ASSERT_LE(suggested_size, sizeof(slab));
  buf->base = slab;
  buf->len = sizeof(slab);
  alloc_cb_called++;
}


static void close_cb(ns_timer* handle) {
  ASSERT_EQ(1, handle->is_closing());
  close_cb_called++;
}


static void close_cb(ns_udp* handle) {
  ASSERT_EQ(1, handle->is_closing());
  close_cb_called++;
}


static void send_cb(ns_udp_send* req, int status) {
  ASSERT_NOT_NULL(req);
  ASSERT(status == 0);
  ASSERT_EQ(status, 0);
  CHECK_HANDLE(req->handle());
  send_cb_called++;
}

static void send_cb_recverr(ns_udp_send* req, int status) {
  ASSERT_PTR_NE(req, nullptr);
  ASSERT((status == 0 || status == UV_ECONNREFUSED));
  CHECK_HANDLE(req->handle());
  send_cb_called++;
}

static void recv_cb(uv_udp_t* arg,
                    ssize_t nread,
                    const uv_buf_t*,
                    const struct sockaddr* addr,
                    unsigned) {
  ns_udp* handle = ns_udp::cast(arg);
  CHECK_HANDLE(handle);
  recv_cb_called++;

  if (nread < 0) {
    FAIL("unexpected error");
  } else if (nread == 0) {
    /* Returning unused buffer */
    ASSERT_NULL(addr);
  } else {
    ASSERT_NOT_NULL(addr);
  }
}


static void timer_cb(ns_timer* h) {
  ASSERT_PTR_EQ(h, &timer);
  timer_cb_called++;
  client.close(close_cb);
  if (can_recverr)
    client2.close(close_cb);
  h->close(close_cb);
}


TEST_CASE("udp_send_unreachable", "[udp]") {
  struct sockaddr_in addr;
  struct sockaddr_in addr2;
  struct sockaddr_in addr3;
  ns_udp_send req1, req2, req3, req4;
  uv_buf_t buf;
  int r;

#ifdef __linux__
  can_recverr = 1;
#endif

  ASSERT_EQ(0, uv_ip4_addr("127.0.0.1", kTestPort, &addr));
  ASSERT_EQ(0, uv_ip4_addr("127.0.0.1", kTestPort2, &addr2));
  ASSERT_EQ(0, uv_ip4_addr("127.0.0.1", kTestPort3, &addr3));

  r = timer.init(uv_default_loop());
  ASSERT_EQ(r, 0);

  r = timer.start(timer_cb, 1000, 0);
  ASSERT_EQ(r, 0);

  r = client.init(uv_default_loop());
  ASSERT_EQ(r, 0);

  r = client.bind(SOCKADDR_CONST_CAST(&addr2), 0);
  ASSERT_EQ(r, 0);

  r = uv_udp_recv_start(&client, alloc_cb, recv_cb);
  ASSERT_EQ(r, 0);

  /* client sends "PING", then "PANG" */
  buf = uv_buf_init(ping_str, 4);

  r = client.send(&req1, &buf, 1, SOCKADDR_CONST_CAST(&addr), send_cb);
  ASSERT_EQ(r, 0);

  buf = uv_buf_init(pang_str, 4);

  r = client.send(&req2, &buf, 1, SOCKADDR_CONST_CAST(&addr), send_cb);
  ASSERT_EQ(r, 0);

  if (can_recverr) {
    r = client2.init(uv_default_loop());
    ASSERT_EQ(r, 0);

    r = client2.bind(SOCKADDR_CONST_CAST(&addr3), UV_UDP_LINUX_RECVERR);
    ASSERT_EQ(r, 0);

    r = uv_udp_recv_start(&client2, alloc_cb, recv_cb);
    ASSERT_EQ(r, 0);

    /* client sends "PING", then "PANG" */
    buf = uv_buf_init(ping_str, 4);

    r = client2.send(
        &req3, &buf, 1, SOCKADDR_CONST_CAST(&addr), send_cb_recverr);
    ASSERT_EQ(r, 0);

    buf = uv_buf_init(pang_str, 4);

    r = client2.send(
        &req4, &buf, 1, SOCKADDR_CONST_CAST(&addr), send_cb_recverr);
    ASSERT_EQ(r, 0);
  }

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  ASSERT_EQ(send_cb_called, (can_recverr ? 4 : 2));
  ASSERT_EQ(recv_cb_called, alloc_cb_called);
  ASSERT_EQ(timer_cb_called, 1);
  ASSERT_EQ(close_cb_called, (can_recverr ? 3 : 2));

  make_valgrind_happy();
}
