#include "../include/nsuv-inl.h"
#include "./catch.hpp"
#include "./helpers.h"

#include <string.h>

using nsuv::ns_random;

static char scratch[256];
static int random_cb_called;


static void random_cb(ns_random*, int status, void* buf, size_t buflen) {
  char zero[sizeof(scratch)];

  memset(zero, 0, sizeof(zero));

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


TEST_CASE("random_async", "[random]") {
  ns_random req;
  uv_loop_t* loop;

  loop = uv_default_loop();
  ASSERT(UV_EINVAL == req.get(
      loop, scratch, sizeof(scratch), -1, random_cb));
  ASSERT(UV_E2BIG == req.get(loop, scratch, -1, -1, random_cb));

  ASSERT(0 == req.get(loop, scratch, 0, 0, random_cb));
  ASSERT(0 == random_cb_called);

  ASSERT(0 == uv_run(loop, UV_RUN_DEFAULT));
  ASSERT(1 == random_cb_called);

  ASSERT(0 == req.get(loop, scratch, sizeof(scratch), 0, random_cb));
  ASSERT(1 == random_cb_called);

  ASSERT(0 == uv_run(loop, UV_RUN_DEFAULT));
  ASSERT(2 == random_cb_called);

  make_valgrind_happy();
}


TEST_CASE("random_sync", "[random]") {
  char zero[256];
  char buf[256];

  memset(buf, 0, sizeof(buf));
  ASSERT(0 == ns_random::get(buf, sizeof(buf), 0));

  /* Buy a lottery ticket if you manage to trip this assertion. */
  memset(zero, 0, sizeof(zero));
  ASSERT(0 != memcmp(buf, zero, sizeof(zero)));

  make_valgrind_happy();
}
