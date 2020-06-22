#include "../include/nsuv-inl.h"
#include "./catch.hpp"
#include "./helpers.h"

using nsuv::ns_addrinfo;

static const char* invalid_name = "xyzzy.xyzzy.xyzzy.";
static const char* valid_name = "localhost";

std::string* my_data = nullptr;

static void gettaddrinfo_failure_cb(ns_addrinfo* info, int status) {
  REQUIRE(0 > status);
  REQUIRE(info->info() == nullptr);
}

static void gettaddrinfo_success_cb(ns_addrinfo* info, int status) {
  REQUIRE(0 == status);
  REQUIRE(info->info());
}

static void gettaddrinfo_void_data_cb(ns_addrinfo* info,
                                      int status,
                                      std::string* data) {
  REQUIRE(0 == status);
  REQUIRE(info->info());
  REQUIRE(data == my_data);
  delete my_data;
}

TEST_CASE("get_sync", "[addrinfo]") {
  ns_addrinfo info;

  REQUIRE(0 > info.get(uv_default_loop(),
                       nullptr,
                       invalid_name,
                       nullptr,
                       nullptr));

  REQUIRE(0 == uv_run(uv_default_loop(), UV_RUN_DEFAULT));

  REQUIRE(0 == info.get(uv_default_loop(),
                        nullptr,
                        valid_name,
                        nullptr,
                        nullptr));

  REQUIRE(0 == uv_run(uv_default_loop(), UV_RUN_DEFAULT));

  make_valgrind_happy();
}

TEST_CASE("invalid_get_async", "[addrinfo]") {
  ns_addrinfo info;

  REQUIRE(0 == info.get(uv_default_loop(),
                        gettaddrinfo_failure_cb,
                        invalid_name,
                        nullptr,
                        nullptr));

  REQUIRE(0 == uv_run(uv_default_loop(), UV_RUN_DEFAULT));

  make_valgrind_happy();
}

TEST_CASE("valid_get_async", "[addrinfo]") {
  ns_addrinfo info;

  REQUIRE(0 == info.get(uv_default_loop(),
                        gettaddrinfo_success_cb,
                        valid_name,
                        nullptr,
                        nullptr));

  REQUIRE(0 == uv_run(uv_default_loop(), UV_RUN_DEFAULT));

  make_valgrind_happy();
}

TEST_CASE("valid_get_async_void_data", "[addrinfo]") {
  ns_addrinfo info;
  struct addrinfo hints;

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
  hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
  hints.ai_flags = AI_PASSIVE;    /* For wildcard IP address */
  hints.ai_protocol = 0;          /* Any protocol */
  hints.ai_canonname = nullptr;
  hints.ai_addr = nullptr;
  hints.ai_next = nullptr;

  my_data = new std::string("my_data");

  REQUIRE(0 == info.get(uv_default_loop(),
                        gettaddrinfo_void_data_cb,
                        valid_name,
                        nullptr,
                        &hints,
                        my_data));

  REQUIRE(0 == uv_run(uv_default_loop(), UV_RUN_DEFAULT));

  make_valgrind_happy();
}
