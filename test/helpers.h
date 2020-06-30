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

constexpr int kTestPort = 9123;

// TODO(trevnorris): This is temporary while libuv tests are being ported. A
// more permanent solution should be made.
#define container_of(ptr, type, member)                                       \
  reinterpret_cast<type*>(reinterpret_cast<char*>(ptr) - offsetof(type, member))

#define SOCKADDR_CAST(addr) \
  reinterpret_cast<struct sockaddr*>(addr)

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

#endif  // TEST_HELPERS_H_
