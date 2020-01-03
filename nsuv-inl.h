#ifndef NSUV_INL_H_
#define NSUV_INL_H_

#include "./nsuv.h"
#include <uv.h>

#include <cstring>  // memcpy
#include <new>  // nothrow
#include <utility>  // move
#include <vector>

namespace nsuv {

// TODO(trevnorris): Don't like using a macro here.
#define NSUV_GET_CB(cb1, cb2)                                                \
  ((cb1) == nullptr ? nullptr : (cb2))


/* ns_req */

template <class UV_T, class R_T, class H_T>
template <typename CB, typename D_T>
void ns_req<UV_T, R_T, H_T>::init(H_T* handle, CB cb, D_T* data) {
  handle_ = handle;
  req_cb_ = reinterpret_cast<void(*)()>(cb);
  req_cb_data_ = data;
}

template <class UV_T, class R_T, class H_T>
H_T* ns_req<UV_T, R_T, H_T>::handle() {
  return handle_;
}

template <class UV_T, class R_T, class H_T>
UV_T* ns_req<UV_T, R_T, H_T>::uv_req() {
  return static_cast<UV_T*>(this);
}

template <class UV_T, class R_T, class H_T>
uv_req_t* ns_req<UV_T, R_T, H_T>::base_req() {
  return reinterpret_cast<uv_req_t*>(uv_req());
}

template <class UV_T, class R_T, class H_T>
uv_req_type ns_req<UV_T, R_T, H_T>::get_type() {
#if UV_VERSION_HEX >= 70400
  return uv_req_get_type(base_req());
#else
  return UV_T::type;
#endif
}

template <class UV_T, class R_T, class H_T>
const char* ns_req<UV_T, R_T, H_T>::type_name() {
#if UV_VERSION_HEX >= 70400
  return uv_req_type_name(get_type());
#else
  switch (get_type()) {
#define XX(uc, lc) case UV_##uc: return #lc;
    UV_REQ_TYPE_MAP(XX)
#undef XX
    case UV_REQ_TYPE_MAX:
    case UV_UNKNOWN_REQ: return nullptr;
  }
  return nullptr;
#endif
}

template <class UV_T, class R_T, class H_T>
int ns_req<UV_T, R_T, H_T>::cancel() {
  return uv_cancel(base_req());
}

template <class UV_T, class R_T, class H_T>
template <typename D_T>
D_T* ns_req<UV_T, R_T, H_T>::get_data() {
  return static_cast<D_T*>(UV_T::data);
}

template <class UV_T, class R_T, class H_T>
void ns_req<UV_T, R_T, H_T>::set_data(void* ptr) {
  UV_T::data = ptr;
}

template <class UV_T, class R_T, class H_T>
R_T* ns_req<UV_T, R_T, H_T>::cast(void* req) {
  return cast(static_cast<uv_req_t*>(req));
}

template <class UV_T, class R_T, class H_T>
R_T* ns_req<UV_T, R_T, H_T>::cast(uv_req_t* req) {
  return cast(reinterpret_cast<UV_T*>(req));
}

template <class UV_T, class R_T, class H_T>
R_T* ns_req<UV_T, R_T, H_T>::cast(UV_T* req) {
  return static_cast<R_T*>(req);
}


/* ns_connect */

template <class H_T>
template <typename CB, typename D_T>
void ns_connect<H_T>::init(
    H_T* handle, const struct sockaddr* addr, CB cb, D_T* data) {
  ns_req<uv_connect_t, ns_connect<H_T>, H_T>::init(handle, cb, data);
  std::memcpy(&addr_, addr, sizeof(addr_));
}

template <class H_T>
const sockaddr* ns_connect<H_T>::sockaddr() {
  return const_cast<const struct sockaddr*>(&addr_);
}


/* ns_write */

template <class H_T>
template <typename CB, typename D_T>
void ns_write<H_T>::init(H_T* handle,
                         const uv_buf_t bufs[],
                         size_t nbufs,
                         CB cb,
                         D_T* data) {
  ns_req<uv_write_t, ns_write<H_T>, H_T>::init(handle, cb, data);
  for (size_t i = 0; i < nbufs; i++) {
    bufs_.push_back(bufs[i]);
  }
}

template <class H_T>
template <typename CB, typename D_T>
void ns_write<H_T>::init(H_T* handle,
                         const std::vector<uv_buf_t>& bufs,
                         CB cb,
                         D_T* data) {
  ns_req<uv_write_t, ns_write<H_T>, H_T>::init(handle, cb, data);
  bufs_ = bufs;
}

template <class H_T>
template <typename CB, typename D_T>
void ns_write<H_T>::init(H_T* handle,
                         std::vector<uv_buf_t>&& bufs,
                         CB cb, D_T* data) {
  ns_req<uv_write_t, ns_write<H_T>, H_T>::init(handle, cb, data);
  bufs_ = std::move(bufs);
}

template <class H_T>
std::vector<uv_buf_t>& ns_write<H_T>::bufs() {
  return bufs_;
}


/* ns_udp_send */

template <typename CB, typename D_T>
void ns_udp_send::init(ns_udp* handle,
                       const uv_buf_t bufs[],
                       size_t nbufs,
                       const struct sockaddr* addr,
                       CB cb,
                       D_T* data) {
  ns_req<uv_udp_send_t, ns_udp_send, ns_udp>::init(handle, cb, data);
  bufs_.reserve(nbufs);
  std::memcpy(bufs_.data(), bufs, sizeof(uv_buf_t) * nbufs);
  std::memcpy(&addr_, addr, sizeof(addr_));
}

template <typename CB, typename D_T>
void ns_udp_send::init(ns_udp* handle,
                       const std::vector<uv_buf_t>& bufs,
                       const struct sockaddr* addr,
                       CB cb,
                       D_T* data) {
  ns_req<uv_udp_send_t, ns_udp_send, ns_udp>::init(handle, cb, data);
  bufs_ = bufs;
  std::memcpy(&addr_, addr, sizeof(addr_));
}

template <typename CB, typename D_T>
void ns_udp_send::init(ns_udp* handle,
                       std::vector<uv_buf_t>&& bufs,
                       const struct sockaddr* addr,
                       CB cb,
                       D_T* data) {
  ns_req<uv_udp_send_t, ns_udp_send, ns_udp>::init(handle, cb, data);
  bufs_ = std::move(bufs);
  std::memcpy(&addr_, addr, sizeof(addr_));
}

std::vector<uv_buf_t>& ns_udp_send::bufs() {
  return bufs_;
}

const sockaddr* ns_udp_send::sockaddr() {
  return const_cast<const struct sockaddr*>(&addr_);
}


/* ns_handle */

template <class UV_T, class H_T>
UV_T* ns_handle<UV_T, H_T>::uv_handle() {
  return static_cast<UV_T*>(this);
}

template <class UV_T, class H_T>
uv_handle_t* ns_handle<UV_T, H_T>::base_handle() {
  return reinterpret_cast<uv_handle_t*>(uv_handle());
}

template <class UV_T, class H_T>
uv_loop_t* ns_handle<UV_T, H_T>::get_loop() {
  return uv_handle_get_loop(base_handle());
}

template <class UV_T, class H_T>
uv_handle_type ns_handle<UV_T, H_T>::get_type() {
#if UV_VERSION_HEX >= 70400
  return uv_handle_get_type(base_handle());
#else
  return UV_T::type;
#endif
}

template <class UV_T, class H_T>
const char* ns_handle<UV_T, H_T>::type_name() {
#if UV_VERSION_HEX >= 70400
  return uv_handle_type_name(get_type());
#else
  switch (get_type()) {
#define XX(uc, lc) case UV_##uc: return #lc;
    UV_HANDLE_TYPE_MAP(XX)
#undef XX
    case UV_FILE: return "file";
    case UV_HANDLE_TYPE_MAX:
    case UV_UNKNOWN_HANDLE: return nullptr;
  }
  return nullptr;
#endif
}

template <class UV_T, class H_T>
bool ns_handle<UV_T, H_T>::is_closing() {
  return uv_is_closing(base_handle()) > 0;
}

template <class UV_T, class H_T>
bool ns_handle<UV_T, H_T>::is_active() {
  return uv_is_active(base_handle()) > 0;
}

template <class UV_T, class H_T>
void ns_handle<UV_T, H_T>::close() {
  uv_close(base_handle(), nullptr);
}

template <class UV_T, class H_T>
void ns_handle<UV_T, H_T>::close(void(*cb)(H_T*)) {
  auto lcb = [](uv_handle_t* handle) {
    H_T* wrap = H_T::cast(handle);
    auto* cb_ = reinterpret_cast<decltype(cb)>(wrap->close_cb_ptr_);
    cb_(wrap);
  };
  close_cb_ptr_ = reinterpret_cast<void(*)()>(cb);
  uv_close(base_handle(), NSUV_GET_CB(cb, lcb));
}

template <class UV_T, class H_T>
template <typename D_T>
void ns_handle<UV_T, H_T>::close(void(*cb)(H_T*, D_T*), D_T* data) {
  auto lcb = [](uv_handle_t* handle) {
    H_T* wrap = H_T::cast(handle);
    auto* cb_ = reinterpret_cast<decltype(cb)>(wrap->close_cb_ptr_);
    cb_(wrap, static_cast<D_T*>(wrap->close_cb_data_));
  };
  close_cb_ptr_ = reinterpret_cast<void(*)()>(cb);
  close_cb_data_ = data;
  uv_close(base_handle(), NSUV_GET_CB(cb, lcb));
}

template <class UV_T, class H_T>
void ns_handle<UV_T, H_T>::close_and_delete() {
  uv_close(base_handle(), close_delete_cb_);
}

template <class UV_T, class H_T>
void ns_handle<UV_T, H_T>::set_data(void* data) {
  UV_T::data = data;
}

template <class UV_T, class H_T>
template <typename D_T>
D_T* ns_handle<UV_T, H_T>::get_data() {
  return static_cast<D_T*>(UV_T::data);
}

template <class UV_T, class H_T>
void ns_handle<UV_T, H_T>::unref() {
  uv_unref(base_handle());
}

template <class UV_T, class H_T>
H_T* ns_handle<UV_T, H_T>::cast(void* handle) {
  return cast(static_cast<uv_handle_t*>(handle));
}

template <class UV_T, class H_T>
H_T* ns_handle<UV_T, H_T>::cast(uv_handle_t* handle) {
  return cast(reinterpret_cast<UV_T*>(handle));
}

template <class UV_T, class H_T>
H_T* ns_handle<UV_T, H_T>::cast(UV_T* handle) {
  return static_cast<H_T*>(handle);
}

template <class UV_T, class H_T>
void ns_handle<UV_T, H_T>::close_delete_cb_(uv_handle_t* handle) {
  delete H_T::cast(handle);
}


/* ns_stream */

template <class UV_T, class H_T>
uv_stream_t* ns_stream<UV_T, H_T>::base_stream() {
  return reinterpret_cast<uv_stream_t*>(H_T::uv_handle());
}

template <class UV_T, class H_T>
int ns_stream<UV_T, H_T>::listen(int backlog, void(*cb)(H_T*, int)) {
  auto lcb = [](uv_stream_t* handle, int status) {
    auto* server = H_T::cast(handle);
    auto* cb_ = reinterpret_cast<decltype(cb)>(server->listen_cb_ptr_);
    cb_(server, status);
  };
  listen_cb_ptr_ = reinterpret_cast<void(*)()>(cb);
  return uv_listen(base_stream(), backlog, NSUV_GET_CB(cb, lcb));
}

template <class UV_T, class H_T>
template <typename D_T>
int ns_stream<UV_T, H_T>::listen(int backlog,
                                 void(*cb)(H_T*, int, D_T*),
                                 D_T* data) {
  auto lcb = [](uv_stream_t* handle, int status) {
    auto* server = H_T::cast(handle);
    auto* cb_ = reinterpret_cast<decltype(cb)>(server->listen_cb_ptr_);
    cb_(server, status, static_cast<D_T*>(server->listen_cb_data_));
  };
  listen_cb_ptr_ = reinterpret_cast<void(*)()>(cb);
  listen_cb_data_ = data;
  return uv_listen(base_stream(), backlog, NSUV_GET_CB(cb, lcb));
}

template <class UV_T, class H_T>
int ns_stream<UV_T, H_T>::write(ns_write<H_T>* req,
                                const uv_buf_t bufs[],
                                size_t nbufs,
                                void(*cb)(ns_write<H_T>*, int)) {
  auto lcb = [](uv_write_t* uv_req, int status) {
    auto* wreq = ns_write<H_T>::cast(uv_req);
    auto* cb_ = reinterpret_cast<decltype(cb)>(wreq->req_cb_);
    cb_(wreq, status);
  };
  req->init(H_T::cast(this), bufs, nbufs, cb);
  return uv_write(req->uv_req(),
                  base_stream(),
                  req->bufs().data(),
                  req->bufs().size(),
                  NSUV_GET_CB(cb, lcb));
}

template <class UV_T, class H_T>
int ns_stream<UV_T, H_T>::write(ns_write<H_T>* req,
                                const std::vector<uv_buf_t>& bufs,
                                void(*cb)(ns_write<H_T>*, int)) {
  auto lcb = [](uv_write_t* uv_req, int status) {
    auto* wreq = ns_write<H_T>::cast(uv_req);
    auto* cb_ = reinterpret_cast<decltype(cb)>(wreq->req_cb_);
    cb_(wreq, status);
  };
  req->init(H_T::cast(this), bufs, cb);
  return uv_write(req->uv_req(),
                  base_stream(),
                  req->bufs().data(),
                  req->bufs().size(),
                  NSUV_GET_CB(cb, lcb));
}

template <class UV_T, class H_T>
template <typename D_T>
int ns_stream<UV_T, H_T>::write(ns_write<H_T>* req,
                                const uv_buf_t bufs[],
                                size_t nbufs,
                                void(*cb)(ns_write<H_T>*, int, D_T*),
                                D_T* data) {
  auto lcb = [](uv_write_t* uv_req, int status) {
    auto* wreq = ns_write<H_T>::cast(uv_req);
    auto* cb_ = reinterpret_cast<decltype(cb)>(wreq->req_cb_);
    cb_(wreq, status, static_cast<D_T*>(wreq->req_cb_data_));
  };
  req->init(H_T::cast(this), bufs, nbufs, cb, data);
  return uv_write(req->uv_req(),
                  base_stream(),
                  req->bufs().data(),
                  req->bufs().size(), NSUV_GET_CB(cb, lcb));
}

template <class UV_T, class H_T>
template <typename D_T>
int ns_stream<UV_T, H_T>::write(ns_write<H_T>* req,
                                const std::vector<uv_buf_t>& bufs,
                                void(*cb)(ns_write<H_T>*, int, D_T*),
                                D_T* data) {
  auto lcb = [](uv_write_t* uv_req, int status) {
    auto* wreq = ns_write<H_T>::cast(uv_req);
    auto* cb_ = reinterpret_cast<decltype(cb)>(wreq->req_cb_);
    cb_(wreq, status, static_cast<D_T*>(wreq->req_cb_data_));
  };
  req->init(H_T::cast(this), bufs, cb, data);
  return uv_write(req->uv_req(),
                  base_stream(),
                  req->bufs().data(),
                  req->bufs().size(),
                  NSUV_GET_CB(cb, lcb));
}


/* ns_async */

int ns_async::init(uv_loop_t* loop, void(*cb)(ns_async*)) {
  auto lcb = [](uv_async_t* handle) {
    ns_async* wrap = ns_async::cast(handle);
    auto* cb_ = reinterpret_cast<decltype(cb)>(wrap->async_cb_ptr_);
    cb_(wrap);
  };
  async_cb_ptr_ = reinterpret_cast<void(*)()>(cb);
  return uv_async_init(loop, uv_handle(), NSUV_GET_CB(cb, lcb));
}

template <typename D_T>
int ns_async::init(uv_loop_t* loop, void(*cb)(ns_async*, D_T*), D_T* data) {
  auto lcb = [](uv_async_t* handle) {
    auto* wrap = ns_async::cast(handle);
    auto* cb_ = reinterpret_cast<decltype(cb)>(wrap->async_cb_ptr_);
    cb_(wrap, static_cast<D_T*>(wrap->async_cb_data_));
  };
  async_cb_ptr_ = reinterpret_cast<void(*)()>(cb);
  async_cb_data_ = data;
  return uv_async_init(loop, uv_handle(), NSUV_GET_CB(cb, lcb));
}

int ns_async::send() {
  return uv_async_send(uv_handle());
}


/* ns_poll */

int ns_poll::init(uv_loop_t* loop, int fd) {
  return uv_poll_init(loop, uv_handle(), fd);
}

int ns_poll::init_socket(uv_loop_t* loop, uv_os_sock_t socket) {
  return uv_poll_init_socket(loop, uv_handle(), socket);
}

int ns_poll::start(int events, void(*cb)(ns_poll*, int, int)) {
  auto lcb = [](uv_poll_t* handle, int poll, int events) {
    ns_poll* wrap = ns_poll::cast(handle);
    auto* cb_ = reinterpret_cast<decltype(cb)>(wrap->poll_cb_ptr_);
    cb_(wrap, poll, events);
  };
  poll_cb_ptr_ = reinterpret_cast<void(*)()>(cb);
  return uv_poll_start(uv_handle(), events, NSUV_GET_CB(cb, lcb));
}

template <typename D_T>
int ns_poll::start(int events, void(*cb)(ns_poll*, int, int, D_T*), D_T* data) {
  auto lcb = [](uv_poll_t* handle, int poll, int events) {
    ns_poll* wrap = ns_poll::cast(handle);
    auto* cb_ = reinterpret_cast<decltype(cb)>(wrap->poll_cb_ptr_);
    cb_(wrap, poll, events, static_cast<D_T*>(wrap->poll_cb_data_));
  };
  poll_cb_ptr_ = reinterpret_cast<void(*)()>(cb);
  poll_cb_data_ = data;
  return uv_poll_start(uv_handle(), events, NSUV_GET_CB(cb, lcb));
}

int ns_poll::stop() {
  return uv_poll_stop(uv_handle());
}


/* ns_tcp */

int ns_tcp::init(uv_loop_t* loop) {
  return uv_tcp_init(loop, uv_handle());
}

int ns_tcp::bind(const struct sockaddr* addr, unsigned int flags) {
  return uv_tcp_bind(uv_handle(), addr, flags);
}

int ns_tcp::connect(ns_connect<ns_tcp>* req,
                    const struct sockaddr* addr,
                    void(*cb)(ns_connect<ns_tcp>*, int)) {
  auto lcb = [](uv_connect_t* uv_req, int status) {
    auto* creq = ns_connect<ns_tcp>::cast(uv_req);
    auto* cb_ = reinterpret_cast<decltype(cb)>(creq->req_cb_);
    cb_(creq, status);
  };
  req->init(this, addr, cb);
  return uv_tcp_connect(req->uv_req(),
                        uv_handle(),
                        addr,
                        NSUV_GET_CB(cb, lcb));
}

template <typename D_T>
int ns_tcp::connect(ns_connect<ns_tcp>* req,
                    const struct sockaddr* addr,
                    void(*cb)(ns_connect<ns_tcp>*, int, D_T*),
                    D_T* data) {
  auto lcb = [](uv_connect_t* uv_req, int status) {
    auto* creq = ns_connect<ns_tcp>::cast(uv_req);
    auto* cb_ = reinterpret_cast<decltype(cb)>(creq->req_cb_);
    cb_(creq, status, static_cast<D_T*>(creq->req_cb_data_));
  };
  req->init(this, addr, cb, data);
  return uv_tcp_connect(req->uv_req(),
                        uv_handle(),
                        addr,
                        NSUV_GET_CB(cb, lcb));
}

int ns_tcp::nodelay(bool enable) {
  return uv_tcp_nodelay(uv_handle(), enable);
}

int ns_tcp::keepalive(bool enable, int delay) {
  return uv_tcp_keepalive(uv_handle(), enable, delay);
}


/* ns_timer */

int ns_timer::init(uv_loop_t* loop) {
  return uv_timer_init(loop, uv_handle());
}

int ns_timer::start(void(*cb)(ns_timer*), uint64_t timeout, uint64_t repeat) {
  auto lcb = [](uv_timer_t* handle) {
    ns_timer* wrap = ns_timer::cast(handle);
    auto* cb_ = reinterpret_cast<decltype(cb)>(wrap->timer_cb_ptr_);
    cb_(wrap);
  };
  timer_cb_ptr_ = reinterpret_cast<void(*)()>(cb);
  return uv_timer_start(uv_handle(), NSUV_GET_CB(cb, lcb), timeout, repeat);
}

template <typename D_T>
int ns_timer::start(void(*cb)(ns_timer*, D_T*),
                    uint64_t timeout,
                    uint64_t repeat,
                    D_T* data) {
  auto lcb = [](uv_timer_t* handle) {
    ns_timer* wrap = ns_timer::cast(handle);
    auto* cb_ = reinterpret_cast<decltype(cb)>(wrap->timer_cb_ptr_);
    cb_(wrap, static_cast<D_T*>(wrap->timer_cb_data_));
  };
  timer_cb_ptr_ = reinterpret_cast<void(*)()>(cb);
  timer_cb_data_ = data;
  return uv_timer_start(uv_handle(), NSUV_GET_CB(cb, lcb), timeout, repeat);
}

int ns_timer::stop() {
  return uv_timer_stop(uv_handle());
}

size_t ns_timer::get_repeat() {
  return uv_timer_get_repeat(uv_handle());
}

/* ns_prepare */

int ns_prepare::init(uv_loop_t* loop) {
  return uv_prepare_init(loop, uv_handle());
}

int ns_prepare::start(void(*cb)(ns_prepare*)) {
  auto lcb = [](uv_prepare_t* handle) {
    ns_prepare* wrap = ns_prepare::cast(handle);
    auto* cb_ = reinterpret_cast<decltype(cb)>(wrap->prepare_cb_ptr_);
    cb_(wrap);
  };
  prepare_cb_ptr_ = reinterpret_cast<void(*)()>(cb);
  return uv_prepare_start(uv_handle(), NSUV_GET_CB(cb, lcb));
}

template <typename D_T>
int ns_prepare::start(void(*cb)(ns_prepare*, D_T*), D_T* data) {
  auto lcb = [](uv_prepare_t* handle) {
    ns_prepare* wrap = ns_prepare::cast(handle);
    auto* cb_ = reinterpret_cast<decltype(cb)>(wrap->prepare_cb_ptr_);
    cb_(wrap, static_cast<D_T*>(wrap->prepare_cb_data_));
  };
  prepare_cb_ptr_ = reinterpret_cast<void(*)()>(cb);
  prepare_cb_data_ = data;
  return uv_prepare_start(uv_handle(), NSUV_GET_CB(cb, lcb));
}

int ns_prepare::stop() {
  return uv_prepare_stop(uv_handle());
}


/* ns_udp */

int ns_udp::init(uv_loop_t* loop) {
  return uv_udp_init(loop, uv_handle());
}

int ns_udp::send(ns_udp_send* req,
                 const uv_buf_t bufs[],
                 size_t nbufs,
                 const struct sockaddr* addr,
                 void(*cb)(ns_udp_send*, int)) {
  auto lcb = [](uv_udp_send_t* uv_req, int status) {
    auto* ureq = ns_udp_send::cast(uv_req);
    auto* cb_ = reinterpret_cast<decltype(cb)>(ureq->req_cb_);
    cb_(ureq, status);
  };
  req->init(this, bufs, nbufs, addr, cb);
  return uv_udp_send(req->uv_req(),
                     uv_handle(),
                     req->bufs().data(),
                     req->bufs().size(),
                     addr,
                     NSUV_GET_CB(cb, lcb));
}

int ns_udp::send(ns_udp_send* req,
                 const std::vector<uv_buf_t>& bufs,
                 const struct sockaddr* addr,
                 void(*cb)(ns_udp_send*, int)) {
  auto lcb = [](uv_udp_send_t* uv_req, int status) {
    auto* ureq = ns_udp_send::cast(uv_req);
    auto* cb_ = reinterpret_cast<decltype(cb)>(ureq->req_cb_);
    cb_(ureq, status);
  };
  req->init(this, bufs, addr, cb);
  return uv_udp_send(req->uv_req(),
                     uv_handle(),
                     req->bufs().data(),
                     req->bufs().size(),
                     addr,
                     NSUV_GET_CB(cb, lcb));
}

template <typename D_T>
int ns_udp::send(ns_udp_send* req,
                 const uv_buf_t bufs[],
                 size_t nbufs,
                 const struct sockaddr* addr,
                 void(*cb)(ns_udp_send*, int, D_T*),
                 D_T* data) {
  auto lcb = [](uv_udp_send_t* uv_req, int status) {
    auto* ureq = ns_udp_send::cast(uv_req);
    auto* cb_ = reinterpret_cast<decltype(cb)>(ureq->req_cb_);
    cb_(ureq, status, static_cast<D_T*>(ureq->req_cb_data_));
  };
  req->init(this, bufs, nbufs, addr, cb, data);
  return uv_udp_send(req->uv_req(),
                     uv_handle(),
                     req->bufs().data(),
                     req->bufs().size(),
                     addr,
                     NSUV_GET_CB(cb, lcb));
}

template <typename D_T>
int ns_udp::send(ns_udp_send* req,
                 const std::vector<uv_buf_t>& bufs,
                 const struct sockaddr* addr,
                 void(*cb)(ns_udp_send*, int, D_T*),
                 D_T* data) {
  auto lcb = [](uv_udp_send_t* uv_req, int status) {
    auto* ureq = ns_udp_send::cast(uv_req);
    auto* cb_ = reinterpret_cast<decltype(cb)>(ureq->req_cb_);
    cb_(ureq, status, static_cast<D_T*>(ureq->req_cb_data_));
  };
  req->init(this, bufs, addr, cb, data);
  return uv_udp_send(req->uv_req(),
                     uv_handle(),
                     req->bufs().data(),
                     req->bufs().size(),
                     addr,
                     NSUV_GET_CB(cb, lcb));
}


/* ns_mutex */

int ns_mutex::init() {
  return uv_mutex_init(&mutex_);
}

int ns_mutex::init_recursive() {
  return uv_mutex_init_recursive(&mutex_);
}

void ns_mutex::destroy() {
  uv_mutex_destroy(&mutex_);
}

void ns_mutex::lock() {
  uv_mutex_lock(&mutex_);
}

int ns_mutex::trylock() {
  return uv_mutex_trylock(&mutex_);
}

void ns_mutex::unlock() {
  uv_mutex_unlock(&mutex_);
}

ns_mutex::scoped_lock::scoped_lock(ns_mutex* mutex) : mutex_ref_(mutex) {
  mutex_ref_->lock();
}

ns_mutex::scoped_lock::~scoped_lock() {
  mutex_ref_->unlock();
}


/* ns_thread */

int ns_thread::create(void(*cb)(ns_thread*)) {
  auto lcb = [](void* arg) {
    auto* wrap = static_cast<decltype(this)>(arg);
    auto* cb_ = reinterpret_cast<decltype(cb)>(wrap->thread_cb_ptr_);
    cb_(wrap);
  };
  parent_ = uv_thread_self();
  thread_cb_ptr_ = reinterpret_cast<void(*)()>(cb);
  return uv_thread_create(&thread_, NSUV_GET_CB(cb, lcb), this);
}

template <typename D_T>
int ns_thread::create(void(*cb)(ns_thread*, D_T*), D_T* data) {
  auto lcb = [](void* arg) {
    auto* wrap = static_cast<decltype(this)>(arg);
    auto* cb_ = reinterpret_cast<decltype(cb)>(wrap->thread_cb_ptr_);
    cb_(wrap, static_cast<D_T*>(wrap->thread_cb_data_));
  };
  parent_ = uv_thread_self();
  thread_cb_ptr_ = reinterpret_cast<void(*)()>(cb);
  thread_cb_data_ = data;
  return uv_thread_create(&thread_, NSUV_GET_CB(cb, lcb), this);
}

int ns_thread::create_ex(const uv_thread_options_t* params,
                         void(*cb)(ns_thread*)) {
  auto lcb = [](void* arg) {
    auto* wrap = static_cast<decltype(this)>(arg);
    auto* cb_ = reinterpret_cast<decltype(cb)>(wrap->thread_cb_ptr_);
    cb_(wrap);
  };
  parent_ = uv_thread_self();
  thread_cb_ptr_ = reinterpret_cast<void(*)()>(cb);
  return uv_thread_create_ex(&thread_, params, NSUV_GET_CB(cb, lcb), this);
}

template <typename D_T>
int ns_thread::create_ex(const uv_thread_options_t* params,
                         void(*cb)(ns_thread*, D_T*),
                         D_T* data) {
  auto lcb = [](void* arg) {
    auto* wrap = static_cast<decltype(this)>(arg);
    auto* cb_ = reinterpret_cast<decltype(cb)>(wrap->thread_cb_ptr_);
    cb_(wrap, static_cast<D_T*>(wrap->thread_cb_data_));
  };
  parent_ = uv_thread_self();
  thread_cb_ptr_ = reinterpret_cast<void(*)()>(cb);
  thread_cb_data_ = data;
  return uv_thread_create_ex(&thread_, params, NSUV_GET_CB(cb, lcb), this);
}

int ns_thread::join() {
  return uv_thread_join(&thread_);
}

uv_thread_t ns_thread::owner() {
  return parent_;
}

int ns_thread::equal(uv_thread_t* t2) {
  return uv_thread_equal(&thread_, t2);
}

uv_thread_t ns_thread::self() {
  return uv_thread_self();
}

#undef NSUV_GET_CB

}  // namespace nsuv

#endif  // NSUV_INL_H_
