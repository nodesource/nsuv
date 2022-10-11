#ifndef TEST_HELPERS_H_
#define TEST_HELPERS_H_

#if defined(__clang__) ||                                                     \
    defined(__GNUC__) ||                                                      \
    defined(__INTEL_COMPILER)
# define UNUSED __attribute__((unused))
#else
# define UNUSED
#endif

#if !defined(_WIN32)
# include <sys/time.h>
# include <sys/resource.h>  /* setrlimit() */
#endif

#include <uv.h>
#include "./catch.hpp"

constexpr int kTestPort = 9123;
constexpr int kTestPort2 = 9124;
constexpr int kTestPort3 = 9125;

#ifdef _WIN32
constexpr char kTestPipename[] = "\\\\?\\pipe\\uv-test";
constexpr char kTestPipename2[] = "\\\\?\\pipe\\uv-test2";
constexpr char kTestPipename3[] = "\\\\?\\pipe\\uv-test3";
#else
constexpr char kTestPipename[] = "/tmp/uv-test-sock";
constexpr char kTestPipename2[] = "/tmp/uv-test-sock2";
constexpr char kTestPipename3[] = "/tmp/uv-test-sock3";
#endif

// TODO(trevnorris): This is temporary while libuv tests are being ported. A
// more permanent solution should be made.
#define container_of(ptr, type, member)                                       \
  reinterpret_cast<type*>(reinterpret_cast<char*>(ptr) - offsetof(type, member))

#define SOCKADDR_CAST(addr) \
  reinterpret_cast<struct sockaddr*>(addr)

#define SOCKADDR_CONST_CAST(addr) \
  reinterpret_cast<const struct sockaddr*>(addr)

// Wrap ASSERT macros to use REQUIRE, etc.
#define ASSERT(expr)                                       \
  do {                                                     \
    REQUIRE(expr);                                         \
  } while (0)

#define ASSERT_BASE(a, operator, b, type, conv)               \
  do {                                                        \
    volatile type eval_a = (type) (a);                        \
    volatile type eval_b = (type) (b);                        \
    REQUIRE(eval_a operator eval_b);                          \
  } while (0)

#define ASSERT_BASE_STR(expr, a, operator, b, type, conv)       \
  do {                                                          \
    REQUIRE(expr);                                              \
  } while (0)

#define ASSERT_BASE_LEN(expr, a, operator, b, conv, len)      \
  do {                                                        \
    REQUIRE(expr);                                            \
  } while (0)

#define ASSERT_BASE_HEX(expr, a, operator, b, size)             \
  do {                                                          \
    REQUIRE(expr);                                              \
  } while (0)

#define ASSERT_EQ(a, b) ASSERT_BASE(a, ==, b, int64_t, PRId64)
#define ASSERT_GE(a, b) ASSERT_BASE(a, >=, b, int64_t, PRId64)
#define ASSERT_GT(a, b) ASSERT_BASE(a, >, b, int64_t, PRId64)
#define ASSERT_LE(a, b) ASSERT_BASE(a, <=, b, int64_t, PRId64)
#define ASSERT_LT(a, b) ASSERT_BASE(a, <, b, int64_t, PRId64)
#define ASSERT_NE(a, b) ASSERT_BASE(a, !=, b, int64_t, PRId64)

#define ASSERT_UINT64_EQ(a, b) ASSERT_BASE(a, ==, b, uint64_t, PRIu64)
#define ASSERT_UINT64_GE(a, b) ASSERT_BASE(a, >=, b, uint64_t, PRIu64)
#define ASSERT_UINT64_GT(a, b) ASSERT_BASE(a, >, b, uint64_t, PRIu64)
#define ASSERT_UINT64_LE(a, b) ASSERT_BASE(a, <=, b, uint64_t, PRIu64)
#define ASSERT_UINT64_LT(a, b) ASSERT_BASE(a, <, b, uint64_t, PRIu64)
#define ASSERT_UINT64_NE(a, b) ASSERT_BASE(a, !=, b, uint64_t, PRIu64)

#define ASSERT_DOUBLE_EQ(a, b) ASSERT_BASE(a, ==, b, double, "f")
#define ASSERT_DOUBLE_GE(a, b) ASSERT_BASE(a, >=, b, double, "f")
#define ASSERT_DOUBLE_GT(a, b) ASSERT_BASE(a, >, b, double, "f")
#define ASSERT_DOUBLE_LE(a, b) ASSERT_BASE(a, <=, b, double, "f")
#define ASSERT_DOUBLE_LT(a, b) ASSERT_BASE(a, <, b, double, "f")
#define ASSERT_DOUBLE_NE(a, b) ASSERT_BASE(a, !=, b, double, "f")

#define ASSERT_STR_EQ(a, b) \
  ASSERT_BASE_STR(strcmp(a, b) == 0, a, == , b, char*, "s")

#define ASSERT_STR_NE(a, b) \
  ASSERT_BASE_STR(strcmp(a, b) != 0, a, !=, b, char*, "s")

#define ASSERT_MEM_EQ(a, b, size) \
  ASSERT_BASE_LEN(memcmp(a, b, size) == 0, a, ==, b, s, size)

#define ASSERT_MEM_NE(a, b, size) \
  ASSERT_BASE_LEN(memcmp(a, b, size) != 0, a, !=, b, s, size)

#define ASSERT_MEM_HEX_EQ(a, b, size) \
  ASSERT_BASE_HEX(memcmp(a, b, size) == 0, a, ==, b, size)

#define ASSERT_MEM_HEX_NE(a, b, size) \
  ASSERT_BASE_HEX(memcmp(a, b, size) != 0, a, !=, b, size)

#define ASSERT_NULL(a) \
  ASSERT_BASE(a, ==, nullptr, void*, "p")

#define ASSERT_NOT_NULL(a) \
  ASSERT_BASE(a, !=, nullptr, void*, "p")

#define ASSERT_PTR_EQ(a, b) \
  ASSERT_BASE(a, ==, b, void*, "p")

#define ASSERT_PTR_NE(a, b) \
  ASSERT_BASE(a, !=, b, void*, "p")

#define RETURN_OK()                                                           \
  do {                                                                        \
    return;                                                                   \
  } while (0)

#define RETURN_SKIP(explanation)                                              \
  do {                                                                        \
    fprintf(stderr, "%s\n", explanation);                                     \
    fflush(stderr);                                                           \
    return;                                                                   \
  } while (0)

static void close_walk_cb(uv_handle_t* handle, void*) {
  if (!uv_is_closing(handle))
    uv_close(handle, nullptr);
}

static void close_loop(uv_loop_t* loop) {
  uv_walk(loop, close_walk_cb, nullptr);
  uv_run(loop, UV_RUN_DEFAULT);
}

UNUSED static void make_valgrind_happy() {
  close_loop(uv_default_loop());
  REQUIRE(0 == uv_loop_close(uv_default_loop()));
}

UNUSED static int can_ipv6(void) {
  uv_interface_address_t* addr;
  int supported;
  int count;
  int i;

  if (uv_interface_addresses(&addr, &count))
    return 0;  /* Assume no IPv6 support on failure. */

  supported = 0;
  for (i = 0; supported == 0 && i < count; i += 1)
    supported = (AF_INET6 == addr[i].address.address6.sin6_family);

  uv_free_interface_addresses(addr, count);
  return supported;
}

#if defined(__CYGWIN__) || defined(__MSYS__) || defined(__PASE__)
# define NO_FS_EVENTS "Filesystem watching not supported on this platform."
#endif

#if defined(__MSYS__)
# define NO_SEND_HANDLE_ON_PIPE \
  "MSYS2 runtime does not support sending handles on pipes."
#elif defined(__CYGWIN__)
# define NO_SEND_HANDLE_ON_PIPE \
  "Cygwin runtime does not support sending handles on pipes."
#endif

#if defined(__MSYS__)
# define NO_SELF_CONNECT \
  "MSYS2 runtime hangs on listen+connect in same process."
#elif defined(__CYGWIN__)
# define NO_SELF_CONNECT \
  "Cygwin runtime hangs on listen+connect in same process."
#endif

#endif  // TEST_HELPERS_H_
