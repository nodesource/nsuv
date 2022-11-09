#if !defined(_WIN32)

#include "../include/nsuv-inl.h"
#include "./helpers.h"

#include <errno.h>
#include <sys/socket.h>
#include <unistd.h>

using nsuv::ns_connect;
using nsuv::ns_idle;
using nsuv::ns_tcp;

static ns_tcp server_handle;
static ns_tcp client_handle;
static ns_tcp peer_handle;
static ns_idle idle;
static ns_connect<ns_tcp> connect_req;
static int ticks;
static const int kMaxTicks = 10;

static void alloc_cb(uv_handle_t*, size_t, uv_buf_t* buf) {
  static char storage[1024];
  *buf = uv_buf_init(storage, sizeof(storage));
}


static void idle_cb(ns_idle* idle) {
  if (++ticks < kMaxTicks)
    return;

  server_handle.close();
  client_handle.close();
  peer_handle.close();
  idle->close();
}


static void read_cb(uv_stream_t* handle, ssize_t nread, const uv_buf_t*) {
#ifdef __MVS__
  char lbuf[12];
#endif
  uv_os_fd_t fd;

  ASSERT(nread >= 0);
  ASSERT(0 == uv_fileno(reinterpret_cast<uv_handle_t*>(handle), &fd));
  ASSERT(0 == idle.start(idle_cb));

#ifdef __MVS__
  /* Need to flush out the OOB data. Otherwise, this callback will get
   * triggered on every poll with nread = 0.
   */
  ASSERT(-1 != recv(fd, lbuf, sizeof(lbuf), MSG_OOB));
#endif
}


static void connect_cb(ns_connect<ns_tcp>* req, int status) {
  ASSERT(req->handle() == &client_handle);
  ASSERT(0 == status);
}


static void connection_cb(ns_tcp* handle, int status) {
  int r;
  uv_os_fd_t fd;

  ASSERT(0 == status);
  ASSERT(0 == uv_accept(handle->base_stream(), peer_handle.base_stream()));
  ASSERT(0 == uv_read_start(peer_handle.base_stream(), alloc_cb, read_cb));

  /* Send some OOB data */
  ASSERT(0 == uv_fileno(reinterpret_cast<uv_handle_t*>(&client_handle), &fd));

  ASSERT(0 == uv_stream_set_blocking(client_handle.base_stream(), 1));

  /* The problem triggers only on a second message, it seem that xnu is not
   * triggering `kevent()` for the first one
   */
  do {
    r = send(fd, "hello", 5, MSG_OOB);
  } while (r < 0 && errno == EINTR);
  ASSERT(5 == r);

  do {
    r = send(fd, "hello", 5, MSG_OOB);
  } while (r < 0 && errno == EINTR);
  ASSERT(5 == r);

  ASSERT(0 == uv_stream_set_blocking(client_handle.base_stream(), 0));
}


TEST_CASE("tcp_oob", "[tcp]") {
  struct sockaddr_in addr;
  uv_loop_t* loop;

  ASSERT(0 == uv_ip4_addr("127.0.0.1", kTestPort, &addr));
  loop = uv_default_loop();

  ASSERT(0 == server_handle.init(loop));
  ASSERT(0 == client_handle.init(loop));
  ASSERT(0 == peer_handle.init(loop));
  ASSERT(0 == idle.init(loop));
  ASSERT(0 == server_handle.bind(SOCKADDR_CONST_CAST(&addr), 0));
  ASSERT(0 == server_handle.listen(1, connection_cb));

  /* Ensure two separate packets */
  ASSERT(0 == client_handle.nodelay(true));

  ASSERT(0 == client_handle.connect(&connect_req,
                                    SOCKADDR_CONST_CAST(&addr),
                                    connect_cb));
  ASSERT(0 == uv_run(loop, UV_RUN_DEFAULT));

  ASSERT(ticks == kMaxTicks);

  make_valgrind_happy();
}

#endif /* !_WIN32 */
