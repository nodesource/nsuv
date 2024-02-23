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


static void close_cb(ns_udp* handle, std::weak_ptr<size_t> d) {
  auto sp = d.lock();
  ASSERT(sp);
  ASSERT_EQ(42, *sp);
  CHECK_HANDLE(handle);
  close_cb_called++;
}


static void sv_send_cb(ns_udp_send* req, int status, std::weak_ptr<size_t> d) {
  auto sp = d.lock();
  ASSERT(sp);
  ASSERT_EQ(42, *sp);
  ASSERT_NOT_NULL(req);
  ASSERT((status == 0 || status == UV_ENETUNREACH || status == UV_EPERM));
  CHECK_HANDLE(req->handle());

  sv_send_cb_called++;

  req->handle()->close(close_cb, d);
}


TEST_CASE("udp_multicast_interface_wp", "[udp]") {
/* TODO(gengjiawen): Fix test on QEMU. */
#if defined(__QEMU__)
  RETURN_SKIP("Test does not currently work in QEMU");
#endif

  std::shared_ptr<size_t> sp = std::make_shared<size_t>(42);
  int r;
  ns_udp_send req;
  uv_buf_t buf;
  struct sockaddr_in addr;
  struct sockaddr_in baddr;

  ASSERT(0 == uv_ip4_addr("239.255.0.1", kTestPort, &addr));

  r = server.init(uv_default_loop());
  ASSERT(r == 0);

  ASSERT(0 == uv_ip4_addr("0.0.0.0", 0, &baddr));
  r = server.bind(SOCKADDR_CONST_CAST(&baddr), 0);
  ASSERT(r == 0);

  r = uv_udp_set_multicast_interface(&server, "0.0.0.0");
  ASSERT(r == 0);

  /* server sends "PING" */
  buf = uv_buf_init(ping_str, 4);
  r = server.send(
      &req, &buf, 1, SOCKADDR_CONST_CAST(&addr), sv_send_cb, TO_WEAK(sp));
  ASSERT(r == 0);

  ASSERT(close_cb_called == 0);
  ASSERT(sv_send_cb_called == 0);

  /* run the loop till all events are processed */
  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  ASSERT(sv_send_cb_called == 1);
  ASSERT(close_cb_called == 1);

  ASSERT(client.uv_handle()->send_queue_size == 0);
  ASSERT(server.uv_handle()->send_queue_size == 0);

  make_valgrind_happy();
}
