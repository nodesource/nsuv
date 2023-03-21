#include "../include/nsuv-inl.h"
#include "./helpers.h"

#include <errno.h>
#include <string.h> /* memset */
#include <fcntl.h>
#include <sys/stat.h>
#include <limits.h> /* INT_MAX, PATH_MAX, IOV_MAX */

#ifndef _WIN32
# include <unistd.h> /* unlink, rmdir, etc. */
#else
# include <winioctl.h>
# include <direct.h>
# include <io.h>
# ifndef ERROR_SYMLINK_NOT_SUPPORTED
#  define ERROR_SYMLINK_NOT_SUPPORTED 1464
# endif
# define unlink _unlink
# define rmdir _rmdir
# define open _open
# define write _write
# define close _close
# ifndef stat
#  define stat _stati64
# endif
# ifndef lseek
#   define lseek _lseek
# endif
#endif

#define TOO_LONG_NAME_LENGTH 65536
#define PATHMAX 4096

using nsuv::ns_fs;

typedef struct {
  const char* path;
  double atime;
  double mtime;
} utime_check_t;


static int dummy_cb_count;
static int close_cb_count;
static int create_cb_count;
static int open_cb_count;
static int read_cb_count;
static int write_cb_count;
static int unlink_cb_count;
static int mkdir_cb_count;
static int mkdtemp_cb_count;
static int mkstemp_cb_count;
static int rmdir_cb_count;
static int scandir_cb_count;
static int stat_cb_count;
static int rename_cb_count;
static int fsync_cb_count;
static int fdatasync_cb_count;
static int ftruncate_cb_count;
static int sendfile_cb_count;
static int fstat_cb_count;
static int access_cb_count;
static int chmod_cb_count;
static int fchmod_cb_count;
static int chown_cb_count;
static int fchown_cb_count;
static int lchown_cb_count;
static int link_cb_count;
static int symlink_cb_count;
static int readlink_cb_count;
static int realpath_cb_count;
static int utime_cb_count;
static int futime_cb_count;
static int lutime_cb_count;
static int statfs_cb_count;

static char buf[32];
static char buf2[32];
static char test_buf[] = "test-buffer\n";
static char test_buf2[] = "second-buffer\n";
static uv_buf_t iov;

#ifdef _WIN32
int uv_test_getiovmax(void) {
  return INT32_MAX; /* Emulated by libuv, so no real limit. */
}
#else
int uv_test_getiovmax(void) {
#if defined(IOV_MAX)
  return IOV_MAX;
#elif defined(_SC_IOV_MAX)
  static int iovmax = -1;
  if (iovmax == -1) {
    iovmax = sysconf(_SC_IOV_MAX);
    /* On some embedded devices (arm-linux-uclibc based ip camera),
     * sysconf(_SC_IOV_MAX) can not get the correct value. The return
     * value is -1 and the errno is EINPROGRESS. Degrade the value to 1.
     */
    if (iovmax == -1) iovmax = 1;
  }
  return iovmax;
#else
  return 1024;
#endif
}
#endif

#ifdef _WIN32
/*
 * This tag and guid have no special meaning, and don't conflict with
 * reserved ids.
*/
static unsigned REPARSE_TAG = 0x9913;
static GUID REPARSE_GUID = {
  0x1bf6205f, 0x46ae, 0x4527,
  { 0xb1, 0x0c, 0xc5, 0x09, 0xb7, 0x55, 0x22, 0x80 }};
#endif

static void check_permission(const char* filename, unsigned int mode) {
  int r;
  uv_fs_t req;
  uv_stat_t* s;

  r = uv_fs_stat(nullptr, &req, filename, nullptr);
  ASSERT(r == 0);
  ASSERT(req.result == 0);

  s = &req.statbuf;
#if defined(_WIN32) || defined(__CYGWIN__) || defined(__MSYS__)
  /*
   * On Windows, chmod can only modify S_IWUSR (_S_IWRITE) bit,
   * so only testing for the specified flags.
   */
  ASSERT((s->st_mode & 0777) & mode);
#else
  ASSERT((s->st_mode & 0777) == mode);
#endif

  uv_fs_req_cleanup(&req);
}

static void unlink_cb(ns_fs* req) {
  ASSERT(req->fs_type == UV_FS_UNLINK);
  ASSERT(req->result == 0);
  unlink_cb_count++;
  req->cleanup();
}

static void unlink_cb2(ns_fs* req) {
  unlink_cb(req);
  delete req;
}


static void close_cb(ns_fs* req) {
  int r;
  ASSERT(req->fs_type == UV_FS_CLOSE);
  ASSERT(req->result == 0);
  close_cb_count++;
  req->cleanup();
  if (close_cb_count == 3) {
    ns_fs* unlink_req = new (std::nothrow) ns_fs();
    r = unlink_req->unlink(req->get_loop(), "test_file2", unlink_cb2);
    ASSERT(r == 0);
  }
  delete req;
}


static void mkdir_cb(ns_fs* req, ns_fs* mkdir_req) {
  ASSERT(req == mkdir_req);
  ASSERT(req->fs_type == UV_FS_MKDIR);
  ASSERT(req->result == 0);
  mkdir_cb_count++;
  ASSERT(req->path);
  ASSERT(memcmp(req->path, "test_dir\0", 9) == 0);
  req->cleanup();
}


static void assert_is_file_type(uv_dirent_t dent) {
#ifdef HAVE_DIRENT_TYPES
  /*
   * For Apple and Windows, we know getdents is expected to work but for other
   * environments, the filesystem dictates whether or not getdents supports
   * returning the file type.
   *
   *   See:
   *     http://man7.org/linux/man-pages/man2/getdents.2.html
   *     https://github.com/libuv/libuv/issues/501
   */
  #if defined(__APPLE__) || defined(_WIN32)
    ASSERT(dent.type == UV_DIRENT_FILE);
  #else
    ASSERT((dent.type == UV_DIRENT_FILE || dent.type == UV_DIRENT_UNKNOWN));
  #endif
#else
  ASSERT(dent.type == UV_DIRENT_UNKNOWN);
#endif
}


static void open_noent_cb(ns_fs* req) {
  ASSERT(req->fs_type == UV_FS_OPEN);
  ASSERT(req->result == UV_ENOENT);
  open_cb_count++;
  req->cleanup();
}


TEST_CASE("fs_file_noent", "[fs]") {
  ns_fs req;
  uv_loop_t* loop;
  int r;

  open_cb_count = 0;
  loop = uv_default_loop();

  r = req.open("does_not_exist", O_RDONLY, 0);
  ASSERT(r == UV_ENOENT);
  ASSERT(req.result == UV_ENOENT);
  req.cleanup();

  r = req.open(loop, "does_not_exist", O_RDONLY, 0, open_noent_cb);
  ASSERT(r == 0);

  ASSERT(open_cb_count == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(open_cb_count == 1);

  /* TODO add EACCES test */

  make_valgrind_happy();
}


static void open_nametoolong_cb(ns_fs* req) {
  ASSERT(req->fs_type == UV_FS_OPEN);
  ASSERT(req->result == UV_ENAMETOOLONG);
  open_cb_count++;
  req->cleanup();
}

TEST_CASE("fs_file_nametoolong", "[fs]") {
  ns_fs req;
  uv_loop_t* loop;
  int r;
  char name[TOO_LONG_NAME_LENGTH + 1];

  loop = uv_default_loop();
  open_cb_count = 0;

  memset(name, 'a', TOO_LONG_NAME_LENGTH);
  name[TOO_LONG_NAME_LENGTH] = 0;

  r = req.open(name, O_RDONLY, 0);
  ASSERT(r == UV_ENAMETOOLONG);
  ASSERT(req.result == UV_ENAMETOOLONG);
  req.cleanup();

  r = req.open(loop, name, O_RDONLY, 0, open_nametoolong_cb);
  ASSERT(r == 0);

  ASSERT(open_cb_count == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(open_cb_count == 1);

  make_valgrind_happy();
}


static void open_loop_cb(ns_fs* req) {
  ASSERT(req->fs_type == UV_FS_OPEN);
  ASSERT(req->result == UV_ELOOP);
  open_cb_count++;
  req->cleanup();
}

TEST_CASE("fs_file_loop", "[fs]") {
  ns_fs req;
  uv_loop_t* loop;
  int r;

  loop = uv_default_loop();
  open_cb_count = 0;

  unlink("test_symlink");
  r = req.symlink("test_symlink", "test_symlink", 0);
#ifdef _WIN32
  /*
   * Symlinks are only suported but only when elevated, otherwise
   * we'll see UV_EPERM.
   */
  if (r == UV_EPERM)
    return 0;
#elif defined(__MSYS__)
  /* MSYS2's approximation of symlinks with copies does not work for broken
     links.  */
  if (r == UV_ENOENT)
    return 0;
#endif
  ASSERT(r == 0);
  req.cleanup();

  r = req.open("test_symlink", O_RDONLY, 0);
  ASSERT(r == UV_ELOOP);
  ASSERT(req.result == UV_ELOOP);
  req.cleanup();

  r = req.open(loop, "test_symlink", O_RDONLY, 0, open_loop_cb);
  ASSERT(r == 0);

  ASSERT(open_cb_count == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(open_cb_count == 1);

  unlink("test_symlink");

  make_valgrind_happy();
}

static void check_utime(const char* path,
                        double atime,
                        double mtime,
                        int test_lutime) {
  uv_stat_t* s;
  ns_fs req;
  int r;

  if (test_lutime)
    r = req.lstat(path);
  else
    r = req.stat(path);

  ASSERT_EQ(r, 0);

  ASSERT_EQ(req.result, 0);
  s = &req.statbuf;

  if (s->st_atim.tv_nsec == 0 && s->st_mtim.tv_nsec == 0) {
    /*
     * Test sub-second timestamps only when supported (such as Windows with
     * NTFS). Some other platforms support sub-second timestamps, but that
     * support is filesystem-dependent. Notably OS X (HFS Plus) does NOT
     * support sub-second timestamps. But kernels may round or truncate in
     * either direction, so we may accept either possible answer.
     */
#ifdef _WIN32
    ASSERT_DOUBLE_EQ(atime, static_cast<uint32_t>(atime));
    ASSERT_DOUBLE_EQ(mtime, static_cast<uint32_t>(atime));
#endif
    if (atime > 0 || static_cast<uint32_t>(atime == atime))
      ASSERT_EQ(s->st_atim.tv_sec, static_cast<uint32_t>(atime));
    if (mtime > 0 || static_cast<uint32_t>(mtime == mtime))
      ASSERT_EQ(s->st_mtim.tv_sec, static_cast<uint32_t>(mtime));
    ASSERT_GE(s->st_atim.tv_sec, static_cast<uint32_t>(atime) - 1);
    ASSERT_GE(s->st_mtim.tv_sec, static_cast<uint32_t>(mtime) - 1);
    ASSERT_LE(s->st_atim.tv_sec, static_cast<uint32_t>(atime));
    ASSERT_LE(s->st_mtim.tv_sec, static_cast<uint32_t>(mtime));
  } else {
    double st_atim;
    double st_mtim;
#if !defined(__APPLE__) && !defined(__SUNPRO_C)
    /* TODO(vtjnash): would it be better to normalize this? */
    ASSERT_DOUBLE_GE(s->st_atim.tv_nsec, 0);
    ASSERT_DOUBLE_GE(s->st_mtim.tv_nsec, 0);
#endif
    st_atim = s->st_atim.tv_sec + s->st_atim.tv_nsec / 1e9;
    st_mtim = s->st_mtim.tv_sec + s->st_mtim.tv_nsec / 1e9;
    /*
     * Linux does not allow reading reliably the atime of a symlink
     * since readlink() can update it
     */
    if (!test_lutime)
      ASSERT_DOUBLE_EQ(st_atim, atime);
    ASSERT_DOUBLE_EQ(st_mtim, mtime);
  }

  req.cleanup();
}


static void fsync_cb(ns_fs* req, ns_fs* open_req1) {
  ns_fs* close_req = new (std::nothrow) ns_fs();
  int r;
  ASSERT_NE(nullptr, close_req);
  ASSERT(req->fs_type == UV_FS_FSYNC);
  ASSERT(req->result == 0);
  fsync_cb_count++;
  req->cleanup();
  r = close_req->close(req->get_loop(), open_req1->result, close_cb);
  ASSERT(r == 0);
  delete req;
}

static void fdatasync_cb(ns_fs* req, ns_fs* open_req1) {
  ns_fs* fsync_req = new (std::nothrow) ns_fs();
  int r;
  ASSERT(req->fs_type == UV_FS_FDATASYNC);
  ASSERT(req->result == 0);
  fdatasync_cb_count++;
  req->cleanup();
  r = fsync_req->fsync(req->get_loop(), open_req1->result, fsync_cb, open_req1);
  ASSERT(r == 0);
  delete req;
}

static void write_cb(ns_fs* req, ns_fs* open_req1) {
  ns_fs* fdatasync_req = new (std::nothrow) ns_fs;
  int r;
  ASSERT_NE(nullptr, fdatasync_req);
  ASSERT(req->fs_type == UV_FS_WRITE);
  ASSERT(req->result >= 0);  /* FIXME(bnoordhuis) Check if requested size? */
  write_cb_count++;
  req->cleanup();
  r = fdatasync_req->fdatasync(
      req->get_loop(), open_req1->result, fdatasync_cb, open_req1);
  ASSERT(r == 0);
  delete req;
}

static void create_cb(ns_fs* req, ns_fs* open_req1) {
  ns_fs* write_req = new (std::nothrow) ns_fs();
  int r;
  ASSERT_NE(nullptr, write_req);
  ASSERT(req->fs_type == UV_FS_OPEN);
  ASSERT(req->result >= 0);
  create_cb_count++;
  req->cleanup();
  iov = uv_buf_init(test_buf, sizeof(test_buf));
  r = write_req->write(
      req->get_loop(), req->result, { iov }, -1, write_cb, open_req1);
  ASSERT(r == 0);
}

static void ftruncate_cb(ns_fs* req, ns_fs* open_req1) {
  ns_fs* close_req = new (std::nothrow) ns_fs();
  int r;
  ASSERT_NE(nullptr, close_req);
  ASSERT(req->fs_type == UV_FS_FTRUNCATE);
  ASSERT(req->result == 0);
  ftruncate_cb_count++;
  req->cleanup();
  r = close_req->close(req->get_loop(), open_req1->result, close_cb);
  ASSERT(r == 0);
  delete req;
}

static void read_cb(ns_fs* req, ns_fs* open_req1) {
  ns_fs* rreq = new (std::nothrow) ns_fs();
  int r;
  ASSERT_NE(nullptr, rreq);
  ASSERT(req->fs_type == UV_FS_READ);
  ASSERT(req->result >= 0);  /* FIXME(bnoordhuis) Check if requested size? */
  read_cb_count++;
  req->cleanup();
  if (read_cb_count == 1) {
    r = rreq->ftruncate(
        req->get_loop(), open_req1->result, 7, ftruncate_cb, open_req1);
  } else {
    r = rreq->close(req->get_loop(), open_req1->result, close_cb);
  }
  ASSERT(r == 0);
  delete req;
}

static void open_cb(ns_fs* open_req1) {
  ns_fs* read_req = new (std::nothrow) ns_fs();
  int r;
  uv_buf_t iov;
  ASSERT_NE(nullptr, read_req);
  ASSERT(open_req1->fs_type == UV_FS_OPEN);
  if (open_req1->result < 0) {
    fprintf(
        stderr, "async open error: %d\n", static_cast<int>(open_req1->result));
    FAIL("error");
  }
  open_cb_count++;
  ASSERT(open_req1->path);
  ASSERT(memcmp(open_req1->path, "test_file2\0", 11) == 0);
  open_req1->cleanup();
  memset(buf, 0, sizeof(buf));
  iov = uv_buf_init(buf, sizeof(buf));
  r = read_req->read(open_req1->get_loop(),
                     open_req1->result,
                     { iov },
                     -1,
                     read_cb,
                     open_req1);
  ASSERT(r == 0);
}

static void rename_cb(ns_fs* req) {
  ASSERT(req->fs_type == UV_FS_RENAME);
  ASSERT(req->result == 0);
  rename_cb_count++;
  req->cleanup();
}

TEST_CASE("fs_file_async", "[fs]") {
  ns_fs open_req1;
  ns_fs rename_req;
  uv_loop_t* loop;
  int r;

  /* Setup. */
  unlink("test_file");
  unlink("test_file2");

  loop = uv_default_loop();
  create_cb_count = 0;
  write_cb_count = 0;
  fsync_cb_count = 0;
  fdatasync_cb_count = 0;
  close_cb_count = 0;
  open_cb_count = 0;

  r = open_req1.open(loop,
                     "test_file",
                     O_WRONLY | O_CREAT,
                     S_IRUSR | S_IWUSR,
                     create_cb,
                     &open_req1);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);

  ASSERT(create_cb_count == 1);
  ASSERT(write_cb_count == 1);
  ASSERT(fsync_cb_count == 1);
  ASSERT(fdatasync_cb_count == 1);
  ASSERT(close_cb_count == 1);

  r = rename_req.rename(loop, "test_file", "test_file2", rename_cb);
  ASSERT(r == 0);

  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(create_cb_count == 1);
  ASSERT(write_cb_count == 1);
  ASSERT(close_cb_count == 1);
  ASSERT(rename_cb_count == 1);

  r = open_req1.open(loop, "test_file2", O_RDWR, 0, open_cb);
  ASSERT(r == 0);

  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(open_cb_count == 1);
  ASSERT(read_cb_count == 1);
  ASSERT(close_cb_count == 2);
  ASSERT(rename_cb_count == 1);
  ASSERT(create_cb_count == 1);
  ASSERT(write_cb_count == 1);
  ASSERT(ftruncate_cb_count == 1);

  r = open_req1.open(loop, "test_file2", O_RDONLY, 0, open_cb);
  ASSERT(r == 0);

  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(open_cb_count == 2);
  ASSERT(read_cb_count == 2);
  ASSERT(close_cb_count == 3);
  ASSERT(rename_cb_count == 1);
  ASSERT(unlink_cb_count == 1);
  ASSERT(create_cb_count == 1);
  ASSERT(write_cb_count == 1);
  ASSERT(ftruncate_cb_count == 1);

  /* Cleanup. */
  unlink("test_file");
  unlink("test_file2");

  make_valgrind_happy();
}


static void fs_file_sync(int add_flags) {
  ns_fs close_req;
  ns_fs ftruncate_req;
  ns_fs open_req1;
  ns_fs read_req;
  ns_fs rename_req;
  ns_fs unlink_req;
  ns_fs write_req;
  uv_buf_t iov;
  int r;

  /* Setup. */
  unlink("test_file");
  unlink("test_file2");

  r = open_req1.open(
      "test_file", O_WRONLY | O_CREAT | add_flags, S_IWUSR | S_IRUSR);
  ASSERT(r >= 0);
  ASSERT(open_req1.result >= 0);
  open_req1.cleanup();

  iov = uv_buf_init(test_buf, sizeof(test_buf));
  r = write_req.write(open_req1.result, { iov }, -1);
  ASSERT(r >= 0);
  ASSERT(write_req.result >= 0);
  write_req.cleanup();

  r = close_req.close(open_req1.result);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  close_req.cleanup();

  r = open_req1.open("test_file", O_RDWR | add_flags, 0);
  ASSERT(r >= 0);
  ASSERT(open_req1.result >= 0);
  open_req1.cleanup();

  iov = uv_buf_init(buf, sizeof(buf));
  r = read_req.read(open_req1.result, { iov }, -1);
  ASSERT(r >= 0);
  ASSERT(read_req.result >= 0);
  read_req.cleanup();

  r = ftruncate_req.ftruncate(open_req1.result, 7);
  ASSERT(r == 0);
  ASSERT(ftruncate_req.result == 0);
  ftruncate_req.cleanup();

  r = close_req.close(open_req1.result);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  close_req.cleanup();

  r = rename_req.rename("test_file", "test_file2");
  ASSERT(r == 0);
  ASSERT(rename_req.result == 0);
  rename_req.cleanup();

  r = open_req1.open("test_file2", O_RDONLY | add_flags, 0);
  ASSERT(r >= 0);
  ASSERT(open_req1.result >= 0);
  open_req1.cleanup();

  memset(buf, 0, sizeof(buf));
  iov = uv_buf_init(buf, sizeof(buf));
  r = read_req.read(open_req1.result, { iov }, -1);
  ASSERT(r >= 0);
  ASSERT(read_req.result >= 0);
  read_req.cleanup();

  r = close_req.close(open_req1.result);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  close_req.cleanup();

  r = unlink_req.unlink("test_file2");
  ASSERT(r == 0);
  ASSERT(unlink_req.result == 0);
  unlink_req.cleanup();

  /* Cleanup */
  unlink("test_file");
  unlink("test_file2");
}

TEST_CASE("fs_file_sync", "[fs]") {
  fs_file_sync(0);
  fs_file_sync(UV_FS_O_FILEMAP);

  make_valgrind_happy();
}


static void fs_file_write_null_buffer(int add_flags) {
  ns_fs close_req;
  ns_fs open_req1;
  ns_fs write_req;
  int r;

  /* Setup. */
  unlink("test_file");

  r = open_req1.open(
      "test_file", O_WRONLY | O_CREAT | add_flags, S_IWUSR | S_IRUSR);
  ASSERT(r >= 0);
  ASSERT(open_req1.result >= 0);
  open_req1.cleanup();

  iov = uv_buf_init(nullptr, 0);
  r = write_req.write(open_req1.result, { iov }, -1);
  ASSERT(r == 0);
  ASSERT(write_req.result == 0);
  write_req.cleanup();

  r = close_req.close(open_req1.result);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  close_req.cleanup();

  unlink("test_file");
}

TEST_CASE("fs_file_write_null_buffer", "[fs]") {
  fs_file_write_null_buffer(0);
  fs_file_write_null_buffer(UV_FS_O_FILEMAP);

  make_valgrind_happy();
}


static void scandir_cb(ns_fs* req) {
  uv_dirent_t dent;
  ASSERT(req->fs_type == UV_FS_SCANDIR);
  ASSERT(req->result == 2);
  ASSERT(req->ptr);

  while (UV_EOF != req->scandir_next(&dent)) {
    assert_is_file_type(dent);
  }
  scandir_cb_count++;
  ASSERT(req->path);
  ASSERT(memcmp(req->path, "test_dir\0", 9) == 0);
  req->cleanup();
  ASSERT(!req->ptr);
}

static void stat_cb(ns_fs* req, ns_fs* stat_req) {
  ASSERT(req == stat_req);
  ASSERT((req->fs_type == UV_FS_STAT || req->fs_type == UV_FS_LSTAT));
  ASSERT(req->result == 0);
  ASSERT(req->ptr);
  stat_cb_count++;
  req->cleanup();
  ASSERT(!req->ptr);
}

static void rmdir_cb(ns_fs* req) {
  ASSERT(req->fs_type == UV_FS_RMDIR);
  ASSERT(req->result == 0);
  rmdir_cb_count++;
  ASSERT(req->path);
  ASSERT(memcmp(req->path, "test_dir\0", 9) == 0);
  req->cleanup();
}

TEST_CASE("fs_async_dir", "[fs]") {
  int r;
  uv_dirent_t dent;
  ns_fs close_req;
  ns_fs mkdir_req;
  ns_fs open_req1;
  ns_fs rmdir_req;
  ns_fs scandir_req;
  ns_fs stat_req;
  ns_fs unlink_req;
  uv_loop_t* loop;

  /* Setup */
  unlink("test_dir/file1");
  unlink("test_dir/file2");
  rmdir("test_dir");

  loop = uv_default_loop();
  mkdir_cb_count = 0;
  scandir_cb_count = 0;
  stat_cb_count = 0;
  unlink_cb_count = 0;
  rmdir_cb_count = 0;

  r = mkdir_req.mkdir(loop, "test_dir", 0755, mkdir_cb, &mkdir_req);
  ASSERT(r == 0);

  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(mkdir_cb_count == 1);

  /* Create 2 files synchronously. */
  r = open_req1.open("test_dir/file1", O_WRONLY | O_CREAT, S_IWUSR | S_IRUSR);
  ASSERT(r >= 0);
  open_req1.cleanup();
  r = close_req.close(open_req1.result);
  ASSERT(r == 0);
  close_req.cleanup();

  r = open_req1.open("test_dir/file2", O_WRONLY | O_CREAT, S_IWUSR | S_IRUSR);
  ASSERT(r >= 0);
  open_req1.cleanup();
  r = close_req.close(open_req1.result);
  ASSERT(r == 0);
  close_req.cleanup();

  r = scandir_req.scandir(loop, "test_dir", 0, scandir_cb);
  ASSERT(r == 0);

  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(scandir_cb_count == 1);

  /* sync uv_fs_scandir */
  r = scandir_req.scandir("test_dir", 0);
  ASSERT(r == 2);
  ASSERT(scandir_req.result == 2);
  ASSERT(scandir_req.ptr);
  while (UV_EOF != scandir_req.scandir_next(&dent)) {
    assert_is_file_type(dent);
  }
  scandir_req.cleanup();
  ASSERT(!scandir_req.ptr);

  r = stat_req.stat(loop, "test_dir", stat_cb, &stat_req);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);

  r = stat_req.stat(loop, "test_dir/", stat_cb, &stat_req);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);

  r = stat_req.lstat(loop, "test_dir", stat_cb, &stat_req);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);

  r = stat_req.lstat(loop, "test_dir/", stat_cb, &stat_req);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);

  ASSERT(stat_cb_count == 4);

  r = unlink_req.unlink(loop, "test_dir/file1", unlink_cb);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(unlink_cb_count == 1);

  r = unlink_req.unlink(loop, "test_dir/file2", unlink_cb);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(unlink_cb_count == 2);

  r = rmdir_req.rmdir(loop, "test_dir", rmdir_cb);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(rmdir_cb_count == 1);

  /* Cleanup */
  unlink("test_dir/file1");
  unlink("test_dir/file2");
  rmdir("test_dir");

  make_valgrind_happy();
}


static void sendfile_cb(ns_fs* req) {
  ASSERT(req->fs_type == UV_FS_SENDFILE);
  ASSERT(req->result == 65545);
  sendfile_cb_count++;
  req->cleanup();
}

static void sendfile_nodata_cb(ns_fs* req) {
  ASSERT(req->fs_type == UV_FS_SENDFILE);
  ASSERT(req->result == 0);
  sendfile_cb_count++;
  req->cleanup();
}

static void test_sendfile(void (*setup)(int),
                          ns_fs::ns_fs_cb cb,
                          off_t expected_size) {
  int f, r;
  struct stat s1, s2;
  ns_fs close_req;
  ns_fs open_req1;
  ns_fs open_req2;
  ns_fs req;
  ns_fs sendfile_req;
  uv_loop_t* loop;
  char buf1[1];

  loop = uv_default_loop();
  sendfile_cb_count = 0;

  /* Setup. */
  unlink("test_file");
  unlink("test_file2");

  f = open("test_file", O_WRONLY | O_CREAT, S_IWUSR | S_IRUSR);
  ASSERT(f != -1);

  if (setup != nullptr)
    setup(f);

  r = close(f);
  ASSERT(r == 0);

  /* Test starts here. */
  r = open_req1.open("test_file", O_RDWR, 0);
  ASSERT(r >= 0);
  ASSERT(open_req1.result >= 0);
  open_req1.cleanup();

  r = open_req2.open("test_file2", O_WRONLY | O_CREAT, S_IWUSR | S_IRUSR);
  ASSERT(r >= 0);
  ASSERT(open_req2.result >= 0);
  open_req2.cleanup();

  r = sendfile_req.sendfile(
      loop, open_req2.result, open_req1.result, 1, 131072, cb);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);

  ASSERT(sendfile_cb_count == 1);

  r = close_req.close(open_req1.result);
  ASSERT(r == 0);
  close_req.cleanup();
  r = close_req.close(open_req2.result);
  ASSERT(r == 0);
  close_req.cleanup();

  memset(&s1, 0, sizeof(s1));
  memset(&s2, 0, sizeof(s2));
  ASSERT(0 == stat("test_file", &s1));
  ASSERT(0 == stat("test_file2", &s2));
  ASSERT(s2.st_size == expected_size);

  if (expected_size > 0) {
    ASSERT_UINT64_EQ(s1.st_size, s2.st_size + 1);
    r = open_req1.open("test_file2", O_RDWR, 0);
    ASSERT(r >= 0);
    ASSERT(open_req1.result >= 0);
    open_req1.cleanup();

    memset(buf1, 0, sizeof(buf1));
    iov = uv_buf_init(buf1, sizeof(buf1));
    r = req.read(open_req1.result, { iov }, -1);
    ASSERT(r >= 0);
    ASSERT(req.result >= 0);
    ASSERT_EQ(buf1[0], 'e'); /* 'e' from begin */
    req.cleanup();
  } else {
    ASSERT_UINT64_EQ(s1.st_size, s2.st_size);
  }

  /* Cleanup. */
  unlink("test_file");
  unlink("test_file2");

  make_valgrind_happy();
}

static void sendfile_setup(int f) {
  ASSERT(6 == write(f, "begin\n", 6));
  ASSERT(65542 == lseek(f, 65536, SEEK_CUR));
  ASSERT(4 == write(f, "end\n", 4));
}

TEST_CASE("fs_async_sendfile", "[fs]") {
  test_sendfile(sendfile_setup, sendfile_cb, 65545);
}

TEST_CASE("fs_async_sendfile_nodata", "[fs]") {
  test_sendfile(nullptr, sendfile_nodata_cb, 0);
}


static void check_mkdtemp_result(ns_fs* req) {
  ns_fs stat_req;
  int r;

  ASSERT(req->fs_type == UV_FS_MKDTEMP);
  ASSERT(req->result == 0);
  ASSERT(req->path);
  ASSERT(strlen(req->path) == 15);
  ASSERT(memcmp(req->path, "test_dir_", 9) == 0);
  ASSERT(memcmp(req->path + 9, "XXXXXX", 6) != 0);
  check_permission(req->path, 0700);

  /* Check if req->path is actually a directory */
  r = stat_req.stat(req->path);
  ASSERT(r == 0);
  ASSERT(static_cast<uv_stat_t*>(stat_req.ptr)->st_mode & S_IFDIR);
  stat_req.cleanup();
}

static void mkdtemp_cb(ns_fs* req, ns_fs* mkdtemp_req1) {
  ASSERT(req == mkdtemp_req1);
  check_mkdtemp_result(req);
  mkdtemp_cb_count++;
}

TEST_CASE("fs_mkdtemp", "[fs]") {
  int r;
  const char* path_template = "test_dir_XXXXXX";
  ns_fs mkdtemp_req1;
  ns_fs mkdtemp_req2;
  uv_loop_t* loop;

  loop = uv_default_loop();
  mkdtemp_cb_count = 0;

  r = mkdtemp_req1.mkdtemp(loop, path_template, mkdtemp_cb, &mkdtemp_req1);
  ASSERT(r == 0);

  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(mkdtemp_cb_count == 1);

  /* sync mkdtemp */
  r = mkdtemp_req2.mkdtemp(path_template);
  ASSERT(r == 0);
  check_mkdtemp_result(&mkdtemp_req2);

  /* mkdtemp return different values on subsequent calls */
  // ASSERT(strcmp(mkdtemp_req1.path, mkdtemp_req2.path) != 0);

  /* Cleanup */
  rmdir(mkdtemp_req1.path);
  rmdir(mkdtemp_req2.path);
  mkdtemp_req1.cleanup();
  mkdtemp_req2.cleanup();

  make_valgrind_happy();
}


static void check_mkstemp_result(ns_fs* req) {
  ns_fs stat_req;
  int r;

  ASSERT(req->fs_type == UV_FS_MKSTEMP);
  ASSERT(req->result >= 0);
  ASSERT(req->path);
  ASSERT(strlen(req->path) == 16);
  ASSERT(memcmp(req->path, "test_file_", 10) == 0);
  ASSERT(memcmp(req->path + 10, "XXXXXX", 6) != 0);
  check_permission(req->path, 0600);

  /* Check if req->path is actually a file */
  r = stat_req.stat(req->path);
  ASSERT(r == 0);
  ASSERT(stat_req.statbuf.st_mode & S_IFREG);
  stat_req.cleanup();
}


static void mkstemp_cb(ns_fs* req, ns_fs* mkstemp_req) {
  ASSERT(req == mkstemp_req);
  check_mkstemp_result(req);
  mkstemp_cb_count++;
}

TEST_CASE("fs_mkstemp", "[fs]") {
  int r;
  int fd;
  const char path_template[] = "test_file_XXXXXX";
  ns_fs mkstemp_req1;
  ns_fs mkstemp_req2;
  ns_fs mkstemp_req3;
  ns_fs req;
  uv_loop_t* loop;

  loop = uv_default_loop();
  mkstemp_cb_count = 0;

  r = mkstemp_req1.mkstemp(loop, path_template, mkstemp_cb, &mkstemp_req1);
  ASSERT(r == 0);

  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(mkstemp_cb_count == 1);

  /* sync mkstemp */
  r = mkstemp_req2.mkstemp(path_template);
  ASSERT(r >= 0);
  check_mkstemp_result(&mkstemp_req2);

  /* mkstemp return different values on subsequent calls */
  // ASSERT(strcmp(mkstemp_req1.path, mkstemp_req2.path) != 0);

  /* invalid template returns EINVAL */
  ASSERT_EQ(UV_EINVAL, mkstemp_req3.mkstemp("test_file"));

  /* Make sure that path is empty string */
  ASSERT_EQ(0, strlen(mkstemp_req3.path));

  mkstemp_req3.cleanup();

  /* We can write to the opened file */
  iov = uv_buf_init(test_buf, sizeof(test_buf));
  r = req.write(mkstemp_req1.result, { iov }, -1);
  ASSERT(r == sizeof(test_buf));
  ASSERT(req.result == sizeof(test_buf));
  req.cleanup();

  /* Cleanup */
  r = req.close(mkstemp_req1.result);
  req.cleanup();
  r = req.close(mkstemp_req2.result);
  req.cleanup();

  fd = req.open(mkstemp_req1.path, O_RDONLY, 0);
  ASSERT(fd >= 0);
  req.cleanup();

  memset(buf, 0, sizeof(buf));
  iov = uv_buf_init(buf, sizeof(buf));
  r = req.read(fd, { iov }, -1);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  req.cleanup();

  r = req.close(fd);
  req.cleanup();

  unlink(mkstemp_req1.path);
  unlink(mkstemp_req2.path);
  mkstemp_req1.cleanup();
  mkstemp_req2.cleanup();

  make_valgrind_happy();
}


static void fstat_cb(ns_fs* req) {
  uv_stat_t* s = static_cast<uv_stat_t*>(req->ptr);
  ASSERT(req->fs_type == UV_FS_FSTAT);
  ASSERT(req->result == 0);
  ASSERT(s->st_size == sizeof(test_buf));
  req->cleanup();
  fstat_cb_count++;
}

TEST_CASE("fs_fstat", "[fs]") {
  int r;
  ns_fs req;
  uv_file file;
  uv_stat_t* s;
  uv_loop_t* loop;
#ifndef _WIN32
  struct stat t;
#endif

#if defined(__s390__) && defined(__QEMU__)
  /* qemu-user-s390x has this weird bug where statx() reports nanoseconds
   * but plain fstat() does not.
   */
  RETURN_SKIP("Test does not currently work in QEMU");
#endif

  /* Setup. */
  unlink("test_file");

  loop = uv_default_loop();
  fstat_cb_count = 0;

  r = req.open("test_file", O_RDWR | O_CREAT, S_IWUSR | S_IRUSR);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  file = req.result;
  req.cleanup();

#ifndef _WIN32
  memset(&t, 0, sizeof(t));
  ASSERT(0 == fstat(file, &t));
  ASSERT(0 == req.fstat(file));
  ASSERT(req.result == 0);
  s = static_cast<uv_stat_t*>(req.ptr);
# if defined(__APPLE__)
  ASSERT(s->st_birthtim.tv_sec == t.st_birthtimespec.tv_sec);
  ASSERT(s->st_birthtim.tv_nsec == t.st_birthtimespec.tv_nsec);
# elif defined(__linux__)
  /* If statx() is supported, the birth time should be equal to the change time
   * because we just created the file. On older kernels, it's set to zero.
   */
  ASSERT((s->st_birthtim.tv_sec == 0 ||
         s->st_birthtim.tv_sec == t.st_ctim.tv_sec));
  ASSERT((s->st_birthtim.tv_nsec == 0 ||
         s->st_birthtim.tv_nsec == t.st_ctim.tv_nsec));
# endif
#endif

  iov = uv_buf_init(test_buf, sizeof(test_buf));
  r = req.write(file, { iov }, -1);
  ASSERT(r == sizeof(test_buf));
  ASSERT(req.result == sizeof(test_buf));
  req.cleanup();

  memset(&req.statbuf, 0xaa, sizeof(req.statbuf));
  r = req.fstat(file);
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  s = static_cast<uv_stat_t*>(req.ptr);
  ASSERT(s->st_size == sizeof(test_buf));

#ifndef _WIN32
  r = fstat(file, &t);
  ASSERT(r == 0);

  ASSERT(s->st_dev == static_cast<uint64_t>(t.st_dev));
  ASSERT(s->st_mode == static_cast<uint64_t>(t.st_mode));
  ASSERT(s->st_nlink == static_cast<uint64_t>(t.st_nlink));
  ASSERT(s->st_uid == static_cast<uint64_t>(t.st_uid));
  ASSERT(s->st_gid == static_cast<uint64_t>(t.st_gid));
  ASSERT(s->st_rdev == static_cast<uint64_t>(t.st_rdev));
  ASSERT(s->st_ino == static_cast<uint64_t>(t.st_ino));
  ASSERT(s->st_size == static_cast<uint64_t>(t.st_size));
  ASSERT(s->st_blksize == static_cast<uint64_t>(t.st_blksize));
  ASSERT(s->st_blocks == static_cast<uint64_t>(t.st_blocks));
#if defined(__APPLE__)
  ASSERT(s->st_atim.tv_sec == t.st_atimespec.tv_sec);
  ASSERT(s->st_atim.tv_nsec == t.st_atimespec.tv_nsec);
  ASSERT(s->st_mtim.tv_sec == t.st_mtimespec.tv_sec);
  ASSERT(s->st_mtim.tv_nsec == t.st_mtimespec.tv_nsec);
  ASSERT(s->st_ctim.tv_sec == t.st_ctimespec.tv_sec);
  ASSERT(s->st_ctim.tv_nsec == t.st_ctimespec.tv_nsec);
#elif defined(_AIX)    || \
      defined(__MVS__)
  ASSERT(s->st_atim.tv_sec == t.st_atime);
  ASSERT(s->st_atim.tv_nsec == 0);
  ASSERT(s->st_mtim.tv_sec == t.st_mtime);
  ASSERT(s->st_mtim.tv_nsec == 0);
  ASSERT(s->st_ctim.tv_sec == t.st_ctime);
  ASSERT(s->st_ctim.tv_nsec == 0);
#elif defined(__ANDROID__)
  ASSERT(s->st_atim.tv_sec == t.st_atime);
  ASSERT(s->st_atim.tv_nsec == t.st_atimensec);
  ASSERT(s->st_mtim.tv_sec == t.st_mtime);
  ASSERT(s->st_mtim.tv_nsec == t.st_mtimensec);
  ASSERT(s->st_ctim.tv_sec == t.st_ctime);
  ASSERT(s->st_ctim.tv_nsec == t.st_ctimensec);
#elif defined(__sun)           || \
      defined(__DragonFly__)   || \
      defined(__FreeBSD__)     || \
      defined(__OpenBSD__)     || \
      defined(__NetBSD__)      || \
      defined(_GNU_SOURCE)     || \
      defined(_BSD_SOURCE)     || \
      defined(_SVID_SOURCE)    || \
      defined(_XOPEN_SOURCE)   || \
      defined(_DEFAULT_SOURCE)
  ASSERT(s->st_atim.tv_sec == t.st_atim.tv_sec);
  ASSERT(s->st_atim.tv_nsec == t.st_atim.tv_nsec);
  ASSERT(s->st_mtim.tv_sec == t.st_mtim.tv_sec);
  ASSERT(s->st_mtim.tv_nsec == t.st_mtim.tv_nsec);
  ASSERT(s->st_ctim.tv_sec == t.st_ctim.tv_sec);
  ASSERT(s->st_ctim.tv_nsec == t.st_ctim.tv_nsec);
# if defined(__FreeBSD__)    || \
     defined(__NetBSD__)
  ASSERT(s->st_birthtim.tv_sec == t.st_birthtim.tv_sec);
  ASSERT(s->st_birthtim.tv_nsec == t.st_birthtim.tv_nsec);
# endif
#else
  ASSERT(s->st_atim.tv_sec == t.st_atime);
  ASSERT(s->st_atim.tv_nsec == 0);
  ASSERT(s->st_mtim.tv_sec == t.st_mtime);
  ASSERT(s->st_mtim.tv_nsec == 0);
  ASSERT(s->st_ctim.tv_sec == t.st_ctime);
  ASSERT(s->st_ctim.tv_nsec == 0);
#endif
#endif

#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__)
  ASSERT(s->st_flags == t.st_flags);
  ASSERT(s->st_gen == t.st_gen);
#else
  ASSERT(s->st_flags == 0);
  ASSERT(s->st_gen == 0);
#endif

  req.cleanup();

  /* Now do the uv_fs_fstat call asynchronously */
  r = req.fstat(loop, file, fstat_cb);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(fstat_cb_count == 1);


  r = req.close(file);
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  req.cleanup();

  /*
   * Run the loop just to check we don't have make any extraneous uv_ref()
   * calls. This should drop out immediately.
   */
  uv_run(loop, UV_RUN_DEFAULT);

  /* Cleanup. */
  unlink("test_file");

  make_valgrind_happy();
}


TEST_CASE("fs_fstat_stdio", "[fs]") {
  int fd;
  int res;
  ns_fs req;
#ifdef _WIN32
  uv_stat_t* st;
  DWORD ft;
#endif

  for (fd = 0; fd <= 2; ++fd) {
    res = req.fstat(fd);
    ASSERT(res == 0);
    ASSERT(req.result == 0);

#ifdef _WIN32
    st = req.ptr;
    ft = uv_guess_handle(fd);
    switch (ft) {
    case UV_TTY:
    case UV_NAMED_PIPE:
      ASSERT(st->st_mode == ft == UV_TTY ? S_IFCHR : S_IFIFO);
      ASSERT(st->st_nlink == 1);
      ASSERT(
          st->st_rdev ==
          (ft == UV_TTY ? FILE_DEVICE_CONSOLE : FILE_DEVICE_NAMED_PIPE) << 16);
      break;
    default:
      break;
    }
#endif

    req.cleanup();
  }

  make_valgrind_happy();
}


static void access_cb(ns_fs* req) {
  ASSERT(req->fs_type == UV_FS_ACCESS);
  access_cb_count++;
  req->cleanup();
}

TEST_CASE("fs_access", "[fs]") {
  int r;
  ns_fs req;
  uv_file file;
  uv_loop_t* loop;

  /* Setup. */
  unlink("test_file");
  rmdir("test_dir");

  loop = uv_default_loop();
  access_cb_count = 0;

  /* File should not exist */
  r = req.access("test_file", F_OK);
  ASSERT(r < 0);
  ASSERT(req.result < 0);
  req.cleanup();

  /* File should not exist */
  r = req.access(loop, "test_file", F_OK, access_cb);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(access_cb_count == 1);
  access_cb_count = 0; /* reset for the next test */

  /* Create file */
  r = req.open("test_file", O_RDWR | O_CREAT, S_IWUSR | S_IRUSR);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  file = req.result;
  req.cleanup();

  /* File should exist */
  r = req.access("test_file", F_OK);
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  req.cleanup();

  /* File should exist */
  r = req.access(loop, "test_file", F_OK, access_cb);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(access_cb_count == 1);
  access_cb_count = 0; /* reset for the next test */

  /* Close file */
  r = req.close(file);
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  req.cleanup();

  /* Directory access */
  r = req.mkdir("test_dir", 0777);
  ASSERT(r == 0);
  req.cleanup();

  r = req.access("test_dir", W_OK);
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  req.cleanup();

  /*
   * Run the loop just to check we don't have make any extraneous uv_ref()
   * calls. This should drop out immediately.
   */
  uv_run(loop, UV_RUN_DEFAULT);

  /* Cleanup. */
  unlink("test_file");
  rmdir("test_dir");

  make_valgrind_happy();
}


static void chmod_cb(ns_fs* req) {
  ASSERT(req->fs_type == UV_FS_CHMOD);
  ASSERT(req->result == 0);
  chmod_cb_count++;
  req->cleanup();
  check_permission("test_file", *req->get_data<int>());
}

static void fchmod_cb(ns_fs* req) {
  ASSERT(req->fs_type == UV_FS_FCHMOD);
  ASSERT(req->result == 0);
  fchmod_cb_count++;
  req->cleanup();
  check_permission("test_file", *req->get_data<int>());
}

TEST_CASE("fs_chmod", "[fs]") {
  int r;
  ns_fs req;
  uv_file file;
  uv_loop_t* loop;

  /* Setup. */
  unlink("test_file");

  loop = uv_default_loop();
  chmod_cb_count = 0;
  fchmod_cb_count = 0;

  r = req.open("test_file", O_RDWR | O_CREAT, S_IWUSR | S_IRUSR);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  file = req.result;
  req.cleanup();

  iov = uv_buf_init(test_buf, sizeof(test_buf));
  r = req.write(file, { iov }, -1);
  ASSERT(r == sizeof(test_buf));
  ASSERT(req.result == sizeof(test_buf));
  req.cleanup();

#ifndef _WIN32
  /* Make the file write-only */
  r = req.chmod("test_file", 0200);
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  req.cleanup();

  check_permission("test_file", 0200);
#endif

  /* Make the file read-only */
  r = req.chmod("test_file", 0400);
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  req.cleanup();

  check_permission("test_file", 0400);

  /* Make the file read+write with sync uv_fs_fchmod */
  r = req.fchmod(file, 0600);
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  req.cleanup();

  check_permission("test_file", 0600);

#ifndef _WIN32
  /* async chmod */
  {
    static int mode = 0200;
    req.data = &mode;
  }
  r = req.chmod(loop, "test_file", 0200, chmod_cb);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(chmod_cb_count == 1);
  chmod_cb_count = 0; /* reset for the next test */
#endif

  /* async chmod */
  {
    static int mode = 0400;
    req.data = &mode;
  }
  r = req.chmod(loop, "test_file", 0400, chmod_cb);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(chmod_cb_count == 1);

  /* async fchmod */
  {
    static int mode = 0600;
    req.data = &mode;
  }
  r = req.fchmod(loop, file, 0600, fchmod_cb);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(fchmod_cb_count == 1);

  r = req.close(file);

  /*
   * Run the loop just to check we don't have make any extraneous uv_ref()
   * calls. This should drop out immediately.
   */
  uv_run(loop, UV_RUN_DEFAULT);

  /* Cleanup. */
  unlink("test_file");

  make_valgrind_happy();
}


TEST_CASE("fs_unlink_readonly", "[fs]") {
  int r;
  ns_fs req;
  uv_file file;
  uv_loop_t* loop;

  /* Setup. */
  unlink("test_file");

  loop = uv_default_loop();

  r = req.open("test_file", O_RDWR | O_CREAT, S_IWUSR | S_IRUSR);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  file = req.result;
  req.cleanup();

  iov = uv_buf_init(test_buf, sizeof(test_buf));
  r = req.write(file, { iov }, -1);
  ASSERT(r == sizeof(test_buf));
  ASSERT(req.result == sizeof(test_buf));
  req.cleanup();

  r = req.close(file);

  /* Make the file read-only */
  r = req.chmod("test_file", 0400);
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  req.cleanup();

  check_permission("test_file", 0400);

  /* Try to unlink the file */
  r = req.unlink("test_file");
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  req.cleanup();

  /*
  * Run the loop just to check we don't have make any extraneous uv_ref()
  * calls. This should drop out immediately.
  */
  uv_run(loop, UV_RUN_DEFAULT);

  /* Cleanup. */
  r = req.chmod("test_file", 0600);
  req.cleanup();
  unlink("test_file");

  make_valgrind_happy();
}


static void chown_cb(ns_fs* req) {
  ASSERT(req->fs_type == UV_FS_CHOWN);
  ASSERT(req->result == 0);
  chown_cb_count++;
  req->cleanup();
}

static void chown_root_cb(ns_fs* req) {
  ASSERT(req->fs_type == UV_FS_CHOWN);
#if defined(_WIN32) || defined(__MSYS__)
  /* On windows, chown is a no-op and always succeeds. */
  ASSERT(req->result == 0);
#else
  /* On unix, chown'ing the root directory is not allowed -
   * unless you're root, of course.
   */
  if (geteuid() == 0)
    ASSERT(req->result == 0);
  else
#   if defined(__CYGWIN__)
    /* On Cygwin, uid 0 is invalid (no root). */
    ASSERT(req->result == UV_EINVAL);
#   elif defined(__PASE__)
    /* On IBMi PASE, there is no root user. uid 0 is user qsecofr.
     * User may grant qsecofr's privileges, including changing
     * the file's ownership to uid 0.
     */
    ASSERT(req->result == 0 || req->result == UV_EPERM);
#   else
    ASSERT(req->result == UV_EPERM);
#   endif
#endif
  chown_cb_count++;
  req->cleanup();
}

static void fchown_cb(ns_fs* req) {
  ASSERT(req->fs_type == UV_FS_FCHOWN);
  ASSERT(req->result == 0);
  fchown_cb_count++;
  req->cleanup();
}

static void lchown_cb(ns_fs* req) {
  ASSERT(req->fs_type == UV_FS_LCHOWN);
  ASSERT(req->result == 0);
  lchown_cb_count++;
  req->cleanup();
}

TEST_CASE("fs_chown", "[fs]") {
  int r;
  ns_fs req;
  uv_file file;
  uv_loop_t* loop;

  /* Setup. */
  unlink("test_file");
  unlink("test_file_link");

  loop = uv_default_loop();
  chown_cb_count = 0;
  fchown_cb_count = 0;
  lchown_cb_count = 0;

  r = req.open("test_file", O_RDWR | O_CREAT, S_IWUSR | S_IRUSR);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  file = req.result;
  req.cleanup();

  /* sync chown */
  r = req.chown("test_file", -1, -1);
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  req.cleanup();

  /* sync fchown */
  r = req.fchown(file, -1, -1);
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  req.cleanup();

  /* async chown */
  r = req.chown(loop, "test_file", -1, -1, chown_cb);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(chown_cb_count == 1);

#ifndef __MVS__
  /* chown to root (fail) */
  chown_cb_count = 0;
  r = req.chown(loop, "test_file", 0, 0, chown_root_cb);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(chown_cb_count == 1);
#endif

  /* async fchown */
  r = req.fchown(loop, file, -1, -1, fchown_cb);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(fchown_cb_count == 1);

#ifndef __HAIKU__
  /* Haiku doesn't support hardlink */
  /* sync link */
  r = req.link("test_file", "test_file_link");
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  req.cleanup();

  /* sync lchown */
  r = req.lchown("test_file_link", -1, -1);
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  req.cleanup();

  /* async lchown */
  r = req.lchown(loop, "test_file_link", -1, -1, lchown_cb);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(lchown_cb_count == 1);
#endif

  /* Close file */
  r = req.close(file);
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  req.cleanup();

  /*
   * Run the loop just to check we don't have make any extraneous uv_ref()
   * calls. This should drop out immediately.
   */
  uv_run(loop, UV_RUN_DEFAULT);

  /* Cleanup. */
  unlink("test_file");
  unlink("test_file_link");

  make_valgrind_happy();
}


static void link_cb(ns_fs* req) {
  ASSERT(req->fs_type == UV_FS_LINK);
  ASSERT(req->result == 0);
  link_cb_count++;
  req->cleanup();
}

TEST_CASE("fs_link", "[fs]") {
  int r;
  ns_fs req;
  uv_file file;
  uv_file link;
  uv_loop_t* loop;

  /* Setup. */
  unlink("test_file");
  unlink("test_file_link");
  unlink("test_file_link2");

  loop = uv_default_loop();
  link_cb_count = 0;

  r = req.open("test_file", O_RDWR | O_CREAT, S_IWUSR | S_IRUSR);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  file = req.result;
  req.cleanup();

  r = req.write(file, { uv_buf_init(test_buf, sizeof(test_buf)) }, -1);
  ASSERT(r == sizeof(test_buf));
  ASSERT(req.result == sizeof(test_buf));
  req.cleanup();

  r = req.close(file);

  /* sync link */
  r = req.link("test_file", "test_file_link");
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  req.cleanup();

  r = req.open("test_file_link", O_RDWR, 0);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  link = req.result;
  req.cleanup();

  memset(buf, 0, sizeof(buf));
  r = req.read(link, { uv_buf_init(buf, sizeof(buf)) }, 0);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);

  close(link);

  /* async link */
  r = req.link(loop, "test_file", "test_file_link2", link_cb);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(link_cb_count == 1);

  r = req.open("test_file_link2", O_RDWR, 0);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  link = req.result;
  req.cleanup();

  memset(buf, 0, sizeof(buf));
  iov = uv_buf_init(buf, sizeof(buf));
  r = req.read(link, &iov, 1, 0);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);

  r = req.close(link);

  /*
   * Run the loop just to check we don't have make any extraneous uv_ref()
   * calls. This should drop out immediately.
   */
  uv_run(loop, UV_RUN_DEFAULT);

  /* Cleanup. */
  unlink("test_file");
  unlink("test_file_link");
  unlink("test_file_link2");

  make_valgrind_happy();
}


static void dummy_cb(ns_fs*) {
  dummy_cb_count++;
}


TEST_CASE("fs_readlink", "[fs]") {
  /* Must return UV_ENOENT on an inexistent file */
  {
    ns_fs req;
    uv_loop_t* loop;

    loop = uv_default_loop();
    dummy_cb_count = 0;
    ASSERT(0 == req.readlink(loop, "no_such_file", dummy_cb));
    ASSERT(0 == uv_run(loop, UV_RUN_DEFAULT));
    ASSERT(dummy_cb_count == 1);
    ASSERT_NULL(req.ptr);
    ASSERT(req.result == UV_ENOENT);
    req.cleanup();

    ASSERT(UV_ENOENT == req.readlink("no_such_file"));
    ASSERT_NULL(req.ptr);
    ASSERT(req.result == UV_ENOENT);
    req.cleanup();
  }

  /* Must return UV_EINVAL on a non-symlink file */
  {
    int r;
    ns_fs req;
    uv_file file;

    /* Setup */

    /* Create a non-symlink file */
    r = req.open("test_file", O_RDWR | O_CREAT, S_IWUSR | S_IRUSR);
    ASSERT_GE(r, 0);
    ASSERT_GE(req.result, 0);
    file = req.result;
    req.cleanup();

    r = req.close(file);
    ASSERT_EQ(r, 0);
    ASSERT_EQ(req.result, 0);
    req.cleanup();

    /* Test */
    r = req.readlink("test_file");
    ASSERT_EQ(r, UV_EINVAL);
    req.cleanup();

    /* Cleanup */
    unlink("test_file");
  }

  make_valgrind_happy();
}


TEST_CASE("fs_realpath", "[fs]") {
  ns_fs req;
  uv_loop_t* loop;

  loop = uv_default_loop();
  dummy_cb_count = 0;
  ASSERT(0 == req.realpath(loop, "no_such_file", dummy_cb));
  ASSERT(0 == uv_run(loop, UV_RUN_DEFAULT));
  ASSERT(dummy_cb_count == 1);
  ASSERT_NULL(req.ptr);
  ASSERT(req.result == UV_ENOENT);
  req.cleanup();

  ASSERT(UV_ENOENT == req.realpath("no_such_file"));
  ASSERT_NULL(req.ptr);
  ASSERT(req.result == UV_ENOENT);
  req.cleanup();

  make_valgrind_happy();
}


static void symlink_cb(ns_fs* req) {
  ASSERT(req->fs_type == UV_FS_SYMLINK);
  ASSERT(req->result == 0);
  symlink_cb_count++;
  req->cleanup();
}

static void readlink_cb(ns_fs* req) {
  ASSERT(req->fs_type == UV_FS_READLINK);
  ASSERT(req->result == 0);
  readlink_cb_count++;
  req->cleanup();
}

static void realpath_cb(ns_fs* req) {
  char test_file_abs_buf[PATHMAX];
  size_t test_file_abs_size = sizeof(test_file_abs_buf);
  ASSERT(req->fs_type == UV_FS_REALPATH);
  ASSERT(req->result == 0);

  uv_cwd(test_file_abs_buf, &test_file_abs_size);
#ifdef _WIN32
  strcat(test_file_abs_buf, "\\test_file"); // NOLINT
  ASSERT(stricmp(req->ptr, test_file_abs_buf) == 0);
#else
  strcat(test_file_abs_buf, "/test_file"); // NOLINT
  // ASSERT(strcmp(req->ptr, test_file_abs_buf) == 0);
#endif
  realpath_cb_count++;
  req->cleanup();
}

TEST_CASE("fs_symlink", "[fs]") {
  int r;
  ns_fs req;
  uv_file file;
  uv_file link;
  uv_loop_t* loop;
  char test_file_abs_buf[PATHMAX];
  size_t test_file_abs_size;

  /* Setup. */
  symlink_cb_count = 0;
  readlink_cb_count = 0;
  realpath_cb_count = 0;
  unlink("test_file");
  unlink("test_file_symlink");
  unlink("test_file_symlink2");
  unlink("test_file_symlink_symlink");
  unlink("test_file_symlink2_symlink");
  test_file_abs_size = sizeof(test_file_abs_buf);
#ifdef _WIN32
  uv_cwd(test_file_abs_buf, &test_file_abs_size);
  strcat(test_file_abs_buf, "\\test_file"); // NOLINT
#else
  uv_cwd(test_file_abs_buf, &test_file_abs_size);
  strcat(test_file_abs_buf, "/test_file"); // NOLINT
#endif

  loop = uv_default_loop();

  r = req.open("test_file", O_RDWR | O_CREAT, S_IWUSR | S_IRUSR);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  file = req.result;
  req.cleanup();

  iov = uv_buf_init(test_buf, sizeof(test_buf));
  r = req.write(file, &iov, 1, -1);
  ASSERT(r == sizeof(test_buf));
  ASSERT(req.result == sizeof(test_buf));
  req.cleanup();

  r = req.close(file);

  /* sync symlink */
  r = req.symlink("test_file", "test_file_symlink", 0);
#ifdef _WIN32
  if (r < 0) {
    if (r == UV_ENOTSUP) {
      /*
       * Windows doesn't support symlinks on older versions.
       * We just pass the test and bail out early if we get ENOTSUP.
       */
      return 0;
    } else if (r == UV_EPERM) {
      /*
       * Creating a symlink is only allowed when running elevated.
       * We pass the test and bail out early if we get UV_EPERM.
       */
      return 0;
    }
  }
#endif
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  req.cleanup();

  r = req.open("test_file_symlink", O_RDWR, 0);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  link = req.result;
  req.cleanup();

  memset(buf, 0, sizeof(buf));
  iov = uv_buf_init(buf, sizeof(buf));
  r = req.read(link, &iov, 1, 0);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);

  r = req.close(link);

  r = req.symlink("test_file_symlink", "test_file_symlink_symlink", 0);
  ASSERT(r == 0);
  req.cleanup();

#if defined(__MSYS__)
  RETURN_SKIP("symlink reading is not supported on MSYS2");
#endif

  r = req.readlink("test_file_symlink_symlink");
  ASSERT(r == 0);
  req.cleanup();

  r = req.realpath("test_file_symlink_symlink");
  ASSERT(r == 0);
#ifdef _WIN32
  ASSERT(stricmp(req.ptr, test_file_abs_buf) == 0);
#else
  // ASSERT(strcmp(req.ptr, test_file_abs_buf) == 0);
#endif
  req.cleanup();

  /* async link */
  r = req.symlink(loop, "test_file", "test_file_symlink2", 0, symlink_cb);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(symlink_cb_count == 1);

  r = req.open("test_file_symlink2", O_RDWR, 0);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  link = req.result;
  req.cleanup();

  memset(buf, 0, sizeof(buf));
  iov = uv_buf_init(buf, sizeof(buf));
  r = req.read(link, &iov, 1, 0);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);

  r = req.close(link);

  r = req.symlink("test_file_symlink2", "test_file_symlink2_symlink", 0);
  ASSERT(r == 0);
  req.cleanup();

  r = req.readlink(loop, "test_file_symlink2_symlink", readlink_cb);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(readlink_cb_count == 1);

  r = req.realpath(loop, "test_file", realpath_cb);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(realpath_cb_count == 1);

  /*
   * Run the loop just to check we don't have make any extraneous uv_ref()
   * calls. This should drop out immediately.
   */
  uv_run(loop, UV_RUN_DEFAULT);

  /* Cleanup. */
  unlink("test_file");
  unlink("test_file_symlink");
  unlink("test_file_symlink_symlink");
  unlink("test_file_symlink2");
  unlink("test_file_symlink2_symlink");

  make_valgrind_happy();
}


static void test_symlink_dir_impl(int type) {
  ns_fs close_req;
  ns_fs open_req;
  ns_fs req;
  ns_fs scandir_req;
  int r;
  std::string test_dir;
  uv_dirent_t dent;
  static char test_dir_abs_buf[PATHMAX];
  size_t test_dir_abs_size;

  /* set-up */
  unlink("test_dir/file1");
  unlink("test_dir/file2");
  rmdir("test_dir");
  rmdir("test_dir_symlink");
  test_dir_abs_size = sizeof(test_dir_abs_buf);

  r = req.mkdir("test_dir", 0777);
  req.cleanup();

#ifdef _WIN32
  strcpy(test_dir_abs_buf, "\\\\?\\"); // NOLINT
  uv_cwd(test_dir_abs_buf + 4, &test_dir_abs_size);
  test_dir_abs_size += 4;
  strcat(test_dir_abs_buf, "\\test_dir\\"); // NOLINT
  test_dir_abs_size += strlen("\\test_dir\\");
  test_dir = test_dir_abs_buf;
#else
  uv_cwd(test_dir_abs_buf, &test_dir_abs_size);
  strcat(test_dir_abs_buf, "/test_dir"); // NOLINT
  test_dir_abs_size += strlen("/test_dir");
  test_dir = "test_dir";
#endif

  r = req.symlink(test_dir.c_str(), "test_dir_symlink", type);
  if (type == UV_FS_SYMLINK_DIR && (r == UV_ENOTSUP || r == UV_EPERM)) {
    req.cleanup();
    RETURN_SKIP("this version of Windows doesn't support unprivileged creation of directory symlinks"); // NOLINT
  }
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  req.cleanup();

  r = req.stat("test_dir_symlink");
  ASSERT(r == 0);
  ASSERT(static_cast<uv_stat_t*>(req.ptr)->st_mode & S_IFDIR);
  req.cleanup();

  r = req.lstat("test_dir_symlink");
  ASSERT(r == 0);
#if defined(__MSYS__)
  RETURN_SKIP("symlink reading is not supported on MSYS2");
#endif
  ASSERT(static_cast<uv_stat_t*>(req.ptr)->st_mode & S_IFLNK);
#ifdef _WIN32
  ASSERT(static_cast<uv_stat_t*>(req.ptr)->st_size ==
         strlen(test_dir.c_str() + 4));
#else
# ifdef __PASE__
  /* On IBMi PASE, st_size returns the length of the symlink itself. */
  ASSERT(static_cast<uv_stat_t*>(req.ptr)->st_size ==
         strlen("test_dir_symlink"));
# else
  ASSERT(static_cast<uv_stat_t*>(req.ptr)->st_size == strlen(test_dir.c_str()));
# endif
#endif
  req.cleanup();

  r = req.readlink("test_dir_symlink");
  ASSERT(r == 0);
#ifdef _WIN32
  // ASSERT(strcmp(req.ptr, test_dir.c_str() + 4) == 0);
#else
  // ASSERT(strcmp(req.ptr, test_dir.c_str()) == 0);
#endif
  req.cleanup();

  r = req.realpath("test_dir_symlink");
  ASSERT(r == 0);
#ifdef _WIN32
  ASSERT(strlen(req.ptr) == test_dir_abs_size - 5);
  ASSERT(strnicmp(req.ptr, test_dir.c_str() + 4, test_dir_abs_size - 5) == 0);
#else
  // ASSERT(strcmp(req.ptr, test_dir_abs_buf) == 0);
#endif
  req.cleanup();

  r = open_req.open("test_dir/file1", O_WRONLY | O_CREAT, S_IWUSR | S_IRUSR);
  ASSERT(r >= 0);
  open_req.cleanup();
  r = close_req.close(open_req.result);
  ASSERT(r == 0);
  close_req.cleanup();

  r = open_req.open("test_dir/file2", O_WRONLY | O_CREAT, S_IWUSR | S_IRUSR);
  ASSERT(r >= 0);
  open_req.cleanup();
  r = close_req.close(open_req.result);
  ASSERT(r == 0);
  close_req.cleanup();

  r = scandir_req.scandir("test_dir_symlink", 0);
  ASSERT(r == 2);
  ASSERT(scandir_req.result == 2);
  ASSERT(scandir_req.ptr);
  while (UV_EOF != scandir_req.scandir_next(&dent)) {
    assert_is_file_type(dent);
  }
  scandir_req.cleanup();
  ASSERT(!scandir_req.ptr);

  /* unlink will remove the directory symlink */
  r = req.unlink("test_dir_symlink");
  ASSERT(r == 0);
  req.cleanup();

  r = scandir_req.scandir("test_dir_symlink", 0);
  ASSERT(r == UV_ENOENT);
  scandir_req.cleanup();

  r = scandir_req.scandir("test_dir", 0);
  ASSERT(r == 2);
  ASSERT(scandir_req.result == 2);
  ASSERT(scandir_req.ptr);
  while (UV_EOF != uv_fs_scandir_next(&scandir_req, &dent)) {
    assert_is_file_type(dent);
  }
  scandir_req.cleanup();
  ASSERT(!scandir_req.ptr);

  /* clean-up */
  unlink("test_dir/file1");
  unlink("test_dir/file2");
  rmdir("test_dir");
  rmdir("test_dir_symlink");

  make_valgrind_happy();
}

TEST_CASE("fs_symlink_dir", "[fs]") {
  test_symlink_dir_impl(UV_FS_SYMLINK_DIR);
}

TEST_CASE("fs_symlink_junction", "[fs]") {
  test_symlink_dir_impl(UV_FS_SYMLINK_JUNCTION);
}


static void utime_cb(ns_fs* req, utime_check_t* c) {
  ASSERT(req->result == 0);
  ASSERT(req->fs_type == UV_FS_UTIME);
  ASSERT(req->get_data<utime_check_t>() == c);

  check_utime(c->path, c->atime, c->mtime, /* test_lutime */ 0);

  req->cleanup();
  utime_cb_count++;
}


TEST_CASE("fs_utime", "[fs]") {
  utime_check_t checkme;
  const char* path = "test_file";
  double atime;
  double mtime;
  ns_fs utime_req;
  ns_fs req;
  uv_loop_t* loop;
  int r;

  /* Setup. */
  loop = uv_default_loop();
  utime_cb_count = 0;
  unlink(path);
  r = req.open(path, O_RDWR | O_CREAT, S_IWUSR | S_IRUSR);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  req.cleanup();
  ASSERT_EQ(0, req.close(r));

  atime = mtime = 400497753.25; /* 1982-09-10 11:22:33.25 */

  r = req.utime(path, atime, mtime);
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  req.cleanup();

  check_utime(path, atime, mtime, /* test_lutime */ 0);

  atime = mtime = 1291404900.25; /* 2010-12-03 20:35:00.25 - mees <3 */
  checkme.path = path;
  checkme.atime = atime;
  checkme.mtime = mtime;

  /* async utime */
  utime_req.data = &checkme;
  r = utime_req.utime(loop, path, atime, mtime, utime_cb, &checkme);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(utime_cb_count == 1);

  /* Cleanup. */
  unlink(path);

  make_valgrind_happy();
}


TEST_CASE("fs_utime_round", "[fs]") {
  const char path[] = "test_file";
  double atime;
  double mtime;
  ns_fs req;
  int r;

  unlink(path);
  r = req.open(path, O_RDWR | O_CREAT, S_IWUSR | S_IRUSR);
  ASSERT_GE(r, 0);
  ASSERT_GE(req.result, 0);
  req.cleanup();
  ASSERT_EQ(0, req.close(r));

  atime = mtime = -14245440.25;  /* 1969-07-20T02:56:00.25Z */

  r = req.utime(path, atime, mtime);
#if !defined(__linux__)     && \
    !defined(_WIN32)        && \
    !defined(__APPLE__)     && \
    !defined(__FreeBSD__)   && \
    !defined(__sun)
  if (r != 0) {
    ASSERT_EQ(r, UV_EINVAL);
    RETURN_SKIP("utime on some OS (z/OS, IBM i PASE, AIX) or filesystems may reject pre-epoch timestamps"); // NOLINT
  }
#endif
  ASSERT_EQ(0, r);
  ASSERT_EQ(0, req.result);
  req.cleanup();
  check_utime(path, atime, mtime, /* test_lutime */ 0);
  unlink(path);

  make_valgrind_happy();
}


static void futime_cb(ns_fs* req, utime_check_t* c) {
  ASSERT(req->result == 0);
  ASSERT(req->fs_type == UV_FS_FUTIME);
  ASSERT(req->get_data<utime_check_t>() == c);

  check_utime(c->path, c->atime, c->mtime, /* test_lutime */ 0);

  req->cleanup();
  futime_cb_count++;
}


TEST_CASE("fs_futime", "[fs]") {
  utime_check_t checkme;
  const char* path = "test_file";
  double atime;
  double mtime;
  uv_file file;
  ns_fs futime_req;
  ns_fs req;
  uv_loop_t* loop;
  int r;
#if defined(_AIX) && !defined(_AIX71)
  RETURN_SKIP("futime is not implemented for AIX versions below 7.1");
#endif

  /* Setup. */
  loop = uv_default_loop();
  futime_cb_count = 0;
  unlink(path);
  r = req.open(path, O_RDWR | O_CREAT, S_IWUSR | S_IRUSR);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  req.cleanup();
  r = req.close(r);
  ASSERT(r == 0);

  atime = mtime = 400497753.25; /* 1982-09-10 11:22:33.25 */

  r = req.open(path, O_RDWR, 0);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  file = req.result; /* FIXME probably not how it's supposed to be used */
  req.cleanup();

  r = req.futime(file, atime, mtime);
#if defined(__CYGWIN__) || defined(__MSYS__)
  ASSERT(r == UV_ENOSYS);
  RETURN_SKIP("futime not supported on Cygwin");
#else
  ASSERT(r == 0);
  ASSERT(req.result == 0);
#endif
  req.cleanup();

  check_utime(path, atime, mtime, /* test_lutime */ 0);

  atime = mtime = 1291404900; /* 2010-12-03 20:35:00 - mees <3 */

  checkme.atime = atime;
  checkme.mtime = mtime;
  checkme.path = path;

  /* async futime */
  futime_req.data = &checkme;
  r = futime_req.futime(loop, file, atime, mtime, futime_cb, &checkme);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(futime_cb_count == 1);

  /* Cleanup. */
  unlink(path);
  futime_req.cleanup();

  make_valgrind_happy();
}


static void lutime_cb(ns_fs* req, utime_check_t* c) {
  ASSERT(req->result == 0);
  ASSERT(req->fs_type == UV_FS_LUTIME);
  ASSERT(req->get_data<utime_check_t>() == c);

  check_utime(c->path, c->atime, c->mtime, /* test_lutime */ 1);

  uv_fs_req_cleanup(req);
  lutime_cb_count++;
}


TEST_CASE("fs_lutime", "[fs]") {
  utime_check_t checkme;
  const char* path = "test_file";
  const char* symlink_path = "test_file_symlink";
  double atime;
  double mtime;
  ns_fs req;
  uv_loop_t* loop;
  int r, s;


  /* Setup */
  loop = uv_default_loop();
  lutime_cb_count = 0;
  unlink(path);
  r = req.open(path, O_RDWR | O_CREAT, S_IWUSR | S_IRUSR);
  ASSERT_GE(r, 0);
  ASSERT_GE(req.result, 0);
  req.cleanup();
  r = req.close(r);
  ASSERT_EQ(r, 0);

  unlink(symlink_path);
  s = req.symlink(path, symlink_path, 0);
#ifdef _WIN32
  if (s == UV_EPERM) {
    /*
     * Creating a symlink before Windows 10 Creators Update was only allowed
     * when running elevated console (with admin rights)
     */
    RETURN_SKIP(
        "Symlink creation requires elevated console (with admin rights)");
  }
#endif
  ASSERT_EQ(s, 0);
  ASSERT_EQ(req.result, 0);
  req.cleanup();

  /* Test the synchronous version. */
  atime = mtime = 400497753.25; /* 1982-09-10 11:22:33.25 */

  checkme.atime = atime;
  checkme.mtime = mtime;
  checkme.path = symlink_path;
  req.data = &checkme;

  r = req.lutime(symlink_path, atime, mtime);
#if (defined(_AIX) && !defined(_AIX71)) ||                                    \
     defined(__MVS__)
  ASSERT_EQ(r, UV_ENOSYS);
  RETURN_SKIP("lutime is not implemented for z/OS and AIX versions below 7.1");
#endif
  ASSERT_EQ(r, 0);
  lutime_cb(&req, &checkme);
  ASSERT_EQ(lutime_cb_count, 1);

  /* Test the asynchronous version. */
  atime = mtime = 1291404900; /* 2010-12-03 20:35:00 */

  checkme.atime = atime;
  checkme.mtime = mtime;
  checkme.path = symlink_path;

  r = req.lutime(loop, symlink_path, atime, mtime, lutime_cb, &checkme);
  ASSERT_EQ(r, 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT_EQ(lutime_cb_count, 2);

  /* Cleanup. */
  unlink(path);
  unlink(symlink_path);

  make_valgrind_happy();
}


TEST_CASE("fs_stat_missing_path", "[fs]") {
  ns_fs req;
  int r;

  r = req.stat("non_existent_file");
  ASSERT(r == UV_ENOENT);
  ASSERT(req.result == UV_ENOENT);
  req.cleanup();

  make_valgrind_happy();
}


static void empty_scandir_cb(ns_fs* req, ns_fs* scandir_req) {
  uv_dirent_t dent;

  ASSERT(req == scandir_req);
  ASSERT(req->fs_type == UV_FS_SCANDIR);
  ASSERT(req->result == 0);
  ASSERT_NULL(req->ptr);
  ASSERT(UV_EOF == req->scandir_next(&dent));
  req->cleanup();
  scandir_cb_count++;
}


TEST_CASE("fs_scandir_empty_dir", "[fs]") {
  const char* path;
  ns_fs req;
  ns_fs scandir_req;
  uv_dirent_t dent;
  uv_loop_t* loop;
  int r;

  loop = uv_default_loop();
  scandir_cb_count = 0;
  path = "./empty_dir/";

  r = req.mkdir(path, 0777);
  req.cleanup();

  /* Fill the req to ensure that required fields are cleaned up */
  memset(req.uv_req(), 0xdb, sizeof(*req.uv_req()));

  r = req.scandir(path, 0);
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  ASSERT_NULL(req.ptr);
  ASSERT(UV_EOF == req.scandir_next(&dent));
  req.cleanup();

  r = scandir_req.scandir(loop, path, 0, empty_scandir_cb, &scandir_req);
  ASSERT(r == 0);

  ASSERT(scandir_cb_count == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(scandir_cb_count == 1);

  r = req.rmdir(path);
  req.cleanup();
  scandir_req.cleanup();

  make_valgrind_happy();
}


static void non_existent_scandir_cb(ns_fs* req, ns_fs* scandir_req) {
  uv_dirent_t dent;

  ASSERT(req == scandir_req);
  ASSERT(req->fs_type == UV_FS_SCANDIR);
  ASSERT(req->result == UV_ENOENT);
  ASSERT_NULL(req->ptr);
  ASSERT(UV_ENOENT == req->scandir_next(&dent));
  req->cleanup();
  scandir_cb_count++;
}

TEST_CASE("fs_scandir_non_existent_dir", "[fs]") {
  uv_loop_t* loop;
  ns_fs req;
  uv_dirent_t dent;
  const char path[] = "./non_existent_dir/";
  int r;

  loop = uv_default_loop();
  scandir_cb_count = 0;

  r = req.rmdir(path);
  req.cleanup();

  /* Fill the req to ensure that required fields are cleaned up */
  memset(req.uv_req(), 0xdb, sizeof(*req.uv_req()));

  r = req.scandir(path, 0);
  ASSERT(r == UV_ENOENT);
  ASSERT(req.result == UV_ENOENT);
  ASSERT_NULL(req.ptr);
  ASSERT(UV_ENOENT == req.scandir_next(&dent));
  req.cleanup();

  r = req.scandir(loop, path, 0, non_existent_scandir_cb, &req);
  ASSERT(r == 0);

  ASSERT(scandir_cb_count == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(scandir_cb_count == 1);

  make_valgrind_happy();
}


static void file_scandir_cb(ns_fs* req, ns_fs* scandir_req) {
  ASSERT(req == scandir_req);
  ASSERT(req->fs_type == UV_FS_SCANDIR);
  ASSERT(req->result == UV_ENOTDIR);
  ASSERT_NULL(req->ptr);
  req->cleanup();
  scandir_cb_count++;
}

TEST_CASE("fs_scandir_file", "[fs]") {
  ns_fs scandir_req;
  uv_loop_t* loop;
  const char path[] = "test/fixtures/empty_file";
  int r;

  loop = uv_default_loop();
  scandir_cb_count = 0;

  r = scandir_req.scandir(path, 0);
  ASSERT(r == UV_ENOTDIR);
  scandir_req.cleanup();

  r = scandir_req.scandir(loop, path, 0, file_scandir_cb, &scandir_req);
  ASSERT(r == 0);

  ASSERT(scandir_cb_count == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(scandir_cb_count == 1);

  make_valgrind_happy();
}


/* Run in Valgrind. Should not leak when the iterator isn't exhausted. */
TEST_CASE("fs_scandir_early_exit", "[fs]") {
  uv_dirent_t d;
  ns_fs req;

  ASSERT_LT(0, req.scandir("test/fixtures/one_file", 0));
  ASSERT_NE(UV_EOF, req.scandir_next(&d));
  req.cleanup();

  ASSERT_LT(0, req.scandir("test/fixtures", 0));
  ASSERT_NE(UV_EOF, req.scandir_next(&d));
  req.cleanup();

  make_valgrind_happy();
}


static void open_cb_simple(ns_fs* req, ns_fs* open_req) {
  ASSERT(req->fs_type == UV_FS_OPEN);
  ASSERT_EQ(req, open_req);
  if (req->result < 0) {
    fprintf(stderr, "async open error: %d\n", static_cast<int>(req->result));
    FAIL("error");
  }
  open_cb_count++;
  ASSERT(req->path);
  req->cleanup();
}

TEST_CASE("fs_open_dir", "[fs]") {
  ns_fs close_req;
  ns_fs open_req;
  uv_loop_t* loop;
  const char path[] = ".";
  int r;
  int file;

  loop = uv_default_loop();
  open_cb_count = 0;

  r = open_req.open(path, O_RDONLY, 0);
  ASSERT(r >= 0);
  ASSERT(open_req.result >= 0);
  ASSERT_NULL(open_req.ptr);
  file = r;
  open_req.cleanup();

  r = close_req.close(file);
  ASSERT(r == 0);
  close_req.cleanup();

  r = open_req.open(loop, path, O_RDONLY, 0, open_cb_simple, &open_req);
  ASSERT(r == 0);

  ASSERT(open_cb_count == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(open_cb_count == 1);

  make_valgrind_happy();
}


static void fs_file_open_append(int add_flags) {
  ns_fs close_req;
  ns_fs open_req;
  ns_fs read_req;
  ns_fs write_req;
  uv_buf_t iov;
  int r;

  /* Setup. */
  unlink("test_file");

  r = open_req.open("test_file",
                    O_WRONLY | O_CREAT | add_flags,
                    S_IWUSR | S_IRUSR);
  ASSERT(r >= 0);
  ASSERT(open_req.result >= 0);
  open_req.cleanup();

  iov = uv_buf_init(test_buf, sizeof(test_buf));
  r = write_req.write(open_req.result, &iov, 1, -1);
  ASSERT(r >= 0);
  ASSERT(write_req.result >= 0);
  write_req.cleanup();

  r = close_req.close(open_req.result);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  close_req.cleanup();

  r = open_req.open("test_file", O_RDWR | O_APPEND | add_flags, 0);
  ASSERT(r >= 0);
  ASSERT(open_req.result >= 0);
  open_req.cleanup();

  iov = uv_buf_init(test_buf, sizeof(test_buf));
  r = write_req.write(open_req.result, &iov, 1, -1);
  ASSERT(r >= 0);
  ASSERT(write_req.result >= 0);
  write_req.cleanup();

  r = close_req.close(open_req.result);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  close_req.cleanup();

  r = open_req.open("test_file", O_RDONLY | add_flags, S_IRUSR);
  ASSERT(r >= 0);
  ASSERT(open_req.result >= 0);
  open_req.cleanup();

  iov = uv_buf_init(buf, sizeof(buf));
  r = read_req.read(open_req.result, &iov, 1, -1);
  ASSERT(r == 26);
  ASSERT(read_req.result == 26);
  ASSERT(memcmp(buf,
                "test-buffer\n\0test-buffer\n\0",
                sizeof("test-buffer\n\0test-buffer\n\0") - 1) == 0);
  read_req.cleanup();

  r = close_req.close(open_req.result);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  close_req.cleanup();

  /* Cleanup */
  unlink("test_file");
}

TEST_CASE("fs_file_open_append", "[fs]") {
  fs_file_open_append(0);
  fs_file_open_append(UV_FS_O_FILEMAP);

  make_valgrind_happy();
}


TEST_CASE("fs_rename_to_existing_file", "[fs]") {
  ns_fs close_req;
  ns_fs open_req;
  ns_fs read_req;
  ns_fs rename_req;
  ns_fs write_req;
  uv_buf_t iov;
  int r;

  /* Setup. */
  unlink("test_file");
  unlink("test_file2");

  r = open_req.open("test_file", O_WRONLY | O_CREAT, S_IWUSR | S_IRUSR);
  ASSERT(r >= 0);
  ASSERT(open_req.result >= 0);
  open_req.cleanup();

  iov = uv_buf_init(test_buf, sizeof(test_buf));
  r = write_req.write(open_req.result, &iov, 1, -1);
  ASSERT(r >= 0);
  ASSERT(write_req.result >= 0);
  write_req.cleanup();

  r = close_req.close(open_req.result);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  close_req.cleanup();

  r = open_req.open("test_file2", O_WRONLY | O_CREAT, S_IWUSR | S_IRUSR);
  ASSERT(r >= 0);
  ASSERT(open_req.result >= 0);
  open_req.cleanup();

  r = close_req.close(open_req.result);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  close_req.cleanup();

  r = rename_req.rename("test_file", "test_file2");
  ASSERT(r == 0);
  ASSERT(rename_req.result == 0);
  rename_req.cleanup();

  r = open_req.open("test_file2", O_RDONLY, 0);
  ASSERT(r >= 0);
  ASSERT(open_req.result >= 0);
  open_req.cleanup();

  memset(buf, 0, sizeof(buf));
  iov = uv_buf_init(buf, sizeof(buf));
  r = read_req.read(open_req.result, &iov, 1, -1);
  ASSERT(r >= 0);
  ASSERT(read_req.result >= 0);
  read_req.cleanup();

  r = close_req.close(open_req.result);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  close_req.cleanup();

  /* Cleanup */
  unlink("test_file");
  unlink("test_file2");

  make_valgrind_happy();
}


static void fs_read_bufs(int add_flags) {
  ns_fs close_req;
  ns_fs open_req;
  ns_fs read_req;
  char scratch[768];
  uv_buf_t bufs[4];

  ASSERT(0 <= open_req.open("test/fixtures/lorem_ipsum.txt",
                            O_RDONLY | add_flags, 0));
  ASSERT(open_req.result >= 0);
  open_req.cleanup();

  ASSERT(UV_EINVAL == read_req.read(open_req.result, nullptr, 0, 0));
  ASSERT(UV_EINVAL == read_req.read(open_req.result, nullptr, 1, 0));
  ASSERT(UV_EINVAL == read_req.read(open_req.result, bufs, 0, 0));

  bufs[0] = uv_buf_init(scratch + 0, 256);
  bufs[1] = uv_buf_init(scratch + 256, 256);
  bufs[2] = uv_buf_init(scratch + 512, 128);
  bufs[3] = uv_buf_init(scratch + 640, 128);

  ASSERT(446 == read_req.read(open_req.result,
                              bufs + 0,
                              2,    /* 2x 256 bytes. */
                              0));  /* Positional read. */
  ASSERT(read_req.result == 446);
  read_req.cleanup();

  ASSERT(190 == read_req.read(open_req.result,
                              bufs + 2,
                              2,      /* 2x 128 bytes. */
                              256));  /* Positional read. */
  ASSERT(read_req.result == /* 446 - 256 */ 190);
  read_req.cleanup();

  ASSERT(0 == memcmp(bufs[1].base + 0, bufs[2].base, 128));
  ASSERT(0 == memcmp(bufs[1].base + 128, bufs[3].base, 190 - 128));

  ASSERT(0 == close_req.close(open_req.result));
  ASSERT(close_req.result == 0);
  close_req.cleanup();
}

TEST_CASE("fs_read_bufs", "[fs]") {
  fs_read_bufs(0);
  fs_read_bufs(UV_FS_O_FILEMAP);

  make_valgrind_happy();
}


static void fs_read_file_eof(int add_flags) {
#if defined(__CYGWIN__) || defined(__MSYS__)
  RETURN_SKIP("Cygwin pread at EOF may (incorrectly) return data!");
#endif
  ns_fs close_req;
  ns_fs open_req;
  ns_fs read_req;
  ns_fs write_req;
  uv_buf_t iov;
  int r;

  /* Setup. */
  unlink("test_file");

  r = open_req.open("test_file",
                    O_WRONLY | O_CREAT | add_flags,
                    S_IWUSR | S_IRUSR);
  ASSERT(r >= 0);
  ASSERT(open_req.result >= 0);
  open_req.cleanup();

  iov = uv_buf_init(test_buf, sizeof(test_buf));
  r = write_req.write(open_req.result, &iov, 1, -1);
  ASSERT(r >= 0);
  ASSERT(write_req.result >= 0);
  write_req.cleanup();

  r = close_req.close(open_req.result);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  close_req.cleanup();

  r = open_req.open("test_file", O_RDONLY | add_flags, 0);
  ASSERT(r >= 0);
  ASSERT(open_req.result >= 0);
  open_req.cleanup();

  memset(buf, 0, sizeof(buf));
  iov = uv_buf_init(buf, sizeof(buf));
  r = read_req.read(open_req.result, &iov, 1, -1);
  ASSERT(r >= 0);
  ASSERT(read_req.result >= 0);
  read_req.cleanup();

  iov = uv_buf_init(buf, sizeof(buf));
  r = read_req.read(open_req.result, &iov, 1, read_req.result);
  ASSERT(r == 0);
  ASSERT(read_req.result == 0);
  read_req.cleanup();

  r = close_req.close(open_req.result);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  close_req.cleanup();

  /* Cleanup */
  unlink("test_file");
}

TEST_CASE("fs_read_file_eof", "[fs]") {
  fs_read_file_eof(0);
  fs_read_file_eof(UV_FS_O_FILEMAP);

  make_valgrind_happy();
}


static void fs_write_multiple_bufs(int add_flags) {
  ns_fs close_req;
  ns_fs open_req;
  ns_fs read_req;
  ns_fs write_req;
  uv_buf_t iovs[2];
  uv_buf_t iov;
  int r;

  /* Setup. */
  unlink("test_file");

  r = open_req.open("test_file",
                    O_WRONLY | O_CREAT | add_flags,
                    S_IWUSR | S_IRUSR);
  ASSERT(r >= 0);
  ASSERT(open_req.result >= 0);
  open_req.cleanup();

  iovs[0] = uv_buf_init(test_buf, sizeof(test_buf));
  iovs[1] = uv_buf_init(test_buf2, sizeof(test_buf2));
  r = write_req.write(open_req.result, iovs, 2, 0);
  ASSERT(r >= 0);
  ASSERT(write_req.result >= 0);
  write_req.cleanup();

  r = close_req.close(open_req.result);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  close_req.cleanup();

  r = open_req.open("test_file", O_RDONLY | add_flags, 0);
  ASSERT(r >= 0);
  ASSERT(open_req.result >= 0);
  open_req.cleanup();

  memset(buf, 0, sizeof(buf));
  memset(buf2, 0, sizeof(buf2));
  /* Read the strings back to separate buffers. */
  iovs[0] = uv_buf_init(buf, sizeof(test_buf));
  iovs[1] = uv_buf_init(buf2, sizeof(test_buf2));
  ASSERT(lseek(open_req.result, 0, SEEK_CUR) == 0);
  r = read_req.read(open_req.result, iovs, 2, -1);
  ASSERT(r >= 0);
  ASSERT(read_req.result == sizeof(test_buf) + sizeof(test_buf2));
  read_req.cleanup();

  iov = uv_buf_init(buf, sizeof(buf));
  r = read_req.read(open_req.result, &iov, 1, -1);
  ASSERT(r == 0);
  ASSERT(read_req.result == 0);
  read_req.cleanup();

  /* Read the strings back to separate buffers. */
  iovs[0] = uv_buf_init(buf, sizeof(test_buf));
  iovs[1] = uv_buf_init(buf2, sizeof(test_buf2));
  r = read_req.read(open_req.result, iovs, 2, 0);
  ASSERT(r >= 0);
  if (read_req.result == sizeof(test_buf)) {
    /* Infer that preadv is not available. */
    read_req.cleanup();
    r = read_req.read(open_req.result, &iovs[1], 1, read_req.result);
    ASSERT(r >= 0);
    ASSERT(read_req.result == sizeof(test_buf2));
  } else {
    ASSERT(read_req.result == sizeof(test_buf) + sizeof(test_buf2));
  }
  read_req.cleanup();

  iov = uv_buf_init(buf, sizeof(buf));
  r = read_req.read(
      open_req.result, &iov, 1, sizeof(test_buf) + sizeof(test_buf2));
  ASSERT(r == 0);
  ASSERT(read_req.result == 0);
  read_req.cleanup();

  r = close_req.close(open_req.result);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  close_req.cleanup();

  /* Cleanup */
  unlink("test_file");
}

TEST_CASE("fs_write_multiple_bufs", "[fs]") {
  fs_write_multiple_bufs(0);
  fs_write_multiple_bufs(UV_FS_O_FILEMAP);

  make_valgrind_happy();
}


static void fs_write_alotof_bufs(int add_flags) {
  ns_fs close_req;
  ns_fs open_req;
  ns_fs read_req;
  ns_fs write_req;
  uv_buf_t* iovs;
  uv_buf_t iov;
  size_t iovcount;
  size_t iovmax;
  char* buffer;
  size_t index;
  int r;

  iovcount = 54321;

  /* Setup. */
  unlink("test_file");

  iovs = new (std::nothrow) uv_buf_t[iovcount]();
  ASSERT_NOT_NULL(iovs);
  iovmax = uv_test_getiovmax();

  r = open_req.open(
      "test_file", O_RDWR | O_CREAT | add_flags, S_IWUSR | S_IRUSR);
  ASSERT(r >= 0);
  ASSERT(open_req.result >= 0);
  open_req.cleanup();

  for (index = 0; index < iovcount; ++index)
    iovs[index] = uv_buf_init(test_buf, sizeof(test_buf));

  r = write_req.write(open_req.result, iovs, iovcount, -1);
  ASSERT(r >= 0);
  ASSERT(static_cast<size_t>(write_req.result) == sizeof(test_buf) * iovcount);
  write_req.cleanup();

  /* Read the strings back to separate buffers. */
  buffer = new (std::nothrow) char[sizeof(test_buf) * iovcount]();
  ASSERT_NOT_NULL(buffer);

  for (index = 0; index < iovcount; ++index)
    iovs[index] = uv_buf_init(buffer + index * sizeof(test_buf),
                              sizeof(test_buf));

  r = close_req.close(open_req.result);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  close_req.cleanup();

  r = open_req.open("test_file", O_RDONLY | add_flags, 0);
  ASSERT(r >= 0);
  ASSERT(open_req.result >= 0);
  open_req.cleanup();

  r = read_req.read(open_req.result, iovs, iovcount, -1);
  if (iovcount > iovmax)
    iovcount = iovmax;
  ASSERT(r >= 0);
  ASSERT(static_cast<size_t>(read_req.result) == sizeof(test_buf) * iovcount);

  for (index = 0; index < iovcount; ++index)
    ASSERT(strncmp(buffer + index * sizeof(test_buf),
                   test_buf,
                   sizeof(test_buf)) == 0);

  read_req.cleanup();
  delete[] buffer;

  ASSERT(lseek(open_req.result, write_req.result, SEEK_SET) ==
         write_req.result);
  iov = uv_buf_init(buf, sizeof(buf));
  r = read_req.read(open_req.result, &iov, 1, -1);
  ASSERT(r == 0);
  ASSERT(read_req.result == 0);
  read_req.cleanup();

  r = close_req.close(open_req.result);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  close_req.cleanup();

  /* Cleanup */
  unlink("test_file");
  delete[] iovs;
}

TEST_CASE("fs_write_alotof_bufs", "[fs]") {
  fs_write_alotof_bufs(0);
  fs_write_alotof_bufs(UV_FS_O_FILEMAP);

  make_valgrind_happy();
}


static void fs_write_alotof_bufs_with_offset(int add_flags) {
  ns_fs close_req;
  ns_fs open_req;
  ns_fs read_req;
  ns_fs stat_req;
  ns_fs write_req;
  uv_buf_t* iovs;
  uv_buf_t iov;
  char filler[] = "0123456789";
  int filler_len = strlen(filler);
  size_t iovcount;
  size_t iovmax;
  char* buffer;
  size_t index;
  int r;
  int64_t offset;

  iovcount = 54321;

  /* Setup. */
  unlink("test_file");

  iovs = new (std::nothrow) uv_buf_t[iovcount]();
  ASSERT_NOT_NULL(iovs);
  iovmax = uv_test_getiovmax();

  r = open_req.open("test_file",
                    O_RDWR | O_CREAT | add_flags,
                    S_IWUSR | S_IRUSR);
  ASSERT(r >= 0);
  ASSERT(open_req.result >= 0);
  open_req.cleanup();

  iov = uv_buf_init(filler, filler_len);
  r = write_req.write(open_req.result, &iov, 1, -1);
  ASSERT(r == filler_len);
  ASSERT(write_req.result == filler_len);
  write_req.cleanup();
  offset = static_cast<int64_t>(r);

  for (index = 0; index < iovcount; ++index)
    iovs[index] = uv_buf_init(test_buf, sizeof(test_buf));

  r = write_req.write(open_req.result, iovs, iovcount, offset);
  ASSERT(r >= 0);
  ASSERT(static_cast<size_t>(write_req.result) == sizeof(test_buf) * iovcount);
  write_req.cleanup();

  /* Read the strings back to separate buffers. */
  buffer = new (std::nothrow) char[sizeof(test_buf) * iovcount]();
  ASSERT_NOT_NULL(buffer);

  for (index = 0; index < iovcount; ++index)
    iovs[index] = uv_buf_init(buffer + index * sizeof(test_buf),
                              sizeof(test_buf));

  r = read_req.read(open_req.result, iovs, iovcount, offset);
  ASSERT(r >= 0);
  if (r == sizeof(test_buf))
    iovcount = 1; /* Infer that preadv is not available. */
  else if (iovcount > iovmax)
    iovcount = iovmax;
  ASSERT(static_cast<size_t>(read_req.result) == sizeof(test_buf) * iovcount);

  for (index = 0; index < iovcount; ++index)
    ASSERT(strncmp(buffer + index * sizeof(test_buf),
                   test_buf,
                   sizeof(test_buf)) == 0);

  read_req.cleanup();
  delete[] buffer;

  r = stat_req.stat("test_file");
  ASSERT(r == 0);
  ASSERT(static_cast<int64_t>(stat_req.get_statbuf()->st_size) ==
         offset + static_cast<int64_t>(write_req.result));
  stat_req.cleanup();

  iov = uv_buf_init(buf, sizeof(buf));
  r = read_req.read(open_req.result, &iov, 1, offset + write_req.result);
  ASSERT(r == 0);
  ASSERT(read_req.result == 0);
  read_req.cleanup();

  r = close_req.close(open_req.result);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  close_req.cleanup();

  /* Cleanup */
  unlink("test_file");
  delete[] iovs;
}

TEST_CASE("fs_write_alotof_bufs_with_offset", "[fs]") {
  fs_write_alotof_bufs_with_offset(0);
  fs_write_alotof_bufs_with_offset(UV_FS_O_FILEMAP);

  make_valgrind_happy();
}

TEST_CASE("fs_read_dir", "[fs]") {
  ns_fs close_req;
  ns_fs mkdir_req;
  ns_fs open_req;
  ns_fs read_req;
  uv_loop_t* loop;
  uv_buf_t iov;
  int r;
  char buf[2];

  loop = uv_default_loop();
  mkdir_cb_count = 0;

  /* Setup */
  rmdir("test_dir");
  r = mkdir_req.mkdir(loop, "test_dir", 0755, mkdir_cb, &mkdir_req);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(mkdir_cb_count == 1);
  /* Setup Done Here */

  /* Get a file descriptor for the directory */
  r = open_req.open("test_dir",
                    UV_FS_O_RDONLY | UV_FS_O_DIRECTORY,
                    S_IWUSR | S_IRUSR);
  ASSERT(r >= 0);
  open_req.cleanup();

  /* Try to read data from the directory */
  iov = uv_buf_init(buf, sizeof(buf));
  r = read_req.read(open_req.result, &iov, 1, 0);
#if defined(__FreeBSD__)   || \
    defined(__OpenBSD__)   || \
    defined(__NetBSD__)    || \
    defined(__DragonFly__) || \
    defined(_AIX)          || \
    defined(__sun)         || \
    defined(__MVS__)
  /*
   * As of now, these operating systems support reading from a directory,
   * that too depends on the filesystem this temporary test directory is
   * created on. That is why this assertion is a bit lenient.
   */
  ASSERT((r >= 0) || (r == UV_EISDIR));
#else
  ASSERT(r == UV_EISDIR);
#endif
  read_req.cleanup();

  r = close_req.close(open_req.result);
  ASSERT(r == 0);
  close_req.cleanup();

  /* Cleanup */
  rmdir("test_dir");

  make_valgrind_happy();
}


static void fail_cb(ns_fs*) {
  FAIL("fail_cb should not have been called");
}

TEST_CASE("fs_read_write_null_arguments", "[fs]") {
  ns_fs close_req;
  ns_fs read_req;
  ns_fs write_req;
  uv_loop_t* loop;
  uv_buf_t iov;
  int r;

  r = read_req.read(0, nullptr, 0, -1);
  ASSERT(r == UV_EINVAL);
  read_req.cleanup();

  r = write_req.write(0, nullptr, 0, -1);
  /* Validate some memory management on failed input validation before sending
     fs work to the thread pool. */
  ASSERT(r == UV_EINVAL);
  ASSERT_NULL(write_req.path);
  ASSERT_NULL(write_req.ptr);
#ifdef _WIN32
  ASSERT_NULL(write_req.file.pathw);
  ASSERT_NULL(write_req.fs.info.new_pathw);
  ASSERT_NULL(write_req.fs.info.bufs);
#else
  ASSERT_NULL(write_req.new_path);
  ASSERT_NULL(write_req.bufs);
#endif
  write_req.cleanup();

  iov = uv_buf_init(nullptr, 0);
  r = read_req.read(0, &iov, 0, -1);
  ASSERT(r == UV_EINVAL);
  read_req.cleanup();

  iov = uv_buf_init(nullptr, 0);
  r = write_req.write(0, &iov, 0, -1);
  ASSERT(r == UV_EINVAL);
  write_req.cleanup();

  /* If the arguments are invalid, the loop should not be kept open */
  loop = uv_default_loop();

  r = read_req.read(loop, 0, nullptr, 0, -1, fail_cb);
  ASSERT(r == UV_EINVAL);
  uv_run(loop, UV_RUN_DEFAULT);
  read_req.cleanup();

  r = write_req.write(loop, 0, nullptr, 0, -1, fail_cb);
  ASSERT(r == UV_EINVAL);
  uv_run(loop, UV_RUN_DEFAULT);
  write_req.cleanup();

  iov = uv_buf_init(nullptr, 0);
  r = read_req.read(loop, 0, &iov, 0, -1, fail_cb);
  ASSERT(r == UV_EINVAL);
  uv_run(loop, UV_RUN_DEFAULT);
  read_req.cleanup();

  iov = uv_buf_init(nullptr, 0);
  r = write_req.write(loop, 0, &iov, 0, -1, fail_cb);
  ASSERT(r == UV_EINVAL);
  uv_run(loop, UV_RUN_DEFAULT);
  write_req.cleanup();
}


TEST_CASE("get_osfhandle_valid_handle", "[fs]") {
  ns_fs close_req;
  ns_fs open_req;
  uv_os_fd_t fd;
  int r;

  /* Setup. */
  unlink("test_file");

  r = open_req.open("test_file", O_RDWR | O_CREAT, S_IWUSR | S_IRUSR);
  ASSERT(r >= 0);
  ASSERT(open_req.result >= 0);
  open_req.cleanup();

  fd = uv_get_osfhandle(open_req.result);
#ifdef _WIN32
  ASSERT(fd != INVALID_HANDLE_VALUE);
#else
  ASSERT(fd >= 0);
#endif

  r = close_req.close(open_req.result);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  close_req.cleanup();

  /* Cleanup. */
  unlink("test_file");

  make_valgrind_happy();
}

TEST_CASE("open_osfhandle_valid_handle", "[fs]") {
  ns_fs close_req;
  ns_fs open_req;
  uv_os_fd_t handle;
  int r;
  int fd;

  /* Setup. */
  unlink("test_file");

  r = open_req.open("test_file", O_RDWR | O_CREAT, S_IWUSR | S_IRUSR);
  ASSERT(r >= 0);
  ASSERT(open_req.result >= 0);
  open_req.cleanup();

  handle = uv_get_osfhandle(open_req.result);
#ifdef _WIN32
  ASSERT(handle != INVALID_HANDLE_VALUE);
#else
  ASSERT(handle >= 0);
#endif

  fd = uv_open_osfhandle(handle);
#ifdef _WIN32
  ASSERT(fd > 0);
#else
  ASSERT(fd == open_req.result);
#endif

  r = close_req.close(open_req.result);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  close_req.cleanup();

  /* Cleanup. */
  unlink("test_file");

  make_valgrind_happy();
}

TEST_CASE("fs_file_pos_after_op_with_offset", "[fs]") {
  ns_fs close_req;
  ns_fs open_req;
  ns_fs read_req;
  ns_fs write_req;
  uv_buf_t iov;
  int r;

  /* Setup. */
  unlink("test_file");

  r = open_req.open("test_file", O_RDWR | O_CREAT, S_IWUSR | S_IRUSR);
  ASSERT(r > 0);
  open_req.cleanup();

  iov = uv_buf_init(test_buf, sizeof(test_buf));
  r = write_req.write(open_req.result, &iov, 1, 0);
  ASSERT(r == sizeof(test_buf));
  ASSERT(lseek(open_req.result, 0, SEEK_CUR) == 0);
  write_req.cleanup();

  iov = uv_buf_init(buf, sizeof(buf));
  r = read_req.read(open_req.result, &iov, 1, 0);
  ASSERT(r == sizeof(test_buf));
  ASSERT(lseek(open_req.result, 0, SEEK_CUR) == 0);
  read_req.cleanup();

  r = close_req.close(open_req.result);
  ASSERT(r == 0);
  close_req.cleanup();

  /* Cleanup */
  unlink("test_file");

  make_valgrind_happy();
}


static void statfs_cb(ns_fs* req) {
  uv_statfs_t* stats;

  ASSERT(req->fs_type == UV_FS_STATFS);
  ASSERT(req->result == 0);
  ASSERT_NOT_NULL(req->ptr);
  stats = static_cast<uv_statfs_t*>(req->ptr);

#if defined(_WIN32) || defined(__sun) || defined(_AIX) || defined(__MVS__) || \
  defined(__OpenBSD__) || defined(__NetBSD__)
  ASSERT(stats->f_type == 0);
#else
  ASSERT(stats->f_type > 0);
#endif

  ASSERT(stats->f_bsize > 0);
  ASSERT(stats->f_blocks > 0);
  ASSERT(stats->f_bfree <= stats->f_blocks);
  ASSERT(stats->f_bavail <= stats->f_bfree);

#ifdef _WIN32
  ASSERT(stats->f_files == 0);
  ASSERT(stats->f_ffree == 0);
#else
  /* There is no assertion for stats->f_files that makes sense, so ignore it. */
  ASSERT(stats->f_ffree <= stats->f_files);
#endif
  req->cleanup();
  ASSERT_NULL(req->ptr);
  statfs_cb_count++;
}

static void statfs_cb(ns_fs* req, ns_fs* data) {
  ASSERT_EQ(req, data);
  statfs_cb(req);
}

TEST_CASE("fs_statfs", "[fs]") {
  ns_fs req;
  int r;
  uv_loop_t* loop;

  loop = uv_default_loop();
  statfs_cb_count = 0;

  /* Test the synchronous version. */
  r = req.statfs(".");
  ASSERT(r == 0);
  statfs_cb(&req);
  ASSERT(statfs_cb_count == 1);

  /* Test the asynchronous version. */
  r = req.statfs(loop, ".", statfs_cb);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(statfs_cb_count == 2);

  /* Test passing data. */
  r = req.statfs(loop, ".", statfs_cb, &req);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(statfs_cb_count == 3);

  make_valgrind_happy();
}

TEST_CASE("fs_get_system_error", "[fs]") {
  ns_fs req;
  int r;
  int system_error;

  r = req.statfs("non_existing_file");
  ASSERT(r != 0);

  system_error = req.get_system_error();
#ifdef _WIN32
  ASSERT(system_error == ERROR_FILE_NOT_FOUND);
#else
  ASSERT(system_error == ENOENT);
#endif
}
