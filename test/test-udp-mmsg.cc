#include "../include/nsuv-inl.h"
#include "./helpers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using nsuv::ns_udp;
using nsuv::ns_udp_send;

#define CHECK_HANDLE(handle) \
  ASSERT((reinterpret_cast<ns_udp*>(handle) == &recver || \
        reinterpret_cast<ns_udp*>(handle) == &sender))

#define BUFFER_MULTIPLIER 4
#define MAX_DGRAM_SIZE (64 * 1024)
#define NUM_SENDS 8
#define EXPECTED_MMSG_ALLOCS (NUM_SENDS / BUFFER_MULTIPLIER)

static ns_udp recver;
static ns_udp sender;
static int recv_cb_called;
static int close_cb_called;
static int alloc_cb_called;

static char ping_str[] = "PING";


static void alloc_cb(uv_handle_t* arg,
                     size_t,
                     uv_buf_t* buf) {
  ns_udp* handle = ns_udp::cast(arg);

  size_t buffer_size;
  CHECK_HANDLE(handle);

  /* Only alloc enough room for multiple dgrams if we can actually recv them */
  buffer_size = MAX_DGRAM_SIZE;
  if (uv_udp_using_recvmmsg(handle))
    buffer_size *= BUFFER_MULTIPLIER;

  /* Actually malloc to exercise free'ing the buffer later */
  buf->base = new char[buffer_size];
  ASSERT_NOT_NULL(buf->base);
  buf->len = buffer_size;
  alloc_cb_called++;
}


static void close_cb(ns_udp* handle) {
  CHECK_HANDLE(handle);
  ASSERT(handle->is_closing());
  close_cb_called++;
}


static void recv_cb(uv_udp_t* arg,
                    ssize_t nread,
                    const uv_buf_t* rcvbuf,
                    const struct sockaddr* addr,
                    unsigned flags) {
  ns_udp* handle = ns_udp::cast(arg);

  ASSERT_GE(nread, 0);

  /* free and return if this is a mmsg free-only callback invocation */
  if (flags & UV_UDP_MMSG_FREE) {
    ASSERT_EQ(nread, 0);
    ASSERT_NULL(addr);
    delete[] rcvbuf->base;
    return;
  }

  ASSERT_EQ(nread, 4);
  ASSERT_NOT_NULL(addr);
  ASSERT_MEM_EQ(ping_str, rcvbuf->base, nread);

  recv_cb_called++;
  if (recv_cb_called == NUM_SENDS) {
    handle->close(close_cb);
    sender.close(close_cb);
  }

  /* Don't free if the buffer could be reused via mmsg */
  if (rcvbuf && !(flags & UV_UDP_MMSG_CHUNK))
    delete[] rcvbuf->base;
}


TEST_CASE("udp_mmsg", "[udp]") {
  struct sockaddr_in addr;
  uv_buf_t buf;
  int i;

  ASSERT_EQ(0, uv_ip4_addr("0.0.0.0", kTestPort, &addr));

  ASSERT_EQ(0, recver.init_ex(uv_default_loop(),
                              AF_UNSPEC | UV_UDP_RECVMMSG));

  ASSERT_EQ(0, recver.bind(SOCKADDR_CONST_CAST(&addr), 0));

  ASSERT_EQ(0, uv_udp_recv_start(&recver, alloc_cb, recv_cb));

  ASSERT_EQ(0, uv_ip4_addr("127.0.0.1", kTestPort, &addr));

  ASSERT_EQ(0, sender.init(uv_default_loop()));

  buf = uv_buf_init(ping_str, 4);
  for (i = 0; i < NUM_SENDS; i++) {
    ASSERT_EQ(4, sender.try_send(&buf, 1, SOCKADDR_CONST_CAST(&addr)));
  }

  ASSERT_EQ(0, uv_run(uv_default_loop(), UV_RUN_DEFAULT));

  ASSERT_EQ(close_cb_called, 2);
  ASSERT_EQ(recv_cb_called, NUM_SENDS);

  ASSERT_EQ(sender.uv_handle()->send_queue_size, 0);
  ASSERT_EQ(recver.uv_handle()->send_queue_size, 0);

  /* On platforms that don't support mmsg, each recv gets its own alloc */
  if (uv_udp_using_recvmmsg(&recver))
    ASSERT_EQ(alloc_cb_called, EXPECTED_MMSG_ALLOCS);
  else
    ASSERT_EQ(alloc_cb_called, recv_cb_called);

  make_valgrind_happy();
}
