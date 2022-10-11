#include "../include/nsuv-inl.h"
#include "./helpers.h"

#include <stdio.h>
#include <stdlib.h>

using nsuv::ns_udp;
using nsuv::ns_udp_send;

#define DATAGRAMS 6

static ns_udp client;
static ns_udp_send req[DATAGRAMS];

static int send_cb_called;
static int close_cb_called;

static char test_str[] = "TEST";


static void close_cb(ns_udp* handle) {
  ASSERT_PTR_EQ(handle, &client);
  ASSERT(handle->is_closing());
  close_cb_called++;
}


static void send_cb(ns_udp_send* req, int status) {
  if (status != 0)
    ASSERT_EQ(status, UV_ECONNREFUSED);

  if (++send_cb_called == DATAGRAMS)
    req->handle()->close(close_cb);
}


TEST_CASE("udp_sendmmsg_error", "[udp]") {
  struct sockaddr_in addr;
  uv_buf_t buf;
  int i;

  ASSERT_EQ(0, client.init(uv_default_loop()));
  ASSERT_EQ(0, uv_ip4_addr("127.0.0.1", kTestPort, &addr));
  ASSERT_EQ(0, client.connect(SOCKADDR_CONST_CAST(&addr)));

  buf = uv_buf_init(test_str, 4);
  for (i = 0; i < DATAGRAMS; ++i)
    ASSERT_EQ(0, client.send(&req[i], &buf, 1, nullptr, send_cb));

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  ASSERT_EQ(1, close_cb_called);
  ASSERT_EQ(DATAGRAMS, send_cb_called);

  ASSERT_EQ(0, client.uv_handle()->send_queue_size);

  make_valgrind_happy();
}
