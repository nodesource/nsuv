#include "../include/nsuv-inl.h"
#include "./helpers.h"

using nsuv::ns_connect;
using nsuv::ns_tcp;
using nsuv::ns_timer;
using nsuv::ns_write;

#define REQ_COUNT 10000

static ns_timer timer;
static ns_tcp server;
static ns_tcp client;
static ns_tcp incoming;
static int connect_cb_called;
static int close_cb_called;
static int connection_cb_called;
static int write_callbacks;
static int write_cancelled_callbacks;
static int write_error_callbacks;

static ns_write<ns_tcp> write_requests[REQ_COUNT];


static void close_cb(ns_tcp*, std::weak_ptr<size_t> d) {
  auto sp = d.lock();
  ASSERT(sp);
  ASSERT_EQ(42, *sp);
  close_cb_called++;
}

static void timer_cb(ns_timer*, std::weak_ptr<size_t> d) {
  auto sp = d.lock();
  ASSERT(sp);
  ASSERT_EQ(42, *sp);
  client.close(close_cb, d);
  server.close(close_cb, d);
  incoming.close(close_cb, d);
}

static void write_cb(ns_write<ns_tcp>*, int status, std::weak_ptr<size_t> d) {
  auto sp = d.lock();
  ASSERT(sp);
  ASSERT_EQ(42, *sp);
  if (status == 0)
    write_callbacks++;
  else if (status == UV_ECANCELED)
    write_cancelled_callbacks++;
  else
    write_error_callbacks++;
}

static void connect_cb(ns_connect<ns_tcp>* req,
                       int status,
                       std::weak_ptr<size_t> d) {
  auto sp = d.lock();
  static char base[1024];
  int r;
  int i;
  uv_buf_t buf;

  ASSERT(sp);
  ASSERT_EQ(42, *sp);
  ASSERT(status == 0);
  connect_cb_called++;

  buf = uv_buf_init(base, sizeof(base));

  for (i = 0; i < REQ_COUNT; i++) {
    r = req->handle()->write(&write_requests[i], &buf, 1, write_cb, d);
    ASSERT(r == 0);
  }
}


static void connection_cb(ns_tcp* tcp, int status, std::weak_ptr<size_t> d) {
  auto sp = d.lock();

  ASSERT(sp);
  ASSERT_EQ(42, *sp);
  ASSERT(status == 0);

  ASSERT(0 == incoming.init(tcp->get_loop()));
  ASSERT(0 == tcp->accept(&incoming));

  ASSERT(0 == timer.init(uv_default_loop()));
  ASSERT(0 == timer.start(timer_cb, 1000, 0, d));

  connection_cb_called++;
}


static void start_server(std::weak_ptr<size_t> d) {
  struct sockaddr_in addr;

  ASSERT(0 == uv_ip4_addr("0.0.0.0", kTestPort, &addr));

  ASSERT(0 == server.init(uv_default_loop()));
  ASSERT(0 == server.bind(SOCKADDR_CONST_CAST(&addr), 0));
  ASSERT(0 == server.listen(128, connection_cb, d));
}


TEST_CASE("tcp_write_queue_order_wp", "[tcp]") {
  std::shared_ptr<size_t> sp = std::make_shared<size_t>(42);
  ns_connect<ns_tcp> connect_req;
  struct sockaddr_in addr;
  int buffer_size = 16 * 1024;

  start_server(TO_WEAK(sp));

  ASSERT(0 == uv_ip4_addr("127.0.0.1", kTestPort, &addr));

  ASSERT(0 == client.init(uv_default_loop()));
  ASSERT(0 == client.connect(&connect_req,
                             SOCKADDR_CONST_CAST(&addr),
                             connect_cb,
                             TO_WEAK(sp)));
  ASSERT(0 == uv_send_buffer_size(client.base_handle(), &buffer_size));

  ASSERT(0 == uv_run(uv_default_loop(), UV_RUN_DEFAULT));

  ASSERT(connect_cb_called == 1);
  ASSERT(connection_cb_called == 1);
  ASSERT(write_callbacks > 0);
  ASSERT(write_cancelled_callbacks > 0);
  ASSERT(write_callbacks +
         write_error_callbacks +
         write_cancelled_callbacks == REQ_COUNT);
  ASSERT(close_cb_called == 3);

  make_valgrind_happy();
}
