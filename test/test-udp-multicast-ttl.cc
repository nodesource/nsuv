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

static int sv_send_cb_called;
static int close_cb_called;

static char ping_str[] = "PING";


static void close_cb(ns_udp* handle) {
  CHECK_HANDLE(handle);
  close_cb_called++;
}


static void sv_send_cb(ns_udp_send* req, int status) {
  ASSERT_NOT_NULL(req);
  ASSERT((status == 0 || status == UV_ENETUNREACH || status == UV_EPERM));
  CHECK_HANDLE(req->handle());

  sv_send_cb_called++;

  req->handle()->close(close_cb);
}


TEST_CASE("udp_multicast_ttl", "[udp]") {
  int r;
  ns_udp_send req;
  uv_buf_t buf;
  struct sockaddr_in addr;

  r = server.init(uv_default_loop());
  ASSERT(r == 0);

  ASSERT(0 == uv_ip4_addr("0.0.0.0", 0, &addr));
  r = server.bind(SOCKADDR_CONST_CAST(&addr), 0);
  ASSERT(r == 0);

  r = uv_udp_set_multicast_ttl(&server, 32);
  ASSERT(r == 0);

  /* server sends "PING" */
  buf = uv_buf_init(ping_str, 4);
  ASSERT(0 == uv_ip4_addr("239.255.0.1", kTestPort, &addr));
  r = server.send(&req, &buf, 1, SOCKADDR_CONST_CAST(&addr), sv_send_cb);
  ASSERT(r == 0);

  ASSERT(close_cb_called == 0);
  ASSERT(sv_send_cb_called == 0);

  /* run the loop till all events are processed */
  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  ASSERT(sv_send_cb_called == 1);
  ASSERT(close_cb_called == 1);

  make_valgrind_happy();
}
