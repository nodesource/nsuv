#include "../include/nsuv-inl.h"
#include "./helpers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using nsuv::ns_idle;
using nsuv::ns_udp;
using nsuv::ns_udp_send;

#define CHECK_OBJECT(handle, type, parent) \
  ASSERT(reinterpret_cast<type*>(handle) == &(parent))

static ns_udp client;
static ns_idle idle_handle;
static ns_udp_send send_req;
static uv_buf_t buf;
static struct sockaddr_in addr;
static char send_data[1024];

static int loop_hang_called;

static void send_cb(ns_udp_send* req, int status);


static void idle_cb(ns_idle* handle) {
  int r;

  ASSERT_NULL(send_req.handle());
  CHECK_OBJECT(handle, ns_idle, idle_handle);
  ASSERT(0 == handle->stop());

  /* It probably would have stalled by now if it's going to stall at all. */
  if (++loop_hang_called > 1000) {
    client.close();
    idle_handle.close();
    return;
  }

  r = client.send(&send_req, &buf, 1, SOCKADDR_CONST_CAST(&addr), send_cb);
  ASSERT(r == 0);
}


static void send_cb(ns_udp_send* req, int status) {
  ASSERT_NOT_NULL(req);
  ASSERT((status == 0 || status == UV_ENETUNREACH));
  CHECK_OBJECT(req->handle(), ns_udp, client);
  CHECK_OBJECT(req, ns_udp_send, send_req);
  req->handle(nullptr);

  ASSERT(0 == idle_handle.start(idle_cb));
}


TEST_CASE("udp_send_hang_loop", "[udp]") {
  ASSERT(0 == idle_handle.init(uv_default_loop()));

  /* 192.0.2.0/8 is "TEST-NET" and reserved for documentation.
   * Good for us, though. Since we want to have something unreachable.
   */
  ASSERT(0 == uv_ip4_addr("192.0.2.3", kTestPort, &addr));

  ASSERT(0 == client.init(uv_default_loop()));

  buf = uv_buf_init(send_data, sizeof(send_data));

  ASSERT(0 == idle_handle.start(idle_cb));

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  ASSERT(loop_hang_called > 1000);

  make_valgrind_happy();
}
