#ifndef NSUV_H_
#define NSUV_H_

#include "uv.h"
#include <vector>

/* NSUV_WUR -> NSUV_WARN_UNUSED_RESULT */
#if defined(__clang__) && __has_attribute(warn_unused_result)
# define NSUV_WUR __attribute__((warn_unused_result))
#elif defined(__GNUC__) && !__INTEL_COMPILER &&                               \
  (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ > 0))
#define NSUV_WUR __attribute__((warn_unused_result))
#elif defined(_MSC_VER) && (_MSC_VER >= 1700)
#define NSUV_WUR _Check_return_
#else
#define NSUV_WUR /* NOT SUPPORTED */
#endif

#if !defined(DEBUG) && defined(__clang__) && __has_attribute(always_inline)
#define NSUV_INLINE inline __attribute__((always_inline))
#elif !defined(DEBUG) && defined(_MSC_VER)
#define NSUV_INLINE __forceinline
#else
#define NSUV_INLINE inline
#endif

#define NSUV_PROXY_FNS(name, ...)                                             \
  template <typename CB_T>                                                    \
  static NSUV_INLINE void name(__VA_ARGS__);                                  \
  template <typename CB_T, typename D_T>                                      \
  static NSUV_INLINE void name(__VA_ARGS__);

namespace nsuv {

/* uv_req classes */
template <class, class, class>
class ns_req;
template <class>
class ns_connect;
template <class>
class ns_write;
class ns_udp_send;

/* uv_handle classes */
template <class, class>
class ns_handle;
template <class, class>
class ns_stream;
class ns_async;
class ns_poll;
class ns_tcp;
class ns_timer;
class ns_prepare;
class ns_udp;

/* everything else */
class ns_mutex;
class ns_thread;


/**
 * UV_T - uv_<type>_t this class inherits.
 * R_T  - ns_<req_type> that inherits this class.
 * H_T  - ns_<handle_type> that utilizes this class.
 * D_T  - data type passed to the callback, etc.
 */
template <class UV_T, class R_T, class H_T>
class ns_req : public UV_T {
 public:
  template <typename CB, typename D_T = void*>
  NSUV_INLINE void init(H_T* handle, CB cb, D_T* data = nullptr);

  /* Return the ns_handle that has ownership of this req. */
  NSUV_INLINE H_T* handle();
  NSUV_INLINE UV_T* uv_req();
  NSUV_INLINE uv_req_t* base_req();
  NSUV_INLINE uv_req_type get_type();
  NSUV_INLINE const char* type_name();
  NSUV_INLINE NSUV_WUR int cancel();

  /* Enforce better type safety on data getter/setter. */
  template <typename D_T>
  NSUV_INLINE D_T* get_data();
  NSUV_INLINE void set_data(void* ptr);

  static NSUV_INLINE R_T* cast(void* req);
  static NSUV_INLINE R_T* cast(uv_req_t* req);
  static NSUV_INLINE R_T* cast(UV_T* req);

 private:
  template <class, class>
  friend class ns_stream;
  friend class ns_tcp;
  friend class ns_udp;

  H_T* handle_ = nullptr;
  void(*req_cb_)() = nullptr;
  void* req_cb_data_ = nullptr;
};


/* ns_connect */

template <class H_T>
class ns_connect : public ns_req<uv_connect_t, ns_connect<H_T>, H_T> {
 public:
  NSUV_INLINE const struct sockaddr* sockaddr();

 private:
  friend class ns_tcp;

  template <typename CB, typename D_T = void*>
  NSUV_INLINE void init(H_T* handle,
                        const struct sockaddr* addr,
                        CB cb,
                        D_T* data = nullptr);
  struct sockaddr addr_;
};


/* ns_write */

template <class H_T>
class ns_write : public ns_req<uv_write_t, ns_write<H_T>, H_T> {
 public:
  NSUV_INLINE std::vector<uv_buf_t>& bufs();

 private:
  template <class, class>
  friend class ns_stream;
  friend class ns_tcp;

  template <typename CB, typename D_T = void*>
  NSUV_INLINE void init(H_T* handle,
                        const uv_buf_t bufs[],
                        size_t nbufs,
                        CB cb,
                        D_T* data = nullptr);
  template <typename CB, typename D_T = void*>
  NSUV_INLINE void init(H_T* handle,
                        const std::vector<uv_buf_t>& bufs,
                        CB cb,
                        D_T* data = nullptr);
  template <typename CB, typename D_T = void*>
  NSUV_INLINE void init(H_T* handle,
                        std::vector<uv_buf_t>&& bufs,
                        CB cb,
                        D_T* data = nullptr);

  std::vector<uv_buf_t> bufs_;
};


/* ns_udp_send */

class ns_udp_send : public ns_req<uv_udp_send_t, ns_udp_send, ns_udp> {
 public:
  NSUV_INLINE std::vector<uv_buf_t>& bufs();
  NSUV_INLINE const struct sockaddr* sockaddr();

 private:
  friend class ns_udp;

  template <typename CB, typename D_T = void*>
  NSUV_INLINE void init(ns_udp* handle,
                        const uv_buf_t bufs[],
                        size_t nbufs,
                        const struct sockaddr* addr,
                        CB cb,
                        D_T* data = nullptr);
  template <typename CB, typename D_T = void*>
  NSUV_INLINE void init(ns_udp* handle,
                        const std::vector<uv_buf_t>& bufs,
                        const struct sockaddr* addr,
                        CB cb,
                        D_T* data = nullptr);
  template <typename CB, typename D_T = void*>
  NSUV_INLINE void init(ns_udp* handle,
                        std::vector<uv_buf_t>&& bufs,
                        const struct sockaddr* addr,
                        CB cb,
                        D_T* data = nullptr);

  std::vector<uv_buf_t> bufs_;
  struct sockaddr addr_;
};


/* ns_handle */

/* ns_handle is a wrapper for that abstracts libuv API calls specific to
 * uv_handle_t. All inheriting classes must then implement methods that pertain
 * specifically to that handle type.
 */
template <class UV_T, class H_T>
class ns_handle : public UV_T {
 public:
  NSUV_INLINE UV_T* uv_handle();
  NSUV_INLINE uv_handle_t* base_handle();
  NSUV_INLINE uv_loop_t* get_loop();
  NSUV_INLINE uv_handle_type get_type();
  NSUV_INLINE const char* type_name();
  NSUV_INLINE bool is_closing();
  NSUV_INLINE bool is_active();

  /* Close the handle and run the callback. Uses a lambda to allow the callback
   * signature to match the pointer of D_T.
   */
  NSUV_INLINE void close();
  NSUV_INLINE void close(void(*cb)(H_T*));
  template <typename D_T>
  NSUV_INLINE void close(void(*cb)(H_T*, D_T*), D_T* data);
  /* Convinence method to just delete the handle after it's closed. */
  NSUV_INLINE void close_and_delete();
  NSUV_INLINE void set_data(void* data);
  /* A void* always needs to be cast anyway, so allow what it will be cast to
   * as a template type.
   */
  template <typename D_T>
  NSUV_INLINE D_T* get_data();
  NSUV_INLINE void unref();

  static NSUV_INLINE H_T* cast(void* handle);
  static NSUV_INLINE H_T* cast(uv_handle_t* handle);
  static NSUV_INLINE H_T* cast(UV_T* handle);

 private:
  NSUV_PROXY_FNS(close_proxy_, uv_handle_t* handle)

  static NSUV_INLINE void close_delete_cb_(uv_handle_t* handle);

  void(*close_cb_ptr_)() = nullptr;
  void* close_cb_data_ = nullptr;
};


/* ns_stream */

template <class UV_T, class H_T>
class ns_stream : public ns_handle<UV_T, H_T> {
 public:
  uv_stream_t* base_stream();
  // TODO(trevnorris): Running listen() on an already listening socket is UB,
  // but the UB here does not match the UB in libuv.
  NSUV_INLINE NSUV_WUR int listen(int backlog, void(*cb)(H_T*, int));
  template <typename D_T>
  NSUV_INLINE NSUV_WUR int listen(int backlog,
                                  void(*cb)(H_T*, int, D_T*),
                                  D_T* data);
  NSUV_INLINE NSUV_WUR int write(ns_write<H_T>* req,
                                 const uv_buf_t bufs[],
                                 size_t nbufs,
                                 void(*cb)(ns_write<H_T>*, int));
  NSUV_INLINE NSUV_WUR int write(ns_write<H_T>* req,
                                 const std::vector<uv_buf_t>& bufs,
                                 void(*cb)(ns_write<H_T>*, int));
  template <typename D_T>
  NSUV_INLINE NSUV_WUR int write(ns_write<H_T>* req,
                                 const uv_buf_t bufs[],
                                 size_t nbufs,
                                 void(*cb)(ns_write<H_T>*, int, D_T*),
                                 D_T* data);
  template <typename D_T>
  NSUV_INLINE NSUV_WUR int write(ns_write<H_T>* req,
                                 const std::vector<uv_buf_t>& bufs,
                                 void(*cb)(ns_write<H_T>*, int, D_T*),
                                 D_T* data);

 private:
  NSUV_PROXY_FNS(listen_proxy_, uv_stream_t* handle, int status)
  NSUV_PROXY_FNS(write_proxy_, uv_write_t* uv_req, int status)

  void(*listen_cb_ptr_)() = nullptr;
  void* listen_cb_data_ = nullptr;
};


/* ns_async */

class ns_async : public ns_handle<uv_async_t, ns_async> {
 public:
  NSUV_INLINE NSUV_WUR int init(uv_loop_t* loop, void(*cb)(ns_async*));
  template <typename D_T>
  NSUV_INLINE NSUV_WUR int init(uv_loop_t* loop,
                                void(*cb)(ns_async*, D_T*),
                                D_T* data);
  NSUV_INLINE NSUV_WUR int send();

 private:
  NSUV_PROXY_FNS(async_proxy_, uv_async_t* handle)

  void(*async_cb_ptr_)() = nullptr;
  void* async_cb_data_ = nullptr;
};


/* ns_poll */

class ns_poll : public ns_handle<uv_poll_t, ns_poll> {
 public:
  NSUV_INLINE NSUV_WUR int init(uv_loop_t* loop, int fd);
  NSUV_INLINE NSUV_WUR int init_socket(uv_loop_t* loop, uv_os_sock_t socket);

  NSUV_INLINE NSUV_WUR int start(int events, void(*cb)(ns_poll*, int, int));
  template <typename D_T>
  NSUV_INLINE NSUV_WUR int start(int events,
                             void(*cb)(ns_poll*, int, int, D_T*),
                             D_T* data);
  NSUV_INLINE NSUV_WUR int stop();

 private:
  NSUV_PROXY_FNS(poll_proxy_, uv_poll_t* handle, int poll, int events)

  void(*poll_cb_ptr_)() = nullptr;
  void* poll_cb_data_ = nullptr;
};


/* ns_tcp */

class ns_tcp : public ns_stream<uv_tcp_t, ns_tcp> {
 public:
  NSUV_INLINE NSUV_WUR int init(uv_loop_t* loop);
  NSUV_INLINE NSUV_WUR int bind(const struct sockaddr* addr,
                                unsigned int flags = 0);
  NSUV_INLINE NSUV_WUR int connect(ns_connect<ns_tcp>* req,
                                   const struct sockaddr* addr,
                                   void(*cb)(ns_connect<ns_tcp>*, int));
  template <typename D_T>
  NSUV_INLINE NSUV_WUR int connect(ns_connect<ns_tcp>* req,
                                   const struct sockaddr* addr,
                                   void(*cb)(ns_connect<ns_tcp>*, int, D_T*),
                                   D_T* data);
  NSUV_INLINE NSUV_WUR int nodelay(bool enable);
  NSUV_INLINE NSUV_WUR int keepalive(bool enable, int delay);

 private:
  NSUV_PROXY_FNS(connect_proxy_, uv_connect_t* uv_req, int status)
};


/* ns_timer */

class ns_timer : public ns_handle<uv_timer_t, ns_timer> {
 public:
  NSUV_INLINE NSUV_WUR int init(uv_loop_t* loop);
  NSUV_INLINE NSUV_WUR int start(void(*cb)(ns_timer*),
                                 uint64_t timeout,
                                 uint64_t repeat);
  template <typename D_T>
  NSUV_INLINE NSUV_WUR int start(void(*cb)(ns_timer*, D_T*),
                                 uint64_t timeout,
                                 uint64_t repeat,
                                 D_T* data);
  NSUV_INLINE NSUV_WUR int stop();
  NSUV_INLINE size_t get_repeat();

 private:
  NSUV_PROXY_FNS(timer_proxy_, uv_timer_t* handle)
  void(*timer_cb_ptr_)() = nullptr;
  void* timer_cb_data_ = nullptr;
};


/* ns_check, ns_idle, ns_prepare */

#define NSUV_LOOP_WATCHER_DEFINE(name)                                        \
  class ns_##name : public ns_handle<uv_##name##_t, ns_##name> {              \
   public:                                                                    \
    NSUV_INLINE NSUV_WUR int init(uv_loop_t* loop);                           \
    NSUV_INLINE NSUV_WUR int start(void(*cb)(ns_##name*));                    \
    template <typename D_T>                                                   \
    NSUV_INLINE NSUV_WUR int start(void(*cb)(ns_##name*, D_T*), D_T* data);   \
    NSUV_INLINE NSUV_WUR int stop();                                          \
   private:                                                                   \
    NSUV_PROXY_FNS(name##_proxy_, uv_##name##_t* handle)                      \
    void(*name##_cb_ptr_)() = nullptr;                                        \
    void* name##_cb_data_ = nullptr;                                          \
  };

NSUV_LOOP_WATCHER_DEFINE(check)
NSUV_LOOP_WATCHER_DEFINE(idle)
NSUV_LOOP_WATCHER_DEFINE(prepare)

#undef NSUV_LOOP_WATCHER_DEFINE


/* ns_udp */

class ns_udp : public ns_handle<uv_udp_t, ns_udp> {
 public:
  NSUV_INLINE NSUV_WUR int init(uv_loop_t*);
  NSUV_INLINE NSUV_WUR int send(ns_udp_send* req,
                                const uv_buf_t bufs[],
                                size_t nbufs,
                                const struct sockaddr* addr,
                                void(*cb)(ns_udp_send*, int));
  NSUV_INLINE NSUV_WUR int send(ns_udp_send* req,
                                const std::vector<uv_buf_t>& bufs,
                                const struct sockaddr* addr,
                                void(*cb)(ns_udp_send*, int));
  template <typename D_T>
  NSUV_INLINE NSUV_WUR int send(ns_udp_send* req,
                                const uv_buf_t bufs[],
                                size_t nbufs,
                                const struct sockaddr* addr,
                                void(*cb)(ns_udp_send*, int, D_T*),
                                D_T* data);
  template <typename D_T>
  NSUV_INLINE NSUV_WUR int send(ns_udp_send* req,
                                const std::vector<uv_buf_t>& bufs,
                                const struct sockaddr* addr,
                                void(*cb)(ns_udp_send*, int, D_T*),
                                D_T* data);

 private:
  NSUV_PROXY_FNS(send_proxy_, uv_udp_send_t* uv_req, int status)
};


/**
 * The following aren't handles or reqs.
 */


/* ns_mutex */

class ns_mutex {
 public:
  NSUV_INLINE NSUV_WUR int init();
  NSUV_INLINE NSUV_WUR int init_recursive();
  NSUV_INLINE void destroy();
  NSUV_INLINE void lock();
  NSUV_INLINE NSUV_WUR int trylock();
  NSUV_INLINE void unlock();

  class scoped_lock {
   public:
    scoped_lock() = delete;
    scoped_lock(const scoped_lock&) = delete;
    scoped_lock(scoped_lock&&) = delete;
    scoped_lock& operator=(const scoped_lock&) = delete;
    scoped_lock& operator=(scoped_lock&&) = delete;
    NSUV_INLINE explicit scoped_lock(ns_mutex* mutex, bool do_lock = true);
    NSUV_INLINE ~scoped_lock();
   private:
    ns_mutex* mutex_ref_;
  };

 private:
  uv_mutex_t mutex_;
};


/* ns_thread */

class ns_thread {
 public:
  NSUV_INLINE NSUV_WUR int create(void(*cb)(ns_thread*));
  template <typename D_T>
  NSUV_INLINE NSUV_WUR int create(void(*cb)(ns_thread*, D_T*), D_T* data);
  NSUV_INLINE NSUV_WUR int create_ex(const uv_thread_options_t* params,
                                     void(*cb)(ns_thread*));
  template <typename D_T>
  NSUV_INLINE NSUV_WUR int create_ex(const uv_thread_options_t* params,
                                     void(*cb)(ns_thread*, D_T*),
                                     D_T* data);
  NSUV_INLINE NSUV_WUR int join();
  NSUV_INLINE uv_thread_t base();
  NSUV_INLINE uv_thread_t owner();
  NSUV_INLINE NSUV_WUR int equal(uv_thread_t* t2);
  static NSUV_INLINE uv_thread_t self();

 private:
  NSUV_PROXY_FNS(create_proxy_, void* arg)

  uv_thread_t thread_;
  uv_thread_t parent_;
  void(*thread_cb_ptr_)() = nullptr;
  void* thread_cb_data_ = nullptr;
};

}  // namespace nsuv

#undef NSUV_PROXY_FNS
#undef NSUV_INLINE
#undef NSUV_WUR

#endif  // NSUV_H_
