#include "../include/nsuv-inl.h"
#include "./catch.hpp"
#include "./helpers.h"

using nsuv::ns_addrinfo;

static const char* invalid_name = "xyzzy.xyzzy.xyzzy.";
static const char* valid_name = "localhost";

static std::string* my_data_ptr = nullptr;

static void gettaddrinfo_failure_cb(ns_addrinfo* info,
                                    int status,
                                    std::weak_ptr<size_t> d) {
  auto sp = d.lock();
  ASSERT(sp);
  ASSERT_EQ(42, *sp);
  ASSERT_GT(0, status);
  ASSERT_NULL(info->info());
}

TEST_CASE("invalid_get_async_wp", "[addrinfo]") {
  std::shared_ptr<size_t> sp = std::make_shared<size_t>(42);
  ns_addrinfo info;

  ASSERT_EQ(0, info.get(uv_default_loop(),
                        gettaddrinfo_failure_cb,
                        invalid_name,
                        nullptr,
                        nullptr,
                        TO_WEAK(sp)));

  ASSERT_EQ(0, uv_run(uv_default_loop(), UV_RUN_DEFAULT));

  make_valgrind_happy();
}

static void gettaddrinfo_success_cb(ns_addrinfo* info,
                                    int status,
                                    std::weak_ptr<size_t> d) {
  auto sp = d.lock();
  ASSERT(sp);
  ASSERT_EQ(42, *sp);
  ASSERT_EQ(0, status);
  ASSERT(info->info());
}

TEST_CASE("valid_get_async_wp", "[addrinfo]") {
  std::shared_ptr<size_t> sp = std::make_shared<size_t>(42);
  ns_addrinfo info;

  ASSERT_EQ(0, info.get(uv_default_loop(),
                        gettaddrinfo_success_cb,
                        valid_name,
                        nullptr,
                        nullptr,
                        TO_WEAK(sp)));

  ASSERT_EQ(0, uv_run(uv_default_loop(), UV_RUN_DEFAULT));

  make_valgrind_happy();
}

static void gettaddrinfo_void_data_cb(ns_addrinfo* info,
                                      int status,
                                      std::weak_ptr<std::string> d) {
  auto data = d.lock();
  ASSERT_EQ(0, status);
  ASSERT(info->info());
  ASSERT(data);
  ASSERT_EQ(data.get(), my_data_ptr);
}

TEST_CASE("valid_get_async_void_data_wp", "[addrinfo]") {
  auto my_data = std::make_shared<std::string>("my_data");
  std::weak_ptr<std::string> wp = my_data;
  ns_addrinfo info;
  struct addrinfo hints;

  my_data_ptr = my_data.get();

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
  hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
  hints.ai_flags = AI_PASSIVE;    /* For wildcard IP address */
  hints.ai_protocol = 0;          /* Any protocol */
  hints.ai_canonname = nullptr;
  hints.ai_addr = nullptr;
  hints.ai_next = nullptr;

  ASSERT_EQ(0, info.get(uv_default_loop(),
                        gettaddrinfo_void_data_cb,
                        valid_name,
                        nullptr,
                        &hints,
                        wp));

  ASSERT_EQ(0, uv_run(uv_default_loop(), UV_RUN_DEFAULT));

  my_data_ptr = nullptr;
  make_valgrind_happy();
}
