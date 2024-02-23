#include "../include/nsuv-inl.h"
#include "./catch.hpp"
#include "./helpers.h"

using nsuv::ns_timer;

static size_t once_cb_called = 0;
static size_t once_close_cb_called = 0;
static size_t repeat_cb_called = 0;
static size_t repeat_close_cb_called = 0;
static size_t order_cb_called = 0;
static uint64_t start_time;
static uint64_t timer_early_check_expected_time;
static ns_timer tiny_timer;
static ns_timer huge_timer1;
static ns_timer huge_timer2;


static void once_close_cb(ns_timer* handle, std::weak_ptr<size_t> d) {
  auto sp = d.lock();
  ASSERT(sp);
  ASSERT_EQ(42, *sp);

  INFO("ONCE_CLOSE_CB");

  ASSERT_NOT_NULL(handle);
  ASSERT_EQ(0, handle->is_active());

  once_close_cb_called++;
}


static void once_cb(ns_timer* handle, std::weak_ptr<size_t> d) {
  auto sp = d.lock();
  ASSERT(sp);
  ASSERT_EQ(42, *sp);

  INFO("ONCE_CB " << once_cb_called);

  ASSERT_NOT_NULL(handle);
  ASSERT_EQ(0, handle->is_active());

  once_cb_called++;

  handle->close(once_close_cb, d);

  /* Just call this randomly for the code coverage. */
  uv_update_time(uv_default_loop());
}


static void repeat_close_cb(ns_timer* handle, std::weak_ptr<size_t> d) {
  auto sp = d.lock();
  ASSERT(sp);
  ASSERT_EQ(42, *sp);

  INFO("REPEAT_CLOSE_CB");
  ASSERT_NOT_NULL(handle);
  repeat_close_cb_called++;
}


static void repeat_cb(ns_timer* handle, std::weak_ptr<size_t> d) {
  auto sp = d.lock();
  ASSERT(sp);
  ASSERT_EQ(42, *sp);

  INFO("REPEAT_CB");
  ASSERT_NOT_NULL(handle);
  ASSERT_EQ(1, handle->is_active());

  repeat_cb_called++;

  if (repeat_cb_called == 5) {
    handle->close(repeat_close_cb, d);
  }
}


static void never_cb(ns_timer*, std::weak_ptr<size_t>) {
  FAIL("never_cb should never be called");
}


TEST_CASE("timer_wp", "[timer]") {
  constexpr size_t kTimersSize = 10;
  std::shared_ptr<size_t> sp = std::make_shared<size_t>(42);
  ns_timer once_timers[kTimersSize];
  ns_timer* once;
  ns_timer repeat;
  ns_timer never;

  once_cb_called = 0;
  start_time = uv_now(uv_default_loop());
  ASSERT_LT(0, start_time);

  /* Let 10 timers time out in 500 ms total. */
  for (size_t i = 0; i < kTimersSize; i++) {
    once = &once_timers[i];
    ASSERT_EQ(0, once->init(uv_default_loop()));
    ASSERT_EQ(0, once->start(once_cb, i * 50, 0, TO_WEAK(sp)));
  }

  /* The 11th timer is a repeating timer that runs 4 times */
  ASSERT_EQ(0, repeat.init(uv_default_loop()));
  ASSERT_EQ(0, repeat.start(repeat_cb, 100, 100, TO_WEAK(sp)));

  /* The 12th timer should not do anything. */
  ASSERT_EQ(0, never.init(uv_default_loop()));
  ASSERT_EQ(0, never.start(never_cb, 100, 100, TO_WEAK(sp)));
  ASSERT_EQ(0, never.stop());
  never.unref();

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  ASSERT_EQ(once_cb_called, 10);
  ASSERT_EQ(once_close_cb_called, 10);
  INFO("repeat_cb_called" << repeat_cb_called);
  ASSERT_EQ(repeat_cb_called, 5);
  ASSERT_EQ(repeat_close_cb_called, 1);

  ASSERT_LE(500, uv_now(uv_default_loop()) - start_time);

  make_valgrind_happy();
}


TEST_CASE("timer_start_twice_wp", "[timer]") {
  std::shared_ptr<size_t> sp = std::make_shared<size_t>(42);
  ns_timer once;
  once_cb_called = 0;
  ASSERT_EQ(0, once.init(uv_default_loop()));
  ASSERT_EQ(0, once.start(never_cb, 86400 * 1000, 0, TO_WEAK(sp)));
  ASSERT_EQ(0, once.start(once_cb, 10, 0, TO_WEAK(sp)));
  ASSERT_EQ(0, uv_run(uv_default_loop(), UV_RUN_DEFAULT));
  ASSERT_EQ(1, once_cb_called);

  make_valgrind_happy();
}


static void order_cb_a(ns_timer*, std::weak_ptr<size_t> d) {
  auto check = d.lock();
  ASSERT(check);
  ASSERT_EQ(order_cb_called++, *check);
}


static void order_cb_b(ns_timer*, std::weak_ptr<size_t> d) {
  auto check = d.lock();
  ASSERT(check);
  ASSERT_EQ(order_cb_called++, *check);
}


TEST_CASE("timer_order_wp", "[timer]") {
  std::shared_ptr<size_t> sp1 = std::make_shared<size_t>(0);
  std::shared_ptr<size_t> sp2 = std::make_shared<size_t>(1);
  std::weak_ptr<size_t> first = sp1;
  std::weak_ptr<size_t> second = sp2;
  ns_timer handle_a;
  ns_timer handle_b;

  ASSERT_EQ(0, handle_a.init(uv_default_loop()));
  ASSERT_EQ(0, handle_b.init(uv_default_loop()));

  /* Test for starting handle_a then handle_b */
  ASSERT_EQ(0, handle_a.start(order_cb_a, 0, 0, first));
  ASSERT_EQ(0, handle_b.start(order_cb_b, 0, 0, second));
  ASSERT_EQ(0, uv_run(uv_default_loop(), UV_RUN_DEFAULT));

  ASSERT_EQ(order_cb_called, 2);

  ASSERT_EQ(0, handle_a.stop());
  ASSERT_EQ(0, handle_b.stop());

  /* Test for starting handle_b then handle_a */
  order_cb_called = 0;
  ASSERT_EQ(0, handle_b.start(order_cb_b, 0, 0, first));

  ASSERT_EQ(0, handle_a.start(order_cb_a, 0, 0, second));
  ASSERT_EQ(0, uv_run(uv_default_loop(), UV_RUN_DEFAULT));

  ASSERT_EQ(order_cb_called, 2);

  make_valgrind_happy();
}


static void tiny_timer_cb(ns_timer* handle, std::weak_ptr<size_t> d) {
  auto sp = d.lock();
  ASSERT(sp);
  ASSERT_EQ(42, *sp);
  ASSERT_EQ(handle, &tiny_timer);
  tiny_timer.close();
  huge_timer1.close();
  huge_timer2.close();
}


TEST_CASE("timer_huge_timeout_wp", "[timer]") {
  std::shared_ptr<size_t> sp = std::make_shared<size_t>(42);
  ASSERT_EQ(0, tiny_timer.init(uv_default_loop()));
  ASSERT_EQ(0, huge_timer1.init(uv_default_loop()));
  ASSERT_EQ(0, huge_timer2.init(uv_default_loop()));
  ASSERT_EQ(0, tiny_timer.start(tiny_timer_cb, 1, 0, TO_WEAK(sp)));
  ASSERT_EQ(0, huge_timer1.start(
        tiny_timer_cb, 0xffffffffffffLL, 0, TO_WEAK(sp)));
  ASSERT_EQ(0, huge_timer2.start(
        tiny_timer_cb, (uint64_t) -1, 0, TO_WEAK(sp)));
  ASSERT_EQ(0, uv_run(uv_default_loop(), UV_RUN_DEFAULT));

  make_valgrind_happy();
}


static void huge_repeat_cb(ns_timer* handle, std::weak_ptr<size_t> d) {
  static size_t ncalls = 0;
  auto sp = d.lock();

  ASSERT(sp);
  ASSERT_EQ(42, *sp);

  if (ncalls == 0)
    ASSERT_EQ(handle, &huge_timer1);
  else
    ASSERT_EQ(handle, &tiny_timer);

  if (++ncalls == 10) {
    tiny_timer.close();
    huge_timer1.close();
  }
}


TEST_CASE("timer_huge_repeat_wp", "[timer]") {
  std::shared_ptr<size_t> sp = std::make_shared<size_t>(42);
  ASSERT_EQ(0, tiny_timer.init(uv_default_loop()));
  ASSERT_EQ(0, huge_timer1.init(uv_default_loop()));
  ASSERT_EQ(0, tiny_timer.start(huge_repeat_cb, 2, 2, TO_WEAK(sp)));
  ASSERT_EQ(0, huge_timer1.start(
        huge_repeat_cb, 1, (uint64_t) -1, TO_WEAK(sp)));
  ASSERT_EQ(0, uv_run(uv_default_loop(), UV_RUN_DEFAULT));

  make_valgrind_happy();
}


static void timer_run_once_timer_cb(ns_timer*, std::weak_ptr<size_t> d) {
  auto cb_called = d.lock();
  ASSERT(cb_called);
  *cb_called += 1;
}


TEST_CASE("timer_run_once_wp", "[timer]") {
  std::shared_ptr<size_t> sp = std::make_shared<size_t>(0);
  std::weak_ptr<size_t> cb_called = sp;
  ns_timer timer_handle;

  ASSERT_EQ(0, timer_handle.init(uv_default_loop()));
  ASSERT_EQ(0, timer_handle.start(timer_run_once_timer_cb, 0, 0, cb_called));
  ASSERT_EQ(0, uv_run(uv_default_loop(), UV_RUN_ONCE));
  ASSERT_EQ(1, *sp);

  ASSERT_EQ(0, timer_handle.start(timer_run_once_timer_cb, 1, 0, cb_called));
  ASSERT_EQ(0, uv_run(uv_default_loop(), UV_RUN_ONCE));
  ASSERT_EQ(2, *sp);

  timer_handle.close();
  ASSERT_EQ(0, uv_run(uv_default_loop(), UV_RUN_ONCE));

  make_valgrind_happy();
}


static void timer_early_check_cb(ns_timer*, std::weak_ptr<size_t> d) {
  uint64_t hrtime = uv_hrtime() / 1000000;
  auto sp = d.lock();
  ASSERT(sp);
  ASSERT_EQ(42, *sp);
  ASSERT_GE(hrtime, timer_early_check_expected_time);
}


TEST_CASE("timer_early_check_wp", "[timer]") {
  const uint64_t timeout_ms = 10;
  std::shared_ptr<size_t> sp = std::make_shared<size_t>(42);
  ns_timer timer_handle;

  timer_early_check_expected_time = uv_now(uv_default_loop()) + timeout_ms;

  ASSERT_EQ(0, timer_handle.init(uv_default_loop()));
  ASSERT_EQ(0, timer_handle.start(timer_early_check_cb,
                                  timeout_ms,
                                  0,
                                  TO_WEAK(sp)));
  ASSERT_EQ(0, uv_run(uv_default_loop(), UV_RUN_DEFAULT));

  timer_handle.close();
  ASSERT_EQ(0, uv_run(uv_default_loop(), UV_RUN_DEFAULT));

  make_valgrind_happy();
}
