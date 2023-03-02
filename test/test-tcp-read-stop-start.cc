#include "../include/nsuv-inl.h"
#include "./helpers.h"

using nsuv::ns_connect;
using nsuv::ns_tcp;

static ns_tcp server;
static ns_tcp connection;
static int read_cb_called = 0;

static ns_tcp client;
static ns_connect<ns_tcp> connect_req;


static void on_read2(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf);

static void on_write_close_immediately(uv_write_t* req, int status) {
  ASSERT(0 == status);

  ns_tcp::cast(req->handle)->close();
  delete req;
}

static void on_write(uv_write_t* req, int status) {
  ASSERT(0 == status);

  delete req;
}

static void do_write(uv_stream_t* stream, uv_write_cb cb) {
  uv_write_t* req = new (std::nothrow) uv_write_t;
  char base_cstr[] = "1234578";
  uv_buf_t buf;
  buf.base = base_cstr;
  buf.len = 8;
  ASSERT_NOT_NULL(req);
  ASSERT(0 == uv_write(req, stream, &buf, 1, cb));
}

static void on_alloc(uv_handle_t*, size_t, uv_buf_t* buf) {
  static char slab[65536];
  buf->base = slab;
  buf->len = sizeof(slab);
}

static void on_read1(uv_stream_t* stream, ssize_t nread, const uv_buf_t*) {
  ASSERT(nread >= 0);

  /* Do write on a half open connection to force WSAECONNABORTED (on Windows)
   * in the subsequent uv_read_start()
   */
  do_write(stream, on_write);

  ASSERT(0 == uv_read_stop(stream));

  ASSERT(0 == uv_read_start(stream, on_alloc, on_read2));

  read_cb_called++;
}

static void on_read2(uv_stream_t* stream, ssize_t nread, const uv_buf_t*) {
  ASSERT(nread < 0);

  ns_tcp::cast(stream)->close();
  server.close();

  read_cb_called++;
}

static void on_connection(ns_tcp* server, int status) {
  ASSERT(0 == status);

  ASSERT(0 == connection.init(server->get_loop()));

  ASSERT(0 == server->accept(&connection));

  ASSERT(0 == uv_read_start(connection.base_stream(), on_alloc, on_read1));
}

static void on_connect(ns_connect<ns_tcp>*, int status) {
  ASSERT(0 == status);

  do_write(client.base_stream(), on_write_close_immediately);
}

TEST_CASE("tcp_read_stop_start", "[tcp]") {
  uv_loop_t* loop = uv_default_loop();

  { /* Server */
    struct sockaddr_in addr;

    ASSERT(0 == uv_ip4_addr("0.0.0.0", kTestPort, &addr));

    ASSERT(0 == server.init(loop));

    ASSERT(0 == server.bind(SOCKADDR_CAST(&addr), 0));

    ASSERT(0 == server.listen(10, on_connection));
  }

  { /* Client */
    struct sockaddr_in addr;

    ASSERT(0 == uv_ip4_addr("127.0.0.1", kTestPort, &addr));

    ASSERT(0 == client.init(loop));

    ASSERT(0 == client.connect(&connect_req,
                               SOCKADDR_CONST_CAST(&addr),
                               on_connect));
  }

  ASSERT(0 == uv_run(loop, UV_RUN_DEFAULT));

  ASSERT(read_cb_called >= 2);

  make_valgrind_happy();
}
