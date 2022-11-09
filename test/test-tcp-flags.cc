#include "../include/nsuv-inl.h"
#include "./helpers.h"


TEST_CASE("tcp_flags", "[tcp]") {
  uv_loop_t* loop;
  nsuv::ns_tcp handle;
  int r;

  loop = uv_default_loop();

  r = handle.init(loop);
  ASSERT(r == 0);

  r = handle.nodelay(true);
  ASSERT(r == 0);

  r = handle.keepalive(true, 60);
  ASSERT(r == 0);

  handle.close();

  r = uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(r == 0);

  make_valgrind_happy();
}
