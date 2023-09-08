# nsuv
C++ wrapper around libuv focused on making callback arg passing safer. The
data passed to the callback does not use the `data` member of the
`uv_handle_t` or `uv_req_t`. So they can still be used as usual.

You can [check out our blog post](https://nodesource.com/blog/intro-nsuv/)
for additional information.

Example usage:
```cpp
// Notice that the second argument of the callback can contain any pointer type,
// and it'll automatically be checked from the callsite, and give a compiler
// error if it doesn't match.

#include "nsuv-inl.h"

using namespace nsuv;

ns_tcp client;
ns_tcp incoming;
ns_tcp server;
ns_connect<ns_tcp> connect_req;
ns_write<ns_tcp> write_req;

static void alloc_cb(ns_tcp* handle, size_t, uv_buf_t* buf) {
  static char slab[1024];

  buf->base = slab;
  buf->len = sizeof(slab);
}

static void read_cb(ns_tcp* handle, ssize_t, const uv_buf_t*) {
  handle->close();
  client.close();
  server.close();
}

static void write_cb(ns_write<ns_tcp>* req, int) {
  // Retrieve a reference to the uv_buf_t array and size.
  uv_buf_t* bufs = req->bufs();
  size_t size = req->size();
}

static void connection_cb(ns_tcp* server, int) {
  int r;
  r = incoming.init(server->get_loop());
  r = server->accept(&incoming);
  r = incoming.read_start(alloc_cb, read_cb);
}

static void connect_cb(ns_connect<ns_tcp>* req, int, char* data) {
  static char bye_ctr[] = "BYE";
  uv_buf_t buf1 = uv_buf_init(data, strlen(data));
  uv_buf_t buf2 = uv_buf_init(bye_ctr, strlen(bye_ctr));
  // Write to the handle attached to this request and pass along data.
  int r = req->handle()->write(&write_req, { buf1, buf2 }, write_cb);
}

static void do_listen() {
  static char hello_cstr[] = "HELLO";
  struct sockaddr_in addr_in;
  struct sockaddr* addr;
  int r;

  r = uv_ip4_addr("127.0.0.1", 9999, &addr_in);
  addr = reinterpret_cast<struct sockaddr*>(&addr_in);

  // Server setup.
  r = server.init(uv_default_loop());
  r = server.bind(addr, 0);
  r = server.listen(1, connection_cb);

  // Client connection.
  r = client.init(uv_default_loop());
  r = client.connect(&connect_req, addr, connect_cb, hello_cstr);

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);
}
```

Additional usage can be seen in `test/`.
