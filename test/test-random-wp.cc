#include "../include/nsuv-inl.h"
#include "./catch.hpp"
#include "./helpers.h"

#include <string.h>

using nsuv::ns_random;

static char scratch[256];
static int random_cb_called;


static void random_cb(ns_random*,
                      int status,
                      void* buf,
                      size_t buflen,
                      std::weak_ptr<size_t> d) {
  auto sp = d.lock();
  char zero[sizeof(scratch)];

  memset(zero, 0, sizeof(zero));

  ASSERT(sp);
  ASSERT_EQ(42, *sp);
  ASSERT(0 == status);
  ASSERT(buf == static_cast<void*>(scratch));

  if (random_cb_called == 0) {
    ASSERT(buflen == 0);
    ASSERT(0 == memcmp(scratch, zero, sizeof(zero)));
  } else {
    ASSERT(buflen == sizeof(scratch));
    /* Buy a lottery ticket if you manage to trip this assertion. */
    ASSERT(0 != memcmp(scratch, zero, sizeof(zero)));
  }

  random_cb_called++;
}


TEST_CASE("random_async_wp", "[random]") {
  std::shared_ptr<size_t> sp = std::make_shared<size_t>(42);
  ns_random req;
  uv_loop_t* loop;

  loop = uv_default_loop();
  ASSERT(UV_EINVAL == req.get(
      loop, scratch, sizeof(scratch), -1, random_cb, TO_WEAK(sp)));
  ASSERT(UV_E2BIG == req.get(loop, scratch, -1, -1, random_cb, TO_WEAK(sp)));

  ASSERT(0 == req.get(loop, scratch, 0, 0, random_cb, TO_WEAK(sp)));
  ASSERT(0 == random_cb_called);

  ASSERT(0 == uv_run(loop, UV_RUN_DEFAULT));
  ASSERT(1 == random_cb_called);

  ASSERT(0 == req.get(
        loop, scratch, sizeof(scratch), 0, random_cb, TO_WEAK(sp)));
  ASSERT(1 == random_cb_called);

  ASSERT(0 == uv_run(loop, UV_RUN_DEFAULT));
  ASSERT(2 == random_cb_called);

  make_valgrind_happy();
}
