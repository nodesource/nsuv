#include "../include/nsuv-inl.h"
#include "./helpers.h"

using nsuv::ns_connect;
using nsuv::ns_tcp;
using nsuv::ns_timer;

static ns_timer timer1_handle;
static ns_timer timer2_handle;
static ns_tcp tcp_handle;

static int connect_cb_called;
static int timer1_cb_called;
static int close_cb_called;
static int netunreach_errors;


static void close_cb(ns_timer*) {
  close_cb_called++;
}


static void close_cb(ns_tcp*) {
  close_cb_called++;
}


static void connect_cb(ns_connect<ns_tcp>*, int status) {
  /* The expected error is UV_ECANCELED but the test tries to connect to what
   * is basically an arbitrary address in the expectation that no network path
   * exists, so UV_ENETUNREACH is an equally plausible outcome.
   */
  ASSERT((status == UV_ECANCELED || status == UV_ENETUNREACH));
  uv_timer_stop(&timer2_handle);
  connect_cb_called++;
  if (status == UV_ENETUNREACH)
    netunreach_errors++;
}


static void timer1_cb(ns_timer* handle) {
  handle->close(close_cb);
  tcp_handle.close(close_cb);
  timer1_cb_called++;
}


static void timer2_cb(ns_timer*) {
  FAIL("should not be called");
}


TEST_CASE("tcp_close_while_connecting", "[tcp]") {
  ns_connect<ns_tcp> connect_req;
  struct sockaddr_in addr;
  uv_loop_t* loop;
  int r;

  loop = uv_default_loop();
  ASSERT(0 == uv_ip4_addr("1.2.3.4", kTestPort, &addr));
  ASSERT(0 == tcp_handle.init(loop));
  r = tcp_handle.connect(&connect_req, SOCKADDR_CONST_CAST(&addr), connect_cb);
  if (r == UV_ENETUNREACH)
    RETURN_SKIP("Network unreachable.");
  ASSERT(r == 0);
  ASSERT(0 == timer1_handle.init(loop));
  ASSERT(0 == timer1_handle.start(timer1_cb, 1, 0));
  ASSERT(0 == timer2_handle.init(loop));
  ASSERT(0 == timer2_handle.start(timer2_cb, 86400 * 1000, 0));
  ASSERT(0 == uv_run(loop, UV_RUN_DEFAULT));

  ASSERT(connect_cb_called == 1);
  ASSERT(timer1_cb_called == 1);
  ASSERT(close_cb_called == 2);

  make_valgrind_happy();

  if (netunreach_errors > 0)
    RETURN_SKIP("Network unreachable.");
}
