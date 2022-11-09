#ifndef _WIN32

#include "../include/nsuv-inl.h"
#include "./helpers.h"

using nsuv::ns_connect;
using nsuv::ns_tcp;
using nsuv::ns_write;

static char hello_cstr[] = "HELLO";
static uv_loop_t loop;
static ns_tcp tcp_client;
static ns_connect<ns_tcp> connection_request;
static ns_write<ns_tcp> write_request;
static uv_buf_t buf = { hello_cstr, 4 };


static void write_cb(ns_write<ns_tcp>* req, int status) {
  ASSERT(status == UV_ECANCELED);
  req->handle()->close();
}


static void connect_cb(ns_connect<ns_tcp>*, int status) {
  ASSERT(status == UV_ECONNREFUSED);
}


TEST_CASE("tcp_write_after_connect", "[tcp]") {
/* TODO(gengjiawen): Fix test on QEMU. */
#if defined(__QEMU__)
  RETURN_SKIP("Test does not currently work in QEMU");
#endif

  struct sockaddr_in sa;
  ASSERT(0 == uv_ip4_addr("127.0.0.1", kTestPort, &sa));
  ASSERT(0 == uv_loop_init(&loop));
  ASSERT(0 == tcp_client.init(&loop));

  ASSERT(0 == tcp_client.connect(&connection_request,
                                 SOCKADDR_CONST_CAST(&sa),
                                 connect_cb));

  ASSERT(0 == tcp_client.write(&write_request, &buf, 1, write_cb));

  uv_run(&loop, UV_RUN_DEFAULT);

  make_valgrind_happy();
}

#endif /* !_WIN32 */
