#ifndef INCLUDE_NSUV_INL_H_
#define INCLUDE_NSUV_INL_H_

#include "./nsuv.h"

#if !defined(_WIN32)
#include <sys/un.h>  // sockaddr_un
#endif

#include <cstdlib>  // abort
#include <cstring>  // memcpy
#include <new>      // nothrow
#include <utility>  // move

namespace nsuv {

#define NSUV_CAST_NULLPTR static_cast<void*>(nullptr)

#define NSUV_CHECK_NULL(v, r) ((v) == nullptr ? nullptr : (r))

#define NSUV_OK 0

using util::addr_size;

/* ns_base_req */

template <class UV_T, class R_T>
template <typename CB, typename D_T>
void ns_base_req<UV_T, R_T>::init(CB cb, D_T* data) {
  req_cb_ = reinterpret_cast<void (*)()>(cb);
  req_cb_data_ = data;
}

template <class UV_T, class R_T>
UV_T* ns_base_req<UV_T, R_T>::uv_req() {
  return static_cast<UV_T*>(this);
}

template <class UV_T, class R_T>
uv_req_t* ns_base_req<UV_T, R_T>::base_req() {
  return reinterpret_cast<uv_req_t*>(uv_req());
}

template <class UV_T, class R_T>
uv_req_type ns_base_req<UV_T, R_T>::get_type() {
#if UV_VERSION_HEX >= 70400
  return uv_req_get_type(base_req());
#else
  return UV_T::type;
#endif
}

template <class UV_T, class R_T>
const char* ns_base_req<UV_T, R_T>::type_name() {
#if UV_VERSION_HEX >= 70400
  return uv_req_type_name(get_type());
#else
  switch (get_type()) {
#  define XX(uc, lc)                                                           \
    case UV_##uc:                                                              \
      return #lc;
    UV_REQ_TYPE_MAP(XX)
#  undef XX
    case UV_REQ_TYPE_MAX:
    case UV_UNKNOWN_REQ:
      return nullptr;
  }
  return nullptr;
#endif
}

template <class UV_T, class R_T>
int ns_base_req<UV_T, R_T>::cancel() {
  return uv_cancel(base_req());
}

template <class UV_T, class R_T>
template <typename D_T>
D_T* ns_base_req<UV_T, R_T>::get_data() {
  return static_cast<D_T*>(UV_T::data);
}

template <class UV_T, class R_T>
void ns_base_req<UV_T, R_T>::set_data(void* ptr) {
  UV_T::data = ptr;
}

template <class UV_T, class R_T>
R_T* ns_base_req<UV_T, R_T>::cast(void* req) {
  return cast(static_cast<uv_req_t*>(req));
}

template <class UV_T, class R_T>
R_T* ns_base_req<UV_T, R_T>::cast(uv_req_t* req) {
  return cast(reinterpret_cast<UV_T*>(req));
}

template <class UV_T, class R_T>
R_T* ns_base_req<UV_T, R_T>::cast(UV_T* req) {
  return static_cast<R_T*>(req);
}

/* ns_req */

template <class UV_T, class R_T, class H_T>
template <typename CB, typename D_T>
void ns_req<UV_T, R_T, H_T>::init(CB cb, D_T* data) {
  ns_base_req<UV_T, R_T>::req_cb_ = reinterpret_cast<void (*)()>(cb);
  ns_base_req<UV_T, R_T>::req_cb_data_ = data;
}

template <class UV_T, class R_T, class H_T>
H_T* ns_req<UV_T, R_T, H_T>::handle() {
  return H_T::cast(static_cast<UV_T*>(this)->handle);
}

template <class UV_T, class R_T, class H_T>
void ns_req<UV_T, R_T, H_T>::handle(H_T* handle) {
  static_cast<UV_T*>(this)->handle = handle->uv_handle();
}

/* ns_connect */

template <class H_T>
template <typename CB, typename D_T>
int ns_connect<H_T>::init(const struct sockaddr* addr, CB cb, D_T* data) {
  int len = addr_size(addr);
  if (len < 0)
    return len;

  ns_req<uv_connect_t, ns_connect<H_T>, H_T>::init(cb, data);
  std::memcpy(&addr_, addr, len);

  return NSUV_OK;
}

template <class H_T>
const sockaddr* ns_connect<H_T>::sockaddr() {
  return reinterpret_cast<struct sockaddr*>(&addr_);
}


/* ns_write */

template <class H_T>
template <typename CB, typename D_T>
int ns_write<H_T>::init(
    const uv_buf_t bufs[], size_t nbufs, CB cb, D_T* data) {
  ns_req<uv_write_t, ns_write<H_T>, H_T>::init(cb, data);
  // Clear this in case it's being reused.
  bufs_.clear();
  try {
    bufs_.reserve(nbufs);
  } catch (...) {
    return UV_ENOMEM;
  }
  for (size_t i = 0; i < nbufs; i++) {
    bufs_.push_back(bufs[i]);
  }
  return NSUV_OK;
}

template <class H_T>
template <typename CB, typename D_T>
int ns_write<H_T>::init(const std::vector<uv_buf_t>& bufs,
                        CB cb,
                        D_T* data) {
  ns_req<uv_write_t, ns_write<H_T>, H_T>::init(cb, data);
  bufs_.clear();
  try {
    bufs_.reserve(bufs.size());
  } catch (...) {
    return UV_ENOMEM;
  }
  bufs_.insert(bufs_.begin(), bufs.begin(), bufs.end());
  return NSUV_OK;
}

template <class H_T>
template <typename CB, typename D_T>
int ns_write<H_T>::init(std::vector<uv_buf_t>&& bufs,
                        CB cb,
                        D_T* data) {
  ns_req<uv_write_t, ns_write<H_T>, H_T>::init(cb, data);
  bufs_ = std::move(bufs);
  return NSUV_OK;
}

template <class H_T>
std::vector<uv_buf_t>& ns_write<H_T>::bufs() {
  return bufs_;
}


/* ns_udp_send */

template <typename CB, typename D_T>
int ns_udp_send::init(const uv_buf_t bufs[],
                      size_t nbufs,
                      const struct sockaddr* addr,
                      CB cb,
                      D_T* data) {
  ns_req<uv_udp_send_t, ns_udp_send, ns_udp>::init(cb, data);
  std::vector<uv_buf_t> vbufs;
  try {
    vbufs.reserve(nbufs);
  } catch (...) {
    return UV_ENOMEM;
  }
  for (size_t i = 0; i < nbufs; i++) {
    vbufs.push_back(bufs[i]);
  }
  return init(std::move(vbufs), addr, cb, data);
}

template <typename CB, typename D_T>
int ns_udp_send::init(const std::vector<uv_buf_t>& bufs,
                      const struct sockaddr* addr,
                      CB cb,
                      D_T* data) {
  ns_req<uv_udp_send_t, ns_udp_send, ns_udp>::init(cb, data);
  bufs_.clear();
  try {
    bufs_.reserve(bufs.size());
  } catch (...) {
    return UV_ENOMEM;
  }
  bufs_.insert(bufs_.begin(), bufs.begin(), bufs.end());
  if (addr != nullptr) {
    addr_.reset(new (std::nothrow) struct sockaddr_storage());
    if (addr == nullptr)
      return UV_ENOMEM;

    int len = addr_size(addr);
    if (len < 0)
      return len;

    std::memcpy(addr_.get(), addr, len);
  }

  return NSUV_OK;
}

template <typename CB, typename D_T>
int ns_udp_send::init(std::vector<uv_buf_t>&& bufs,
                      const struct sockaddr* addr,
                      CB cb,
                      D_T* data) {
  ns_req<uv_udp_send_t, ns_udp_send, ns_udp>::init(cb, data);
  bufs_ = std::move(bufs);
  if (addr != nullptr) {
    addr_.reset(new (std::nothrow) struct sockaddr_storage());
    if (addr == nullptr)
      return UV_ENOMEM;

    int len = addr_size(addr);
    if (len < 0)
      return len;

    std::memcpy(addr_.get(), addr, len);
  }

  return NSUV_OK;
}

std::vector<uv_buf_t>& ns_udp_send::bufs() {
  return bufs_;
}

const sockaddr* ns_udp_send::sockaddr() {
  return reinterpret_cast<struct sockaddr*>(addr_.get());
}


/* ns_addrinfo */

ns_addrinfo::ns_addrinfo() {
  // Make sure to assign nullptr right away so there will be no issues with
  // the destructor.
  uv_getaddrinfo_t::addrinfo = nullptr;
}

ns_addrinfo::~ns_addrinfo() {
  // Passing nullptr to uv_freeaddrinfo is a noop
  uv_freeaddrinfo(uv_getaddrinfo_t::addrinfo);
}

template <typename D_T>
int ns_addrinfo::get(uv_loop_t* loop,
                     void (*cb)(ns_addrinfo*, int, D_T*),
                     const char* node,
                     const char* service,
                     const struct addrinfo* hints,
                     D_T* data) {
  ns_base_req<uv_getaddrinfo_t, ns_addrinfo>::init(cb, data);
  free();
  addrinfo_cb_ptr_ = reinterpret_cast<void (*)()>(cb);
  addrinfo_cb_data_ = data;

  return uv_getaddrinfo(
      loop,
      uv_req(),
      NSUV_CHECK_NULL(cb, (&addrinfo_proxy_<decltype(cb), D_T>)),
      node,
      service,
      hints);
}

int ns_addrinfo::get(uv_loop_t* loop,
                     void (*cb)(ns_addrinfo*, int),
                     const char* node,
                     const char* service,
                     const struct addrinfo* hints) {
  ns_base_req<uv_getaddrinfo_t, ns_addrinfo>::init(cb);
  free();
  addrinfo_cb_ptr_ = reinterpret_cast<void (*)()>(cb);

  return uv_getaddrinfo(loop,
                        uv_req(),
                        NSUV_CHECK_NULL(cb, (&addrinfo_proxy_<decltype(cb)>)),
                        node,
                        service,
                        hints);
}

const addrinfo* ns_addrinfo::info() {
  return uv_getaddrinfo_t::addrinfo;
}

void ns_addrinfo::free() {
  uv_freeaddrinfo(uv_getaddrinfo_t::addrinfo);
  uv_getaddrinfo_t::addrinfo = nullptr;
}

template <typename CB_T>
void ns_addrinfo::addrinfo_proxy_(uv_getaddrinfo_t* req,
                                  int status,
                                  struct addrinfo*) {
  auto* ai_req = ns_addrinfo::cast(req);
  auto* cb_ = reinterpret_cast<CB_T>(ai_req->addrinfo_cb_ptr_);
  cb_(ai_req, status);
}

template <typename CB_T, typename D_T>
void ns_addrinfo::addrinfo_proxy_(uv_getaddrinfo_t* req,
                                  int status,
                                  struct addrinfo*) {
  auto* ai_req = ns_addrinfo::cast(req);
  auto* cb_ = reinterpret_cast<CB_T>(ai_req->addrinfo_cb_ptr_);
  cb_(ai_req, status, static_cast<D_T*>(ai_req->addrinfo_cb_data_));
}


/* ns_random */

int ns_random::get(void* buf, size_t buflen, uint32_t flags) {
  return uv_random(nullptr, nullptr, buf, buflen, flags, nullptr);
}

int ns_random::get(uv_loop_t* loop,
                   void* buf,
                   size_t buflen,
                   uint32_t flags,
                   random_cb_sig cb) {
  random_cb_ptr_ = reinterpret_cast<void (*)()>(cb);

  return uv_random(loop,
                   this,
                   buf,
                   buflen,
                   flags,
                   NSUV_CHECK_NULL(cb, (&random_proxy_<decltype(cb)>)));
}

template <typename D_T>
int ns_random::get(uv_loop_t* loop,
                   void* buf,
                   size_t buflen,
                   uint32_t flags,
                   random_cb_d_sig<D_T> cb,
                   D_T* data) {
  random_cb_ptr_ = reinterpret_cast<void (*)()>(cb);
  cb_data_ = data;

  return uv_random(loop,
                   this,
                   buf,
                   buflen,
                   flags,
                   NSUV_CHECK_NULL(cb, (&random_proxy_<decltype(cb), D_T>)));
}

template <typename CB_T>
void ns_random::random_proxy_(uv_random_t* req,
                              int status,
                              void* buf,
                              size_t buflen) {
  auto* r_req = ns_random::cast(req);
  auto* cb = reinterpret_cast<CB_T>(r_req->random_cb_ptr_);
  cb(r_req, status, buf, buflen);
}

template <typename CB_T, typename D_T>
void ns_random::random_proxy_(uv_random_t* req,
                              int status,
                              void* buf,
                              size_t buflen) {
  auto* r_req = ns_random::cast(req);
  auto* cb = reinterpret_cast<CB_T>(r_req->random_cb_ptr_);
  cb(r_req, status, buf, buflen, static_cast<D_T*>(r_req->cb_data_));
}


/* ns_work */

int ns_work::queue_work(uv_loop_t* loop,
                        void (*work_cb)(ns_work*),
                        void (*after_cb)(ns_work*, int)) {
  work_cb_ptr_ = reinterpret_cast<void (*)()>(work_cb);
  after_cb_ptr_ = reinterpret_cast<void (*)()>(after_cb);

  return uv_queue_work(
      loop,
      this,
      NSUV_CHECK_NULL(work_cb, (&work_proxy_<decltype(work_cb)>)),
      NSUV_CHECK_NULL(after_cb, (&after_proxy_<decltype(after_cb)>)));
}

template <typename D_T>
int ns_work::queue_work(uv_loop_t* loop,
                        void (*work_cb)(ns_work*, D_T*),
                        void (*after_cb)(ns_work*, int, D_T*),
                        D_T* data) {
  work_cb_ptr_ = reinterpret_cast<void (*)()>(work_cb);
  after_cb_ptr_ = reinterpret_cast<void (*)()>(after_cb);
  cb_data_ = data;

  // Need a nullptr check in case someone decides to static_cast a nullptr to
  // the work_cb sig. Yes the user shouldn't do this but still need to check.
  return uv_queue_work(
      loop,
      this,
      NSUV_CHECK_NULL(work_cb, (&work_proxy_<decltype(work_cb), D_T>)),
      NSUV_CHECK_NULL(after_cb, (&after_proxy_<decltype(after_cb), D_T>)));
}

int ns_work::queue_work(uv_loop_t* loop, void (*work_cb)(ns_work*)) {
  work_cb_ptr_ = reinterpret_cast<void (*)()>(work_cb);

  return uv_queue_work(
      loop,
      this,
      NSUV_CHECK_NULL(work_cb, (&work_proxy_<decltype(work_cb)>)),
      nullptr);
}

template <typename D_T>
int ns_work::queue_work(uv_loop_t* loop,
                        void (*work_cb)(ns_work*, D_T*),
                        D_T* data) {
  work_cb_ptr_ = reinterpret_cast<void (*)()>(work_cb);
  cb_data_ = data;

  return uv_queue_work(
      loop,
      this,
      NSUV_CHECK_NULL(work_cb, (&work_proxy_<decltype(work_cb), D_T>)),
      nullptr);
}

template <typename CB_T>
void ns_work::work_proxy_(uv_work_t* req) {
  auto* w_req = ns_work::cast(req);
  auto* cb = reinterpret_cast<CB_T>(w_req->work_cb_ptr_);
  cb(w_req);
}

template <typename CB_T>
void ns_work::after_proxy_(uv_work_t* req, int status) {
  auto* w_req = ns_work::cast(req);
  auto* cb = reinterpret_cast<CB_T>(w_req->after_cb_ptr_);
  cb(w_req, status);
}

template <typename CB_T, typename D_T>
void ns_work::work_proxy_(uv_work_t* req) {
  auto* w_req = ns_work::cast(req);
  auto* cb = reinterpret_cast<CB_T>(w_req->work_cb_ptr_);
  cb(w_req, static_cast<D_T*>(w_req->cb_data_));
}

template <typename CB_T, typename D_T>
void ns_work::after_proxy_(uv_work_t* req, int status) {
  auto* w_req = ns_work::cast(req);
  auto* cb = reinterpret_cast<CB_T>(w_req->after_cb_ptr_);
  cb(w_req, status, static_cast<D_T*>(w_req->cb_data_));
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
#  define XX(uc, lc)                                                           \
    case UV_##uc:                                                              \
      return #lc;
    UV_HANDLE_TYPE_MAP(XX)
#  undef XX
    case UV_FILE:
      return "file";
    case UV_HANDLE_TYPE_MAX:
    case UV_UNKNOWN_HANDLE:
      return nullptr;
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
void ns_handle<UV_T, H_T>::close(void (*cb)(H_T*)) {
  close_cb_ptr_ = reinterpret_cast<void (*)()>(cb);
  uv_close(base_handle(),
           NSUV_CHECK_NULL(cb, (&close_proxy_<decltype(cb)>)));
}

template <class UV_T, class H_T>
template <typename D_T>
void ns_handle<UV_T, H_T>::close(void (*cb)(H_T*, D_T*), D_T* data) {
  close_cb_ptr_ = reinterpret_cast<void (*)()>(cb);
  close_cb_data_ = data;
  uv_close(base_handle(),
           NSUV_CHECK_NULL(cb, (&close_proxy_<decltype(cb), D_T>)));
}

template <class UV_T, class H_T>
void ns_handle<UV_T, H_T>::close(void (*cb)(H_T*, void*), std::nullptr_t) {
  return close(cb, NSUV_CAST_NULLPTR);
}

template <class UV_T, class H_T>
void ns_handle<UV_T, H_T>::close_and_delete() {
  if (get_type() == UV_UNKNOWN_HANDLE) {
    delete H_T::cast(base_handle());
  } else {
    uv_close(base_handle(), close_delete_cb_);
  }
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
template <typename CB_T>
void ns_handle<UV_T, H_T>::close_proxy_(uv_handle_t* handle) {
  H_T* wrap = H_T::cast(handle);
  auto* cb_ = reinterpret_cast<CB_T>(wrap->close_cb_ptr_);
  cb_(wrap);
}

template <class UV_T, class H_T>
template <typename CB_T, typename D_T>
void ns_handle<UV_T, H_T>::close_proxy_(uv_handle_t* handle) {
  H_T* wrap = H_T::cast(handle);
  auto* cb_ = reinterpret_cast<CB_T>(wrap->close_cb_ptr_);
  cb_(wrap, static_cast<D_T*>(wrap->close_cb_data_));
}

template <class UV_T, class H_T>
void ns_handle<UV_T, H_T>::close_delete_cb_(uv_handle_t* handle) {
  delete H_T::cast(handle);
}


/* ns_stream */

template <class UV_T, class H_T>
uv_stream_t* ns_stream<UV_T, H_T>::base_stream() {
  return reinterpret_cast<uv_stream_t*>(this->uv_handle());
}

template <class UV_T, class H_T>
int ns_stream<UV_T, H_T>::listen(int backlog, void (*cb)(H_T*, int)) {
  listen_cb_ptr_ = reinterpret_cast<void (*)()>(cb);

  return uv_listen(base_stream(),
                   backlog,
                   NSUV_CHECK_NULL(cb, (&listen_proxy_<decltype(cb)>)));
}

template <class UV_T, class H_T>
template <typename D_T>
int ns_stream<UV_T, H_T>::listen(int backlog,
                                 void (*cb)(H_T*, int, D_T*),
                                 D_T* data) {
  listen_cb_ptr_ = reinterpret_cast<void (*)()>(cb);
  listen_cb_data_ = data;

  return uv_listen(base_stream(),
                   backlog,
                   NSUV_CHECK_NULL(cb, (&listen_proxy_<decltype(cb), D_T>)));
}

template <class UV_T, class H_T>
int ns_stream<UV_T, H_T>::listen(int backlog,
                                 void (*cb)(H_T*, int, void*),
                                 std::nullptr_t) {
  return listen(backlog, cb, NSUV_CAST_NULLPTR);
}

template <class UV_T, class H_T>
int ns_stream<UV_T, H_T>::write(ns_write<H_T>* req,
                                const uv_buf_t bufs[],
                                size_t nbufs,
                                void (*cb)(ns_write<H_T>*, int)) {
  int ret = req->init(bufs, nbufs, cb);
  if (ret != NSUV_OK)
    return ret;

  return uv_write(req->uv_req(),
                  base_stream(),
                  req->bufs().data(),
                  req->bufs().size(),
                  NSUV_CHECK_NULL(cb, (&write_proxy_<decltype(cb)>)));
}

template <class UV_T, class H_T>
int ns_stream<UV_T, H_T>::write(ns_write<H_T>* req,
                                const std::vector<uv_buf_t>& bufs,
                                void (*cb)(ns_write<H_T>*, int)) {
  int ret = req->init(bufs, cb);
  if (ret != NSUV_OK)
    return ret;

  return uv_write(req->uv_req(),
                  base_stream(),
                  req->bufs().data(),
                  req->bufs().size(),
                  NSUV_CHECK_NULL(cb, (&write_proxy_<decltype(cb)>)));
}

template <class UV_T, class H_T>
template <typename D_T>
int ns_stream<UV_T, H_T>::write(ns_write<H_T>* req,
                                const uv_buf_t bufs[],
                                size_t nbufs,
                                void (*cb)(ns_write<H_T>*, int, D_T*),
                                D_T* data) {
  int ret = req->init(bufs, nbufs, cb, data);
  if (ret != NSUV_OK)
    return ret;

  return uv_write(req->uv_req(),
                  base_stream(),
                  req->bufs().data(),
                  req->bufs().size(),
                  NSUV_CHECK_NULL(cb, (&write_proxy_<decltype(cb), D_T>)));
}

template <class UV_T, class H_T>
int ns_stream<UV_T, H_T>::write(ns_write<H_T>* req,
                                const uv_buf_t bufs[],
                                size_t nbufs,
                                void (*cb)(ns_write<H_T>*, int, void*),
                                std::nullptr_t) {
  return write(req, bufs, nbufs, cb, NSUV_CAST_NULLPTR);
}

template <class UV_T, class H_T>
template <typename D_T>
int ns_stream<UV_T, H_T>::write(ns_write<H_T>* req,
                                const std::vector<uv_buf_t>& bufs,
                                void (*cb)(ns_write<H_T>*, int, D_T*),
                                D_T* data) {
  int ret = req->init(bufs, cb, data);
  if (ret != NSUV_OK)
    return ret;

  return uv_write(req->uv_req(),
                  base_stream(),
                  req->bufs().data(),
                  req->bufs().size(),
                  NSUV_CHECK_NULL(cb, (&write_proxy_<decltype(cb), D_T>)));
}

template <class UV_T, class H_T>
int ns_stream<UV_T, H_T>::write(ns_write<H_T>* req,
                                const std::vector<uv_buf_t>& bufs,
                                void (*cb)(ns_write<H_T>*, int, void*),
                                std::nullptr_t) {
  return write(req, bufs, cb, NSUV_CAST_NULLPTR);
}

template <class UV_T, class H_T>
template <typename CB_T>
void ns_stream<UV_T, H_T>::listen_proxy_(uv_stream_t* handle, int status) {
  auto* server = H_T::cast(handle);
  auto* cb_ = reinterpret_cast<CB_T>(server->listen_cb_ptr_);
  cb_(server, status);
}

template <class UV_T, class H_T>
template <typename CB_T, typename D_T>
void ns_stream<UV_T, H_T>::listen_proxy_(uv_stream_t* handle, int status) {
  auto* server = H_T::cast(handle);
  auto* cb_ = reinterpret_cast<CB_T>(server->listen_cb_ptr_);
  cb_(server, status, static_cast<D_T*>(server->listen_cb_data_));
}

template <class UV_T, class H_T>
template <typename CB_T>
void ns_stream<UV_T, H_T>::write_proxy_(uv_write_t* uv_req, int status) {
  auto* wreq = ns_write<H_T>::cast(uv_req);
  auto* cb_ = reinterpret_cast<CB_T>(wreq->req_cb_);
  cb_(wreq, status);
}

template <class UV_T, class H_T>
template <typename CB_T, typename D_T>
void ns_stream<UV_T, H_T>::write_proxy_(uv_write_t* uv_req, int status) {
  auto* wreq = ns_write<H_T>::cast(uv_req);
  auto* cb_ = reinterpret_cast<CB_T>(wreq->req_cb_);
  cb_(wreq, status, static_cast<D_T*>(wreq->req_cb_data_));
}


/* ns_async */

int ns_async::init(uv_loop_t* loop, void (*cb)(ns_async*)) {
  async_cb_ptr_ = reinterpret_cast<void (*)()>(cb);
  async_cb_data_ = nullptr;

  return uv_async_init(loop,
                       uv_handle(),
                       NSUV_CHECK_NULL(cb, (&async_proxy_<decltype(cb)>)));
}

template <typename D_T>
int ns_async::init(uv_loop_t* loop, void (*cb)(ns_async*, D_T*), D_T* data) {
  async_cb_ptr_ = reinterpret_cast<void (*)()>(cb);
  async_cb_data_ = data;

  return uv_async_init(loop,
                       uv_handle(),
                       NSUV_CHECK_NULL(cb, (&async_proxy_<decltype(cb), D_T>)));
}

int ns_async::init(uv_loop_t* loop,
                   void (*cb)(ns_async*, void*),
                   std::nullptr_t) {
  return init(loop, cb, NSUV_CAST_NULLPTR);
}

int ns_async::send() {
  return uv_async_send(uv_handle());
}

template <typename CB_T>
void ns_async::async_proxy_(uv_async_t* handle) {
  ns_async* wrap = ns_async::cast(handle);
  auto* cb_ = reinterpret_cast<CB_T>(wrap->async_cb_ptr_);
  cb_(wrap);
}

template <typename CB_T, typename D_T>
void ns_async::async_proxy_(uv_async_t* handle) {
  auto* wrap = ns_async::cast(handle);
  auto* cb_ = reinterpret_cast<CB_T>(wrap->async_cb_ptr_);
  cb_(wrap, static_cast<D_T*>(wrap->async_cb_data_));
}


/* ns_poll */

int ns_poll::init(uv_loop_t* loop, int fd) {
  poll_cb_ptr_ = nullptr;
  poll_cb_data_ = nullptr;
  return uv_poll_init(loop, uv_handle(), fd);
}

int ns_poll::init_socket(uv_loop_t* loop, uv_os_sock_t socket) {
  poll_cb_ptr_ = nullptr;
  poll_cb_data_ = nullptr;
  return uv_poll_init_socket(loop, uv_handle(), socket);
}

int ns_poll::start(int events, void (*cb)(ns_poll*, int, int)) {
  poll_cb_ptr_ = reinterpret_cast<void (*)()>(cb);

  return uv_poll_start(uv_handle(),
                       events,
                       NSUV_CHECK_NULL(cb, (&poll_proxy_<decltype(cb)>)));
}

template <typename D_T>
int ns_poll::start(int events,
                   void (*cb)(ns_poll*, int, int, D_T*),
                   D_T* data) {
  poll_cb_ptr_ = reinterpret_cast<void (*)()>(cb);
  poll_cb_data_ = data;

  return uv_poll_start(uv_handle(),
                       events,
                       NSUV_CHECK_NULL(cb, (&poll_proxy_<decltype(cb), D_T>)));
}

int ns_poll::start(int events,
                   void (*cb)(ns_poll*, int, int, void*),
                   std::nullptr_t) {
  return start(events, cb, NSUV_CAST_NULLPTR);
}

int ns_poll::stop() {
  return uv_poll_stop(uv_handle());
}

template <typename CB_T>
void ns_poll::poll_proxy_(uv_poll_t* handle, int poll, int events) {
  ns_poll* wrap = ns_poll::cast(handle);
  auto* cb_ = reinterpret_cast<CB_T>(wrap->poll_cb_ptr_);
  cb_(wrap, poll, events);
}

template <typename CB_T, typename D_T>
void ns_poll::poll_proxy_(uv_poll_t* handle, int poll, int events) {
  ns_poll* wrap = ns_poll::cast(handle);
  auto* cb_ = reinterpret_cast<CB_T>(wrap->poll_cb_ptr_);
  cb_(wrap, poll, events, static_cast<D_T*>(wrap->poll_cb_data_));
}


/* ns_tcp */

int ns_tcp::init(uv_loop_t* loop) {
  return uv_tcp_init(loop, uv_handle());
}

int ns_tcp::init_ex(uv_loop_t* loop, unsigned int flags) {
  return uv_tcp_init_ex(loop, uv_handle(), flags);
}

int ns_tcp::open(uv_os_sock_t sock) {
  return uv_tcp_open(uv_handle(), sock);
}

int ns_tcp::nodelay(bool enable) {
  return uv_tcp_nodelay(uv_handle(), enable);
}

int ns_tcp::keepalive(bool enable, int delay) {
  return uv_tcp_keepalive(uv_handle(), enable, delay);
}

int ns_tcp::simultaneous_accepts(bool enable) {
  return uv_tcp_simultaneous_accepts(uv_handle(), enable);
}

int ns_tcp::bind(const struct sockaddr* addr, unsigned int flags) {
  return uv_tcp_bind(uv_handle(), addr, flags);
}

int ns_tcp::getsockname(struct sockaddr* name, int* namelen) {
  return uv_tcp_getsockname(uv_handle(), name, namelen);
}

int ns_tcp::getpeername(struct sockaddr* name, int* namelen) {
  return uv_tcp_getpeername(uv_handle(), name, namelen);
}

int ns_tcp::close_reset(void (*cb)(ns_tcp*)) {
  close_reset_cb_ptr_ = reinterpret_cast<void (*)()>(cb);
  return uv_tcp_close_reset(uv_handle(), &close_reset_proxy_<decltype(cb)>);
}

template <typename D_T>
int ns_tcp::close_reset(void (*cb)(ns_tcp*, D_T*), D_T* data) {
  close_reset_cb_ptr_ = reinterpret_cast<void (*)()>(cb);
  close_reset_data_ = data;
  return uv_tcp_close_reset(uv_handle(),
                            &close_reset_proxy_<decltype(cb), D_T>);
}

int ns_tcp::close_reset(void (*cb)(ns_tcp*, void*), std::nullptr_t) {
  return close_reset(cb, NSUV_CAST_NULLPTR);
}

int ns_tcp::connect(ns_connect<ns_tcp>* req,
                    const struct sockaddr* addr,
                    void (*cb)(ns_connect<ns_tcp>*, int)) {
  int ret = req->init(addr, cb);
  if (ret != NSUV_OK)
    return ret;

  return uv_tcp_connect(
      req->uv_req(),
      uv_handle(),
      addr,
      NSUV_CHECK_NULL(cb, (&connect_proxy_<decltype(cb)>)));
}

template <typename D_T>
int ns_tcp::connect(ns_connect<ns_tcp>* req,
                    const struct sockaddr* addr,
                    void (*cb)(ns_connect<ns_tcp>*, int, D_T*),
                    D_T* data) {
  int ret = req->init(addr, cb, data);
  if (ret != NSUV_OK)
    return ret;

  return uv_tcp_connect(
      req->uv_req(),
      uv_handle(),
      addr,
      NSUV_CHECK_NULL(cb, (&connect_proxy_<decltype(cb), D_T>)));
}

int ns_tcp::connect(ns_connect<ns_tcp>* req,
                    const struct sockaddr* addr,
                    void (*cb)(ns_connect<ns_tcp>*, int, void*),
                    std::nullptr_t) {
  return connect(req, addr, cb, NSUV_CAST_NULLPTR);
}

template <typename CB_T>
void ns_tcp::connect_proxy_(uv_connect_t* uv_req, int status) {
  auto* creq = ns_connect<ns_tcp>::cast(uv_req);
  auto* cb_ = reinterpret_cast<CB_T>(creq->req_cb_);
  cb_(creq, status);
}

template <typename CB_T, typename D_T>
void ns_tcp::connect_proxy_(uv_connect_t* uv_req, int status) {
  auto* creq = ns_connect<ns_tcp>::cast(uv_req);
  auto* cb_ = reinterpret_cast<CB_T>(creq->req_cb_);
  cb_(creq, status, static_cast<D_T*>(creq->req_cb_data_));
}

template <typename CB_T>
void ns_tcp::close_reset_proxy_(uv_handle_t* handle) {
  ns_tcp* wrap = ns_tcp::cast(handle);
  auto* cb = reinterpret_cast<CB_T>(wrap->close_reset_cb_ptr_);
  cb(wrap);
}

template <typename CB_T, typename D_T>
void ns_tcp::close_reset_proxy_(uv_handle_t* handle) {
  ns_tcp* wrap = ns_tcp::cast(handle);
  auto* cb = reinterpret_cast<CB_T>(wrap->close_reset_cb_ptr_);
  cb(wrap, static_cast<D_T*>(wrap->close_reset_data_));
}


/* ns_timer */

int ns_timer::init(uv_loop_t* loop) {
  timer_cb_ptr_ = nullptr;
  timer_cb_data_ = nullptr;
  return uv_timer_init(loop, uv_handle());
}

int ns_timer::start(void (*cb)(ns_timer*), uint64_t timeout, uint64_t repeat) {
  timer_cb_ptr_ = reinterpret_cast<void (*)()>(cb);

  return uv_timer_start(uv_handle(),
                        NSUV_CHECK_NULL(cb, (&timer_proxy_<decltype(cb)>)),
                        timeout,
                        repeat);
}

template <typename D_T>
int ns_timer::start(void (*cb)(ns_timer*, D_T*),
                    uint64_t timeout,
                    uint64_t repeat,
                    D_T* data) {
  timer_cb_ptr_ = reinterpret_cast<void (*)()>(cb);
  timer_cb_data_ = data;

  return uv_timer_start(uv_handle(),
                        NSUV_CHECK_NULL(cb, (&timer_proxy_<decltype(cb), D_T>)),
                        timeout,
                        repeat);
}

int ns_timer::start(void (*cb)(ns_timer*, void*),
                    uint64_t timeout,
                    uint64_t repeat,
                    std::nullptr_t) {
  return start(cb, timeout, repeat, NSUV_CAST_NULLPTR);
}

int ns_timer::stop() {
  return uv_timer_stop(uv_handle());
}

size_t ns_timer::get_repeat() {
  return uv_timer_get_repeat(uv_handle());
}

template <typename CB_T>
void ns_timer::timer_proxy_(uv_timer_t* handle) {
  ns_timer* wrap = ns_timer::cast(handle);
  auto* cb_ = reinterpret_cast<CB_T>(wrap->timer_cb_ptr_);
  cb_(wrap);
}

template <typename CB_T, typename D_T>
void ns_timer::timer_proxy_(uv_timer_t* handle) {
  ns_timer* wrap = ns_timer::cast(handle);
  auto* cb_ = reinterpret_cast<CB_T>(wrap->timer_cb_ptr_);
  cb_(wrap, static_cast<D_T*>(wrap->timer_cb_data_));
}


/* ns_check, ns_idle, ns_prepare */

#define NSUV_LOOP_WATCHER_DEFINE(name)                                         \
  int ns_##name::init(uv_loop_t* loop) {                                       \
    name##_cb_ptr_ = nullptr;                                                  \
    name##_cb_data_ = nullptr;                                                 \
    return uv_##name##_init(loop, uv_handle());                                \
  }                                                                            \
                                                                               \
  int ns_##name::start(void (*cb)(ns_##name*)) {                               \
    if (is_active())                                                           \
      return 0;                                                                \
    name##_cb_ptr_ = reinterpret_cast<void (*)()>(cb);                         \
    return uv_##name##_start(                                                  \
        uv_handle(),                                                           \
        NSUV_CHECK_NULL(cb, (&name##_proxy_<decltype(cb)>)));                  \
  }                                                                            \
                                                                               \
  template <typename D_T>                                                      \
  int ns_##name::start(void (*cb)(ns_##name*, D_T*), D_T* data) {              \
    if (is_active())                                                           \
      return 0;                                                                \
    name##_cb_ptr_ = reinterpret_cast<void (*)()>(cb);                         \
    name##_cb_data_ = data;                                                    \
    return uv_##name##_start(                                                  \
        uv_handle(),                                                           \
        NSUV_CHECK_NULL(cb, (&name##_proxy_<decltype(cb), D_T>)));             \
  }                                                                            \
                                                                               \
  int ns_##name::start(void (*cb)(ns_##name*, void*), std::nullptr_t) {        \
    return start(cb, NSUV_CAST_NULLPTR);                                       \
  }                                                                            \
                                                                               \
  int ns_##name::stop() { return uv_##name##_stop(uv_handle()); }              \
                                                                               \
  template <typename CB_T>                                                     \
  void ns_##name::name##_proxy_(uv_##name##_t* handle) {                       \
    ns_##name* wrap = ns_##name::cast(handle);                                 \
    auto* cb_ = reinterpret_cast<CB_T>(wrap->name##_cb_ptr_);                  \
    cb_(wrap);                                                                 \
  }                                                                            \
                                                                               \
  template <typename CB_T, typename D_T>                                       \
  void ns_##name::name##_proxy_(uv_##name##_t* handle) {                       \
    ns_##name* wrap = ns_##name::cast(handle);                                 \
    auto* cb_ = reinterpret_cast<CB_T>(wrap->name##_cb_ptr_);                  \
    cb_(wrap, static_cast<D_T*>(wrap->name##_cb_data_));                       \
  }


NSUV_LOOP_WATCHER_DEFINE(check)
NSUV_LOOP_WATCHER_DEFINE(idle)
NSUV_LOOP_WATCHER_DEFINE(prepare)

#undef NSUV_LOOP_WATCHER_DEFINE


/* ns_udp */

int ns_udp::init(uv_loop_t* loop) {
  return uv_udp_init(loop, uv_handle());
}

int ns_udp::init_ex(uv_loop_t* loop, unsigned int flags) {
  return uv_udp_init_ex(loop, uv_handle(), flags);
}

int ns_udp::bind(const struct sockaddr* addr, unsigned int flags) {
  int r = uv_udp_bind(uv_handle(), addr, flags);
  if (r == 0) {
    if (addr == nullptr) {
      local_addr_.reset(nullptr);
    } else {
      local_addr_.reset(new (std::nothrow) struct sockaddr_storage());
      if (local_addr_ == nullptr)
        return UV_ENOMEM;

      int len = addr_size(addr);
      if (len < 0)
        return len;

      std::memcpy(local_addr_.get(), addr, len);
    }
  }

  return r;
}

int ns_udp::connect(const struct sockaddr* addr) {
  int r = uv_udp_connect(uv_handle(), addr);
  if (r == 0) {
    if (addr == nullptr) {
      remote_addr_.reset(nullptr);
    } else {
      remote_addr_.reset(new (std::nothrow) struct sockaddr_storage());
      if (remote_addr_ == nullptr)
        return UV_ENOMEM;

      int len = addr_size(addr);
      if (len < 0)
        return len;

      std::memcpy(remote_addr_.get(), addr, len);
    }
  }

  return r;
}

int ns_udp::getpeername(struct sockaddr* name, int* namelen) {
  return uv_udp_getpeername(uv_handle(), name, namelen);
}

int ns_udp::getsockname(struct sockaddr* name, int* namelen) {
  return uv_udp_getsockname(uv_handle(), name, namelen);
}

int ns_udp::try_send(const uv_buf_t bufs[],
                     size_t nbufs,
                     const struct sockaddr* addr) {
  return uv_udp_try_send(uv_handle(), bufs, nbufs, addr);
}

int ns_udp::try_send(const std::vector<uv_buf_t>& bufs,
                     const struct sockaddr* addr) {
  return uv_udp_try_send(uv_handle(), bufs.data(), bufs.size(), addr);
}

int ns_udp::send(ns_udp_send* req,
                 const uv_buf_t bufs[],
                 size_t nbufs,
                 const struct sockaddr* addr) {
  int r = req->init(bufs, nbufs, addr, NSUV_CAST_NULLPTR);
  if (r != 0)
    return r;

  return uv_udp_send(req->uv_req(),
                     uv_handle(),
                     req->bufs().data(),
                     req->bufs().size(),
                     addr,
                     nullptr);
}

int ns_udp::send(ns_udp_send* req,
                 const std::vector<uv_buf_t>& bufs,
                 const struct sockaddr* addr) {
  int r = req->init(bufs, addr, NSUV_CAST_NULLPTR);
  if (r != 0)
    return r;

  return uv_udp_send(req->uv_req(),
                     uv_handle(),
                     req->bufs().data(),
                     req->bufs().size(),
                     addr,
                     nullptr);
}

int ns_udp::send(ns_udp_send* req,
                 const uv_buf_t bufs[],
                 size_t nbufs,
                 const struct sockaddr* addr,
                 void (*cb)(ns_udp_send*, int)) {
  int r = req->init(bufs, nbufs, addr, cb);
  if (r != 0)
    return r;

  return uv_udp_send(req->uv_req(),
                     uv_handle(),
                     req->bufs().data(),
                     req->bufs().size(),
                     addr,
                     NSUV_CHECK_NULL(cb, (&send_proxy_<decltype(cb)>)));
}

int ns_udp::send(ns_udp_send* req,
                 const std::vector<uv_buf_t>& bufs,
                 const struct sockaddr* addr,
                 void (*cb)(ns_udp_send*, int)) {
  int r = req->init(bufs, addr, cb);
  if (r != 0)
    return r;

  return uv_udp_send(req->uv_req(),
                     uv_handle(),
                     req->bufs().data(),
                     req->bufs().size(),
                     addr,
                     NSUV_CHECK_NULL(cb, (&send_proxy_<decltype(cb)>)));
}

template <typename D_T>
int ns_udp::send(ns_udp_send* req,
                 const uv_buf_t bufs[],
                 size_t nbufs,
                 const struct sockaddr* addr,
                 void (*cb)(ns_udp_send*, int, D_T*),
                 D_T* data) {
  int r = req->init(bufs, nbufs, addr, cb, data);
  if (r != 0)
    return r;

  return uv_udp_send(req->uv_req(),
                     uv_handle(),
                     req->bufs().data(),
                     req->bufs().size(),
                     addr,
                     NSUV_CHECK_NULL(cb, (&send_proxy_<decltype(cb), D_T>)));
}

int ns_udp::send(ns_udp_send* req,
                 const uv_buf_t bufs[],
                 size_t nbufs,
                 const struct sockaddr* addr,
                 void (*cb)(ns_udp_send*, int, void*),
                 std::nullptr_t) {
  return send(req, bufs, nbufs, addr, cb, NSUV_CAST_NULLPTR);
}

template <typename D_T>
int ns_udp::send(ns_udp_send* req,
                 const std::vector<uv_buf_t>& bufs,
                 const struct sockaddr* addr,
                 void (*cb)(ns_udp_send*, int, D_T*),
                 D_T* data) {
  int r = req->init(bufs, addr, cb, data);
  if (r != 0)
    return r;

  return uv_udp_send(req->uv_req(),
                     uv_handle(),
                     req->bufs().data(),
                     req->bufs().size(),
                     addr,
                     NSUV_CHECK_NULL(cb, (&send_proxy_<decltype(cb), D_T>)));
}

int ns_udp::send(ns_udp_send* req,
                 const std::vector<uv_buf_t>& bufs,
                 const struct sockaddr* addr,
                 void (*cb)(ns_udp_send*, int, void*),
                 std::nullptr_t) {
  return send(req, bufs, addr, cb, NSUV_CAST_NULLPTR);
}

const sockaddr* ns_udp::local_addr() {
  if (local_addr_ == nullptr) {
    local_addr_.reset(new (std::nothrow) struct sockaddr_storage());
    if (local_addr_ == nullptr)
      return nullptr;

    int len = sizeof(struct sockaddr_storage);
    int r = getsockname(
        reinterpret_cast<struct sockaddr*>(local_addr_.get()),
        &len);
    if (r != 0)
      local_addr_.reset(nullptr);
  }

  return reinterpret_cast<struct sockaddr*>(local_addr_.get());
}

const sockaddr* ns_udp::remote_addr() {
  return reinterpret_cast<struct sockaddr*>(remote_addr_.get());
}

template <typename CB_T>
void ns_udp::send_proxy_(uv_udp_send_t* uv_req, int status) {
  auto* ureq = ns_udp_send::cast(uv_req);
  auto* cb_ = reinterpret_cast<CB_T>(ureq->req_cb_);
  cb_(ureq, status);
}

template <typename CB_T, typename D_T>
void ns_udp::send_proxy_(uv_udp_send_t* uv_req, int status) {
  auto* ureq = ns_udp_send::cast(uv_req);
  auto* cb_ = reinterpret_cast<CB_T>(ureq->req_cb_);
  cb_(ureq, status, static_cast<D_T*>(ureq->req_cb_data_));
}


/* ns_mutex */

ns_mutex::ns_mutex(int* er, bool recursive) : auto_destruct_(true) {
  *er = recursive ? init_recursive() : init();
}

ns_mutex::~ns_mutex() {
  if (auto_destruct_)
    destroy();
}

int ns_mutex::init(bool ad) {
  auto_destruct_ = ad;
  destroyed_ = false;
  return uv_mutex_init(&mutex_);
}

int ns_mutex::init_recursive(bool ad) {
  auto_destruct_ = ad;
  destroyed_ = false;
  return uv_mutex_init_recursive(&mutex_);
}

void ns_mutex::destroy() {
  destroyed_ = true;
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

bool ns_mutex::destroyed() {
  return destroyed_;
}

uv_mutex_t* ns_mutex::base() {
  return &mutex_;
}

ns_mutex::scoped_lock::scoped_lock(ns_mutex* mutex) : ns_mutex_(*mutex) {
  uv_mutex_lock(&ns_mutex_.mutex_);
}

ns_mutex::scoped_lock::scoped_lock(const ns_mutex& mutex) : ns_mutex_(mutex) {
  uv_mutex_lock(&ns_mutex_.mutex_);
}

ns_mutex::scoped_lock::~scoped_lock() {
  uv_mutex_unlock(&ns_mutex_.mutex_);
}

/* ns_rwlock */

ns_rwlock::ns_rwlock(int* er) : auto_destruct_(true) {
  *er = init();
}

ns_rwlock::~ns_rwlock() {
  if (auto_destruct_)
    destroy();
}

int ns_rwlock::init(bool ad) {
  auto_destruct_ = ad;
  destroyed_ = false;
  return uv_rwlock_init(&lock_);
}

void ns_rwlock::destroy() {
  destroyed_ = true;
  uv_rwlock_destroy(&lock_);
}

void ns_rwlock::rdlock() {
  uv_rwlock_rdlock(&lock_);
}

int ns_rwlock::tryrdlock() {
  return uv_rwlock_tryrdlock(&lock_);
}

void ns_rwlock::rdunlock() {
  uv_rwlock_rdunlock(&lock_);
}

void ns_rwlock::wrlock() {
  uv_rwlock_wrlock(&lock_);
}

int ns_rwlock::trywrlock() {
  return uv_rwlock_trywrlock(&lock_);
}

void ns_rwlock::wrunlock() {
  uv_rwlock_wrunlock(&lock_);
}

bool ns_rwlock::destroyed() {
  return destroyed_;
}

uv_rwlock_t* ns_rwlock::base() {
  return &lock_;
}

ns_rwlock::scoped_rdlock::scoped_rdlock(ns_rwlock* lock) : ns_rwlock_(*lock) {
  uv_rwlock_rdlock(&ns_rwlock_.lock_);
}

ns_rwlock::scoped_rdlock::scoped_rdlock(const ns_rwlock& lock) :
    ns_rwlock_(lock) {
  uv_rwlock_rdlock(&ns_rwlock_.lock_);
}

ns_rwlock::scoped_rdlock::~scoped_rdlock() {
  uv_rwlock_rdunlock(&ns_rwlock_.lock_);
}

ns_rwlock::scoped_wrlock::scoped_wrlock(ns_rwlock* lock) : ns_rwlock_(*lock) {
  uv_rwlock_wrlock(&ns_rwlock_.lock_);
}

ns_rwlock::scoped_wrlock::scoped_wrlock(const ns_rwlock& lock) :
    ns_rwlock_(lock) {
  uv_rwlock_wrlock(&ns_rwlock_.lock_);
}

ns_rwlock::scoped_wrlock::~scoped_wrlock() {
  uv_rwlock_wrunlock(&ns_rwlock_.lock_);
}


/* ns_thread */

int ns_thread::create(void (*cb)(ns_thread*)) {
  thread_cb_ptr_ = reinterpret_cast<void (*)()>(cb);

  return uv_thread_create(&thread_,
                          NSUV_CHECK_NULL(cb, (&create_proxy_<decltype(cb)>)),
                          this);
}

template <typename D_T>
int ns_thread::create(void (*cb)(ns_thread*, D_T*), D_T* data) {
  thread_cb_ptr_ = reinterpret_cast<void (*)()>(cb);
  thread_cb_data_ = data;

  return uv_thread_create(
      &thread_,
      NSUV_CHECK_NULL(cb, (&create_proxy_<decltype(cb), D_T>)),
      this);
}

int ns_thread::create(void (*cb)(ns_thread*, void*), std::nullptr_t) {
  return create(cb, NSUV_CAST_NULLPTR);
}

int ns_thread::create_ex(const uv_thread_options_t* params,
                         void (*cb)(ns_thread*)) {
  thread_cb_ptr_ = reinterpret_cast<void (*)()>(cb);

  return uv_thread_create_ex(
      &thread_,
      params,
      NSUV_CHECK_NULL(cb, (&create_proxy_<decltype(cb)>)),
      this);
}

template <typename D_T>
int ns_thread::create_ex(const uv_thread_options_t* params,
                         void (*cb)(ns_thread*, D_T*),
                         D_T* data) {
  thread_cb_ptr_ = reinterpret_cast<void (*)()>(cb);
  thread_cb_data_ = data;

  return uv_thread_create_ex(
      &thread_,
      params,
      NSUV_CHECK_NULL(cb, (&create_proxy_<decltype(cb), D_T>)),
      this);
}

int ns_thread::create_ex(const uv_thread_options_t* params,
                         void (*cb)(ns_thread*, void*),
                         std::nullptr_t) {
  return create_ex(params, cb, NSUV_CAST_NULLPTR);
}

int ns_thread::join() {
  return uv_thread_join(&thread_);
}

uv_thread_t ns_thread::base() {
  return thread_;
}

bool ns_thread::equal(uv_thread_t* t2) {
  return uv_thread_equal(&thread_, t2) == 0;
}

bool ns_thread::equal(uv_thread_t&& t2) {
  return uv_thread_equal(&thread_, &t2) == 0;
}

bool ns_thread::equal(ns_thread* t2) {
  return t2->equal(&thread_) == 0;
}

bool ns_thread::equal(ns_thread&& t2) {
  return t2.equal(&thread_) == 0;
}

bool ns_thread::equal(const uv_thread_t& t1, const uv_thread_t& t2) {
  return uv_thread_equal(&t1, &t2) == 0;
}

bool ns_thread::equal(uv_thread_t&& t1, uv_thread_t&& t2) {
  return uv_thread_equal(&t1, &t2) == 0;
}

uv_thread_t ns_thread::self() {
  return uv_thread_self();
}

template <typename CB_T>
void ns_thread::create_proxy_(void* arg) {
  auto* wrap = static_cast<ns_thread*>(arg);
  auto* cb_ = reinterpret_cast<CB_T>(wrap->thread_cb_ptr_);
  cb_(wrap);
}

template <typename CB_T, typename D_T>
void ns_thread::create_proxy_(void* arg) {
  auto* wrap = static_cast<ns_thread*>(arg);
  auto* cb_ = reinterpret_cast<CB_T>(wrap->thread_cb_ptr_);
  cb_(wrap, static_cast<D_T*>(wrap->thread_cb_data_));
}

int util::addr_size(const struct sockaddr* addr) {
  if (addr == nullptr) {
    return 0;
  }

  int len;
  if (addr->sa_family == AF_INET) {
    len = sizeof(struct sockaddr_in);
  } else if (addr->sa_family == AF_INET6) {
    len = sizeof(struct sockaddr_in6);
  } else if (addr->sa_family == AF_UNIX) {
    len = sizeof(struct sockaddr_un);
  } else {
    return UV_EINVAL;
  }

  return len;
}

#undef NSUV_CHECK_NULL
#undef NSUV_CAST_NULLPTR

}  // namespace nsuv

#endif  // INCLUDE_NSUV_INL_H_
