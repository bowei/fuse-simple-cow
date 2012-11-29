/*
 * The simple cow makes no pretence about performance. The astute
 * reader will notice this is mostly fusexmp.c.
 */
#define FUSE_USE_VERSION 26
#define SIMPLE_COW_VERSION "0.1"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

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

static int cow_getattr(const char *path, struct stat *stbuf)
{
  int res;

  res = lstat(path, stbuf);
  if (res == -1)
    return -errno;

  return 0;
}

static int cow_access(const char *path, int mask)
{
  int res;

  res = access(path, mask);
  if (res == -1)
    return -errno;

  return 0;
}

static int cow_readlink(const char *path, char *buf, size_t size)
{
  int res;

  res = readlink(path, buf, size - 1);
  if (res == -1)
    return -errno;

  buf[res] = '\0';
  return 0;
}


static int cow_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi)
{
  DIR *dp;
  struct dirent *de;

  (void) offset;
  (void) fi;

  dp = opendir(path);
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

static int cow_mknod(const char *path, mode_t mode, dev_t rdev)
{
  int res;

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
  if (res == -1)
    return -errno;

  return 0;
}

static int cow_mkdir(const char *path, mode_t mode)
{
  int res;

  res = mkdir(path, mode);
  if (res == -1)
    return -errno;

  return 0;
}

static int cow_unlink(const char *path)
{
  int res;

  res = unlink(path);
  if (res == -1)
    return -errno;

  return 0;
}

static int cow_rmdir(const char *path)
{
  int res;

  res = rmdir(path);
  if (res == -1)
    return -errno;

  return 0;
}

static int cow_symlink(const char *from, const char *to)
{
  int res;

  res = symlink(from, to);
  if (res == -1)
    return -errno;

  return 0;
}

static int cow_rename(const char *from, const char *to)
{
  int res;

  res = rename(from, to);
  if (res == -1)
    return -errno;

  return 0;
}

static int cow_link(const char *from, const char *to)
{
  int res;

  res = link(from, to);
  if (res == -1)
    return -errno;

  return 0;
}

static int cow_chmod(const char *path, mode_t mode)
{
  int res;

  res = chmod(path, mode);
  if (res == -1)
    return -errno;

  return 0;
}

static int cow_chown(const char *path, uid_t uid, gid_t gid)
{
  int res;

  res = lchown(path, uid, gid);
  if (res == -1)
    return -errno;

  return 0;
}

static int cow_truncate(const char *path, off_t size)
{
  int res;

  res = truncate(path, size);
  if (res == -1)
    return -errno;

  return 0;
}

static int cow_open(const char *path, struct fuse_file_info *fi)
{
  int res;

  res = open(path, fi->flags);
  if (res == -1)
    return -errno;

  close(res);
  return 0;
}

static int cow_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi)
{
  int fd;
  int res;

  (void) fi;
  fd = open(path, O_RDONLY);
  if (fd == -1)
    return -errno;

  res = pread(fd, buf, size, offset);
  if (res == -1)
    res = -errno;

  close(fd);
  return res;
}

static int cow_write(const char *path, const char *buf, size_t size,
                     off_t offset, struct fuse_file_info *fi)
{
  int fd;
  int res;

  (void) fi;
  fd = open(path, O_WRONLY);
  if (fd == -1)
    return -errno;

  res = pwrite(fd, buf, size, offset);
  if (res == -1)
    res = -errno;

  close(fd);
  return res;
}

static int cow_statfs(const char *path, struct statvfs *stbuf)
{
  int res;

  res = statvfs(path, stbuf);
  if (res == -1)
    return -errno;

  return 0;
}

static int cow_release(const char *path, struct fuse_file_info *fi)
{
  /* Just a stub.	 This method is optional and can safely be left
     unimplemented */

  (void) path;
  (void) fi;
  return 0;
}

static int cow_fsync(const char *path, int isdatasync,
                     struct fuse_file_info *fi)
{
  /* Just a stub.	 This method is optional and can safely be left
     unimplemented */

  (void) path;
  (void) isdatasync;
  (void) fi;
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
  .statfs  = cow_statfs,
  .release = cow_release,
  .fsync   = cow_fsync,
};

enum { KEY_HELP, KEY_VERSION };

struct cow_config {
  char* src_dir;
  char* cow_dir;
};

static struct fuse_opt cow_opts[] = {
  { "src_dir=%s", offsetof(struct cow_config, src_dir), 0},
  { "cow_dir=%s", offsetof(struct cow_config, cow_dir), 0},
  FUSE_OPT_KEY("-h", KEY_HELP),
  FUSE_OPT_KEY("--help", KEY_HELP),
  FUSE_OPT_KEY("-V", KEY_VERSION),
  FUSE_OPT_KEY("--version", KEY_VERSION),
  FUSE_OPT_END
};

static struct cow_config the_config = {0};

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
