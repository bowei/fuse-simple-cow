/*
 * See LICENSE.
 *
 * The simple cow makes no pretence about performance.
 */
#define FUSE_USE_VERSION 26
#define SIMPLE_COW_VERSION "0.1"

#ifdef linux
/* For pread()/pwrite()/utimensat() */
#define _XOPEN_SOURCE 700
#endif

#include <fuse.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#include <pthread.h>

struct cow_config {
  char* src_dir;
  char* cow_dir;
};

static struct cow_config the_config = {0};
pthread_mutex_t the_lock = PTHREAD_MUTEX_INITIALIZER;

static char* pathcat(const char* head, const char* tail) {
  char* ret = malloc(strlen(head) + strlen(tail) + 2);
  if (ret == NULL) { abort(); }

  ret[0] = '\0';
  strcat(ret, head);

  if (tail[0] == '\0') { return ret; }

  if (head[strlen(head)-1] != '/' && tail[0] != '/') {
    strcat(ret, "/");
  }
  strcat(ret, tail);

  return ret;
}

static void copy_file(const char* src, const char* dst) {
  pthread_mutex_lock(&the_lock);
  // XXX/bowei -- get rid of system()
  char buf[1024];
  sprintf(buf, "cp -f %s %s.simple_cow_copy", src, dst);
  fprintf(stderr, "%s\n", buf);
  system(buf);

  sprintf(buf, "mv %s.simple_cow_copy %s", dst, dst);
  system(buf);

  pthread_mutex_unlock(&the_lock);
}

static void maybe_copy(const char* path) {
  char* src_path = pathcat(the_config.src_dir, path);
  char* cow_path = pathcat(the_config.cow_dir, path);

  struct stat src_st, cow_st;
  int ret;

  ret = stat(src_path, &src_st);
  if (ret < 0) { goto end; }

  ret = stat(cow_path, &cow_st);
  if (ret < 0) { goto end; }

  if (src_st.st_ino == cow_st.st_ino) {
    copy_file(src_path, cow_path);
  }

end:
  free(src_path);
  free(cow_path);
}

static int cow_getattr(const char *orig_path, struct stat *stbuf)
{
  int res;

  char* path = pathcat(the_config.cow_dir, orig_path);
  res = lstat(path, stbuf);
  free(path);

  if (res == -1)
    return -errno;

  return 0;
}

static int cow_access(const char *orig_path, int mask)
{
  int res;

  char* path = pathcat(the_config.cow_dir, orig_path);
  res = access(path, mask);
  free(path);

  if (res == -1)
    return -errno;

  return 0;
}

static int cow_readlink(const char* orig_path, char *buf, size_t size)
{
  int res;

  // XXX/bowei -- needs adjusting
  char* path = pathcat(the_config.cow_dir, orig_path);
  res = readlink(path, buf, size - 1);
  free(path);

  if (res == -1)
    return -errno;

  buf[res] = '\0';
  return 0;
}


static int cow_readdir(const char* orig_path, void *buf,
                       fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi)
{
  DIR *dp;
  struct dirent *de;

  (void) offset;
  (void) fi;

  char* path = pathcat(the_config.cow_dir, orig_path);
  dp = opendir(path);
  free(path);

  if (dp == NULL)
    return -errno;

  while ((de = readdir(dp)) != NULL) {
    struct stat st;
    memset(&st, 0, sizeof(st));
    st.st_ino = de->d_ino;
    st.st_mode = de->d_type << 12;
    if (filler(buf, de->d_name, &st, 0))
      break;
  }

  closedir(dp);
  return 0;
}

static int cow_mknod(const char* orig_path, mode_t mode, dev_t rdev)
{
  int res;

  char* path = pathcat(the_config.cow_dir, orig_path);
  /* On Linux this could just be 'mknod(path, mode, rdev)' but this
     is more portable */
  if (S_ISREG(mode)) {
    res = open(path, O_CREAT | O_EXCL | O_WRONLY, mode);
    if (res >= 0)
      res = close(res);
  } else if (S_ISFIFO(mode))
    res = mkfifo(path, mode);
  else
    res = mknod(path, mode, rdev);

  free(path);

  if (res == -1)
    return -errno;

  return 0;
}

static int cow_mkdir(const char* orig_path, mode_t mode)
{
  int res;

  char* path = pathcat(the_config.cow_dir, orig_path);
  res = mkdir(path, mode);
  free(path);

  if (res == -1)
    return -errno;

  return 0;
}

static int cow_unlink(const char* orig_path)
{
  int res;

  char* path = pathcat(the_config.cow_dir, orig_path);
  res = unlink(path);
  free(path);

  if (res == -1)
    return -errno;

  return 0;
}

static int cow_rmdir(const char* orig_path)
{
  int res;

  char* path = pathcat(the_config.cow_dir, orig_path);
  res = rmdir(path);
  free(path);

  if (res == -1)
    return -errno;

  return 0;
}

static int cow_symlink(const char *from, const char *to)
{
  int res;

  // XXX/bowei -- what to do here
  res = symlink(from, to);

  if (res == -1)
    return -errno;

  return 0;
}

static int cow_rename(const char *orig_from, const char *orig_to)
{
  int res;

  char* from = pathcat(the_config.cow_dir, orig_from);
  char* to = pathcat(the_config.cow_dir, orig_to);

  res = rename(from, to);

  free(from);
  free(to);

  if (res == -1)
    return -errno;

  return 0;
}

static int cow_link(const char *orig_from, const char *orig_to)
{
  int res;

  char* from = pathcat(the_config.cow_dir, orig_from);
  char* to = pathcat(the_config.cow_dir, orig_to);

  res = link(from, to);

  free(from);
  free(to);

  if (res == -1)
    return -errno;

  return 0;
}

static int cow_chmod(const char* orig_path, mode_t mode)
{
  int res;

  char* path = pathcat(the_config.cow_dir, orig_path);
  res = chmod(path, mode);
  free(path);

  if (res == -1)
    return -errno;

  return 0;
}

static int cow_chown(const char* orig_path, uid_t uid, gid_t gid)
{
  int res;

  char* path = pathcat(the_config.cow_dir, orig_path);
  res = lchown(path, uid, gid);
  free(path);

  if (res == -1)
    return -errno;

  return 0;
}

static int cow_truncate(const char* orig_path, off_t size)
{
  int res;

  maybe_copy(orig_path);

  char* path = pathcat(the_config.cow_dir, orig_path);
  res = truncate(path, size);
  // XXX
  free(path);

  if (res == -1)
    return -errno;

  return 0;
}

static int cow_open(const char* orig_path, struct fuse_file_info *fi)
{
  int res;

  maybe_copy(orig_path);

  char* path = pathcat(the_config.cow_dir, orig_path);
  res = open(path, fi->flags);
  free(path);

  if (res == -1)
    return -errno;

  close(res);
  return 0;
}

static int cow_read(const char* orig_path, char *buf,
                    size_t size, off_t offset,
                    struct fuse_file_info *fi)
{
  int fd;
  int res;

  (void) fi;

  char* path = pathcat(the_config.cow_dir, orig_path);
  fd = open(path, O_RDONLY);
  free(path);

  if (fd == -1)
    return -errno;

  res = pread(fd, buf, size, offset);
  if (res == -1)
    res = -errno;

  close(fd);
  return res;
}

static int cow_write(const char* orig_path, const char *buf, size_t size,
                     off_t offset, struct fuse_file_info *fi)
{
  int fd;
  int res;

  (void) fi;

  maybe_copy(orig_path);

  char* path = pathcat(the_config.cow_dir, orig_path);
  fd = open(path, O_WRONLY);
  free(path);

  if (fd == -1)
    return -errno;

  res = pwrite(fd, buf, size, offset);
  if (res == -1)
    res = -errno;

  close(fd);
  return res;
}

static int cow_statfs(const char* orig_path, struct statvfs *stbuf)
{
  int res;

  char* path = pathcat(the_config.cow_dir, orig_path);
  res = statvfs(path, stbuf);
  free(path);

  if (res == -1)
    return -errno;

  return 0;
}

static struct fuse_operations cow_oper = {
  .getattr  = cow_getattr,
  .access   = cow_access,
  .readlink = cow_readlink,
  .readdir  = cow_readdir,
  .mknod    = cow_mknod,
  .mkdir    = cow_mkdir,
  .symlink  = cow_symlink,
  .unlink   = cow_unlink,
  .rmdir    = cow_rmdir,
  .rename   = cow_rename,
  .link     = cow_link,
  .chmod    = cow_chmod,
  .chown    = cow_chown,
  .truncate = cow_truncate, // x
  .open    = cow_open,
  .read    = cow_read,
  .write   = cow_write, // x
  .statfs  = cow_statfs
};

enum { KEY_HELP, KEY_VERSION };

static struct fuse_opt cow_opts[] = {
  { "src_dir=%s", offsetof(struct cow_config, src_dir), 0},
  { "cow_dir=%s", offsetof(struct cow_config, cow_dir), 0},
  FUSE_OPT_KEY("-h", KEY_HELP),
  FUSE_OPT_KEY("--help", KEY_HELP),
  FUSE_OPT_KEY("-V", KEY_VERSION),
  FUSE_OPT_KEY("--version", KEY_VERSION),
  FUSE_OPT_END
};

static int cow_opt_proc(void *data, const char *arg, int key,
                        struct fuse_args *outargs)
{
  switch (key) {
    case KEY_HELP: {
      fprintf(stderr,
              "Usage: %s mountpoint [options]\n"
              "\n"
              "General options:\n"
              "  -o opt,[opt...]  mount options\n"
              "  -h   --help      print help\n"
              "  -V   --version   print version\n"
              "\n"
              "Simple cow options:\n"
              "  -o src_dir Source directory\n"
              "  -o cow_dir Copy-on-write directory\n\n",
              outargs->argv[0]);
      fuse_opt_add_arg(outargs, "-ho");
      fuse_main(outargs->argc, outargs->argv, &cow_oper, NULL);
      break;
    }
    case KEY_VERSION: {
      fprintf(stderr, "simple-cow version %s\n", SIMPLE_COW_VERSION);
      fuse_opt_add_arg(outargs, "--version");
      fuse_main(outargs->argc, outargs->argv, &cow_oper, NULL);
      exit(0);
      break;
    }
  }

  return 1;
}

int main(int argc, char *argv[])
{
  umask(0);

  struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
  fuse_opt_parse(&args, &the_config, cow_opts, cow_opt_proc);

  if (the_config.src_dir == NULL || the_config.cow_dir == NULL) {
    fprintf(stderr,
            "You must specify -o src_dir=... and a -o cow_dir=...\n");
    exit(1);
  }

  return fuse_main(args.argc, args.argv, &cow_oper, NULL);
}
