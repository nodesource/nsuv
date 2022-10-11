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

#define MULTICAST_ADDR "239.255.0.1"

static ns_udp server;
static ns_udp client;
static ns_udp_send req;
static ns_udp_send req_ss;

static int cl_recv_cb_called;

static int sv_send_cb_called;

static int close_cb_called;

static char ping_str[] = "PING";


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
  close_cb_called++;
}


static void sv_send_cb(ns_udp_send* req, int status) {
  ASSERT_NOT_NULL(req);
  ASSERT(status == 0);
  CHECK_HANDLE(req->handle());

  sv_send_cb_called++;

  if (sv_send_cb_called == 2)
    req->handle()->close(close_cb);
}


static int do_send(ns_udp_send* send_req) {
  uv_buf_t buf;
  struct sockaddr_in addr;

  buf = uv_buf_init(ping_str, 4);

  ASSERT(0 == uv_ip4_addr(MULTICAST_ADDR, kTestPort, &addr));

  /* client sends "PING" */
  return client.send(send_req, &buf, 1, SOCKADDR_CONST_CAST(&addr), sv_send_cb);
}


static void cl_recv_cb(uv_udp_t* arg,
                       ssize_t nread,
                       const uv_buf_t* buf,
                       const struct sockaddr* addr,
                       unsigned flags) {
  ns_udp* handle = ns_udp::cast(arg);

  CHECK_HANDLE(handle);
  ASSERT(flags == 0);

  if (nread < 0) {
    FAIL("unexpected error");
  }

  if (nread == 0) {
    /* Returning unused buffer. Don't count towards cl_recv_cb_called */
    ASSERT_NULL(addr);
    return;
  }

  ASSERT_NOT_NULL(addr);
  ASSERT(nread == 4);
  ASSERT(!memcmp(ping_str, buf->base, nread));

  cl_recv_cb_called++;

  if (cl_recv_cb_called == 2) {
    /* we are done with the server handle, we can close it */
    server.close(close_cb);
  } else {
    int r;
    char source_addr[64];

    r = uv_ip4_name(reinterpret_cast<const struct sockaddr_in*>(addr),
                    source_addr,
                    sizeof(source_addr));
    ASSERT(r == 0);

    r = uv_udp_set_membership(&server, MULTICAST_ADDR, nullptr, UV_LEAVE_GROUP);
    ASSERT(r == 0);

#if !defined(__OpenBSD__) && !defined(__NetBSD__)
    r = uv_udp_set_source_membership(
        &server, MULTICAST_ADDR, nullptr, source_addr, UV_JOIN_GROUP);
    ASSERT(r == 0);
#endif

    r = do_send(&req_ss);
    ASSERT(r == 0);
  }
}


TEST_CASE("udp_multicast_join", "[udp]") {
  int r;
  struct sockaddr_in addr;

  ASSERT(0 == uv_ip4_addr("0.0.0.0", kTestPort, &addr));

  r = server.init(uv_default_loop());
  ASSERT(r == 0);

  r = client.init(uv_default_loop());
  ASSERT(r == 0);

  /* bind to the desired port */
  r = server.bind(SOCKADDR_CONST_CAST(&addr), 0);
  ASSERT(r == 0);

  /* join the multicast channel */
  r = uv_udp_set_membership(&server, MULTICAST_ADDR, nullptr, UV_JOIN_GROUP);
  if (r == UV_ENODEV)
    RETURN_SKIP("No multicast support.");
  ASSERT(r == 0);

  r = uv_udp_recv_start(&server, alloc_cb, cl_recv_cb);
  ASSERT(r == 0);

  r = do_send(&req);
  ASSERT(r == 0);

  ASSERT(close_cb_called == 0);
  ASSERT(cl_recv_cb_called == 0);
  ASSERT(sv_send_cb_called == 0);

  /* run the loop till all events are processed */
  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  ASSERT(cl_recv_cb_called == 2);
  ASSERT(sv_send_cb_called == 2);
  ASSERT(close_cb_called == 2);

  make_valgrind_happy();
}
