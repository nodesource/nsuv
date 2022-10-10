#include "../include/nsuv-inl.h"
#include "./catch.hpp"
#include "./helpers.h"

using nsuv::ns_udp;

TEST_CASE("udp_bind", "[udp]") {
  struct sockaddr_in addr;
  uv_loop_t* loop;
  ns_udp h1, h2;

  ASSERT_EQ(0, uv_ip4_addr("0.0.0.0", kTestPort, &addr));

  loop = uv_default_loop();

  ASSERT_EQ(0, h1.init(loop));
  ASSERT_EQ(0, h2.init(loop));
  ASSERT_EQ(0, h1.bind(SOCKADDR_CAST(&addr), 0));
  ASSERT_EQ(UV_EADDRINUSE, h2.bind(SOCKADDR_CAST(&addr), 0));

  h1.close();
  h2.close();

  ASSERT_EQ(0, uv_run(loop, UV_RUN_DEFAULT));

  make_valgrind_happy();
}


TEST_CASE("udp_bind_reuseaddr", "[udp]") {
  struct sockaddr_in addr;
  uv_loop_t* loop;
  ns_udp h1, h2;

  ASSERT_EQ(0, uv_ip4_addr("0.0.0.0", kTestPort, &addr));

  loop = uv_default_loop();

  ASSERT_EQ(0, h1.init(loop));
  ASSERT_EQ(0, h2.init(loop));
  ASSERT_EQ(0, h1.bind(SOCKADDR_CAST(&addr), UV_UDP_REUSEADDR));
  ASSERT_EQ(0, h2.bind(SOCKADDR_CAST(&addr), UV_UDP_REUSEADDR));

  h1.close();
  h2.close();

  ASSERT_EQ(0, uv_run(loop, UV_RUN_DEFAULT));

  make_valgrind_happy();
}
