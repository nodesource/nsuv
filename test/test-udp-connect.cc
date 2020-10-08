#include "../include/nsuv-inl.h"
#include "./catch.hpp"
#include "./helpers.h"

using nsuv::ns_udp;
using nsuv::ns_udp_send;

ns_udp client;
ns_udp server;
static int cl_send_cb_called;
static const char* exit_str = "EXIT";

static void cl_send_cb(ns_udp_send* req, int status) {
  REQUIRE(req != nullptr);
  REQUIRE(status == 0);
  REQUIRE(req->handle() == &client);
  ++cl_send_cb_called;
  client.close(nullptr);
  server.close(nullptr);
}

TEST_CASE("udp_connect", "[udp]") {
#if defined(__PASE__)
  RETURN_SKIP(
      "IBMi PASE's UDP connection can not be disconnected with AF_UNSPEC.");
#endif
  ns_udp_send req;
  uv_buf_t buf;
  struct sockaddr_in ext_addr;
  struct sockaddr_in lo_addr;

  REQUIRE(0 == server.init(uv_default_loop()));

  REQUIRE(0 == client.init(uv_default_loop()));

  buf = uv_buf_init(const_cast<char*>(exit_str), 4);

  REQUIRE(0 == uv_ip4_addr("8.8.8.8", kTestPort, &ext_addr));
  REQUIRE(0 == uv_ip4_addr("127.0.0.1", kTestPort, &lo_addr));

  REQUIRE(nullptr == server.local_addr());
  REQUIRE(nullptr == server.remote_addr());
  REQUIRE(0 == server.bind(SOCKADDR_CAST(&lo_addr), 0));
  REQUIRE(0 == memcmp(&lo_addr, server.local_addr(), sizeof(lo_addr)));
  REQUIRE(nullptr == server.remote_addr());

  REQUIRE(nullptr == client.local_addr());
  REQUIRE(nullptr == client.remote_addr());
  REQUIRE(0 == client.connect(SOCKADDR_CAST(&lo_addr)));
  REQUIRE(UV_EISCONN == client.connect(SOCKADDR_CAST(&ext_addr)));
  REQUIRE(nullptr != client.local_addr());
  REQUIRE(0 == memcmp(&lo_addr, client.remote_addr(), sizeof(lo_addr)));

  /* To send messages in connected UDP sockets addr must be NULL */
  REQUIRE(UV_EISCONN == client.try_send(&buf, 1, SOCKADDR_CAST(&lo_addr)));
  REQUIRE(4 == client.try_send(&buf, 1, nullptr));
  REQUIRE(UV_EISCONN == client.try_send(&buf, 1, SOCKADDR_CAST(&ext_addr)));

  REQUIRE(0 == client.connect(nullptr));
  REQUIRE(UV_ENOTCONN == client.connect(nullptr));
  REQUIRE(nullptr != client.local_addr());
  REQUIRE(nullptr == client.remote_addr());

  /* To send messages in disconnected UDP sockets addr must be set */
  REQUIRE(4 == client.try_send(&buf, 1, SOCKADDR_CAST(&lo_addr)));
  REQUIRE(UV_EDESTADDRREQ == client.try_send(&buf, 1, nullptr));

  REQUIRE(0 == client.connect(SOCKADDR_CAST(&lo_addr)));
  REQUIRE(nullptr != client.local_addr());
  REQUIRE(0 == memcmp(&lo_addr, client.remote_addr(), sizeof(lo_addr)));

  REQUIRE(UV_EISCONN ==
          client.send(&req, &buf, 1, SOCKADDR_CAST(&lo_addr), cl_send_cb));
  REQUIRE(0 == client.send(&req, &buf, 1, nullptr, cl_send_cb));

  REQUIRE(0 == uv_run(uv_default_loop(), UV_RUN_DEFAULT));

  REQUIRE(cl_send_cb_called == 1);

  REQUIRE(client.uv_handle()->send_queue_size == 0);

  make_valgrind_happy();
}
