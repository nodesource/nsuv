#ifndef _WIN32

#include "../include/nsuv-inl.h"
#include "./helpers.h"

#include <stdio.h>
#include <string.h>

using nsuv::ns_connect;
using nsuv::ns_tcp;
using nsuv::ns_write;

static struct sockaddr_in addr;
static ns_tcp tcp_server;
static ns_tcp tcp_outgoing[2];
static ns_tcp tcp_incoming[ARRAY_SIZE(tcp_outgoing)];
static ns_connect<ns_tcp> connect_reqs[ARRAY_SIZE(tcp_outgoing)];
static ns_tcp tcp_check;
static ns_connect<ns_tcp> tcp_check_req;
static ns_write<ns_tcp> write_reqs[ARRAY_SIZE(tcp_outgoing)];
static uint32_t got_connections;
static uint32_t close_cb_called;
static uint32_t write_cb_called;
static uint32_t read_cb_called;
static uint32_t pending_incoming;

char x_cstr[] = "x";

static void close_cb(ns_tcp*) {
  close_cb_called++;
}

static void write_cb(ns_write<ns_tcp>*, int status) {
  ASSERT(status == 0);
  write_cb_called++;
}

static void connect_cb(ns_connect<ns_tcp>* req, int status) {
  uint32_t i;
  uv_buf_t buf;

  if (req == &tcp_check_req) {
    ASSERT(status != 0);

    /*
     * Time to finish the test: close both the check and pending incoming
     * connections
     */
    (tcp_incoming[pending_incoming]).close(close_cb);
    tcp_check.close(close_cb);
    return;
  }

  ASSERT(status == 0);
  ASSERT(connect_reqs <= req);
  ASSERT(req <= connect_reqs + ARRAY_SIZE(connect_reqs));
  i = req - connect_reqs;

  buf = uv_buf_init(x_cstr, 1);
  ASSERT(0 == (tcp_outgoing[i]).write(&write_reqs[i], &buf, 1, write_cb));
}

static void alloc_cb(ns_tcp*, size_t, uv_buf_t* buf) {
  static char slab[1];
  buf->base = slab;
  buf->len = sizeof(slab);
}

static void read_cb(ns_tcp* stream, ssize_t nread, const uv_buf_t*) {
  uv_loop_t* loop;
  uint32_t i;

  pending_incoming = stream - &tcp_incoming[0];
  ASSERT(pending_incoming < got_connections);
  ASSERT(0 == stream->read_stop());
  ASSERT(1 == nread);

  loop = stream->get_loop();
  read_cb_called++;

  /* Close all active incomings, except current one */
  for (i = 0; i < got_connections; i++) {
    if (i != pending_incoming)
      (tcp_incoming[i]).close(close_cb);
  }

  /* Close server, so no one will connect to it */
  tcp_server.close(close_cb);

  /* Create new fd that should be one of the closed incomings */
  ASSERT(0 == tcp_check.init(loop));
  ASSERT(0 == tcp_check.connect(&tcp_check_req,
                                SOCKADDR_CONST_CAST(&addr),
                                connect_cb));
  ASSERT(0 == tcp_check.read_start(alloc_cb, read_cb));
}

static void connection_cb(ns_tcp* server, int) {
  uint32_t i;
  ns_tcp* incoming;

  ASSERT(server == &tcp_server);

  /* Ignore tcp_check connection */
  if (got_connections == ARRAY_SIZE(tcp_incoming))
    return;

  /* Accept everyone */
  incoming = &tcp_incoming[got_connections++];
  ASSERT(0 == incoming->init(server->get_loop()));
  ASSERT(0 == server->accept(incoming));

  if (got_connections != ARRAY_SIZE(tcp_incoming))
    return;

  /* Once all clients are accepted - start reading */
  for (i = 0; i < ARRAY_SIZE(tcp_incoming); i++) {
    incoming = &tcp_incoming[i];
    ASSERT(0 == incoming->read_start(alloc_cb, read_cb));
  }
}

TEST_CASE("tcp_close_accept", "[tcp]") {
  uint32_t i;
  uv_loop_t* loop;
  ns_tcp* client;

  /*
   * A little explanation of what goes on below:
   *
   * We'll create server and connect to it using two clients, each writing one
   * byte once connected.
   *
   * When all clients will be accepted by server - we'll start reading from them
   * and, on first client's first byte, will close second client and server.
   * After that, we'll immediately initiate new connection to server using
   * tcp_check handle (thus, reusing fd from second client).
   *
   * In this situation uv__io_poll()'s event list should still contain read
   * event for second client, and, if not cleaned up properly, `tcp_check` will
   * receive stale event of second incoming and invoke `connect_cb` with zero
   * status.
   */

  loop = uv_default_loop();
  ASSERT(0 == uv_ip4_addr("127.0.0.1", kTestPort, &addr));

  ASSERT(0 == tcp_server.init(loop));
  ASSERT(0 == tcp_server.bind(SOCKADDR_CONST_CAST(&addr), 0));
  ASSERT(0 == tcp_server.listen(ARRAY_SIZE(tcp_outgoing), connection_cb));

  for (i = 0; i < ARRAY_SIZE(tcp_outgoing); i++) {
    client = tcp_outgoing + i;

    ASSERT(0 == client->init(loop));
    ASSERT(0 == client->connect(&connect_reqs[i],
                                SOCKADDR_CONST_CAST(&addr),
                                connect_cb));
  }

  uv_run(loop, UV_RUN_DEFAULT);

  ASSERT(ARRAY_SIZE(tcp_outgoing) == got_connections);
  ASSERT((ARRAY_SIZE(tcp_outgoing) + 2) == close_cb_called);
  ASSERT(ARRAY_SIZE(tcp_outgoing) == write_cb_called);
  ASSERT(1 == read_cb_called);

  make_valgrind_happy();
}

#endif /* !_WIN32 */
