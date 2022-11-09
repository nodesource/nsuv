#include "../include/nsuv-inl.h"
#include "./helpers.h"

using nsuv::ns_connect;
using nsuv::ns_tcp;
using nsuv::ns_timer;

static int connect_cb_called;
static int close_cb_called;

static ns_connect<ns_tcp> connect_req;
static ns_timer timer;
static ns_tcp conn;

static void connect_cb(ns_connect<ns_tcp>* req, int status);
static void timer_cb(ns_timer* handle);
static void close_cb(ns_tcp* handle);
static void close_cb(ns_timer* handle);


static void zero_global_values() {
  connect_cb_called = 0;
  close_cb_called = 0;
}


static void connect_cb(ns_connect<ns_tcp>* req, int status) {
  ASSERT(req == &connect_req);
  ASSERT(status == UV_ECANCELED);
  connect_cb_called++;
}


static void timer_cb(ns_timer* handle) {
  ASSERT(handle == &timer);
  conn.close(close_cb);
  timer.close(close_cb);
}


static void close_cb(ns_tcp* handle) {
  ASSERT(handle == &conn);
  close_cb_called++;
}


static void close_cb(ns_timer* handle) {
  ASSERT(handle == &timer);
  close_cb_called++;
}


/* Verify that connecting to an unreachable address or port doesn't hang
 * the event loop.
 */
TEST_CASE("tcp_connect_timeout", "[tcp]") {
  struct sockaddr_in addr;
  int r;

  zero_global_values();

  ASSERT(0 == uv_ip4_addr("8.8.8.8", 9999, &addr));

  r = timer.init(uv_default_loop());
  ASSERT(r == 0);

  r = timer.start(timer_cb, 50, 0);
  ASSERT(r == 0);

  r = conn.init(uv_default_loop());
  ASSERT(r == 0);

  r = conn.connect(&connect_req, SOCKADDR_CONST_CAST(&addr), connect_cb);
  if (r == UV_ENETUNREACH)
    RETURN_SKIP("Network unreachable.");
  ASSERT(r == 0);

  r = uv_run(uv_default_loop(), UV_RUN_DEFAULT);
  ASSERT(r == 0);

  make_valgrind_happy();
}

/* Make sure connect fails instantly if the target is nonexisting
 * local port.
 */

static void connect_local_cb(ns_connect<ns_tcp>* req, int status) {
  ASSERT_PTR_EQ(req, &connect_req);
  ASSERT_NE(status, UV_ECANCELED);
  connect_cb_called++;
}

static int is_supported_system(void) {
  int semver[3];
  int min_semver[3] = {10, 0, 16299};
  int cnt;
  uv_utsname_t uname;
  ASSERT_EQ(uv_os_uname(&uname), 0);
  if (strcmp(uname.sysname, "Windows_NT") == 0) {
    cnt = sscanf(uname.release, "%d.%d.%d", &semver[0], &semver[1], &semver[2]);
    if (cnt != 3) {
      return 0;
    }
    /* release >= 10.0.16299 */
    for (cnt = 0; cnt < 3; ++cnt) {
      if (semver[cnt] > min_semver[cnt])
        return 1;
      if (semver[cnt] < min_semver[cnt])
        return 0;
    }
    return 1;
  }
  return 1;
}

TEST_CASE("tcp_local_connect_timeout", "[tcp]") {
  struct sockaddr_in addr;
  int r;

  zero_global_values();

  if (!is_supported_system()) {
    RETURN_SKIP("Unsupported system");
  }
  ASSERT_EQ(0, uv_ip4_addr("127.0.0.1", kTestPort, &addr));

  r = timer.init(uv_default_loop());
  ASSERT_EQ(r, 0);

  /* Give it 1s to timeout. */
  r = timer.start(timer_cb, 1000, 0);
  ASSERT_EQ(r, 0);

  r = conn.init(uv_default_loop());
  ASSERT_EQ(r, 0);

  r = conn.connect(&connect_req, SOCKADDR_CONST_CAST(&addr), connect_local_cb);
  if (r == UV_ENETUNREACH)
    RETURN_SKIP("Network unreachable.");
  ASSERT_EQ(r, 0);

  r = uv_run(uv_default_loop(), UV_RUN_DEFAULT);
  ASSERT(r == 0);

  make_valgrind_happy();
}

TEST_CASE("tcp6_local_connect_timeout", "[tcp]") {
  struct sockaddr_in6 addr;
  int r;

  zero_global_values();

  if (!is_supported_system()) {
    RETURN_SKIP("Unsupported system");
  }
  if (!can_ipv6()) {
    RETURN_SKIP("IPv6 not supported");
  }

  ASSERT_EQ(0, uv_ip6_addr("::1", 9999, &addr));

  r = timer.init(uv_default_loop());
  ASSERT_EQ(r, 0);

  /* Give it 1s to timeout. */
  r = timer.start(timer_cb, 1000, 0);
  ASSERT_EQ(r, 0);

  r = conn.init(uv_default_loop());
  ASSERT_EQ(r, 0);

  r = conn.connect(&connect_req, SOCKADDR_CONST_CAST(&addr), connect_local_cb);
  if (r == UV_ENETUNREACH)
    RETURN_SKIP("Network unreachable.");
  ASSERT_EQ(r, 0);

  r = uv_run(uv_default_loop(), UV_RUN_DEFAULT);
  ASSERT_EQ(r, 0);

  make_valgrind_happy();
}
