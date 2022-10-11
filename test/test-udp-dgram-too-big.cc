#include "../include/nsuv-inl.h"
#include "./helpers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using nsuv::ns_udp;
using nsuv::ns_udp_send;

#define CHECK_HANDLE(handle) \
  ASSERT(reinterpret_cast<ns_udp*>(handle) == &handle_)

#define CHECK_REQ(req) \
  ASSERT((req) == &req_);

static ns_udp handle_;
static ns_udp_send req_;

static int send_cb_called;
static int close_cb_called;


static void close_cb(ns_udp* handle) {
  CHECK_HANDLE(handle);
  close_cb_called++;
}


static void send_cb(ns_udp_send* req, int status) {
  CHECK_REQ(req);
  CHECK_HANDLE(req->handle());

  ASSERT(status == UV_EMSGSIZE);

  req->handle()->close(close_cb);
  send_cb_called++;
}


TEST_CASE("udp_dgram_too_big", "[udp]") {
  char dgram[65536]; /* 64K MTU is unlikely, even on localhost */
  struct sockaddr_in addr;
  uv_buf_t buf;
  int r;

  memset(dgram, 42, sizeof dgram); /* silence valgrind */

  r = handle_.init(uv_default_loop());
  ASSERT(r == 0);

  buf = uv_buf_init(dgram, sizeof dgram);
  ASSERT(0 == uv_ip4_addr("127.0.0.1", kTestPort, &addr));

  r = handle_.send(&req_, &buf, 1, SOCKADDR_CONST_CAST(&addr), send_cb);
  ASSERT(r == 0);

  ASSERT(close_cb_called == 0);
  ASSERT(send_cb_called == 0);

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  ASSERT(send_cb_called == 1);
  ASSERT(close_cb_called == 1);

  make_valgrind_happy();
}
