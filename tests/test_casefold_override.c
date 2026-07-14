#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#ifndef FS_CASEFOLD_FL
#define FS_CASEFOLD_FL 0x40000000
#endif

static int probe(const char *path)
{
  int flags = 0;
  const int fd = open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  if (fd < 0)
  {
    perror(path);
    return 2;
  }

  errno = 0;
  const int result = ioctl(fd, FS_IOC_GETFLAGS, &flags);
  const int saved_errno = errno;
  close(fd);
  printf("%s result=%d errno=%d flags=0x%x casefold=%d\n", path, result,
         saved_errno, flags, !!(flags & FS_CASEFOLD_FL));
  return 0;
}

static int test_non_target_forwarding(void)
{
  int pipe_fds[2];
  if (pipe(pipe_fds))
  {
    perror("pipe");
    return 1;
  }

  const char payload[] = "abc";
  if (write(pipe_fds[1], payload, sizeof(payload)) != (ssize_t)sizeof(payload))
  {
    perror("write");
    return 1;
  }

  int available = 0;
  if (ioctl(pipe_fds[0], FIONREAD, &available) || available != sizeof(payload))
  {
    perror("pointer ioctl forwarding");
    return 1;
  }
  close(pipe_fds[0]);
  close(pipe_fds[1]);

  errno = 0;
  if (ioctl(-1, FIOCLEX) != -1 || errno != EBADF)
  {
    fprintf(stderr, "two-argument ioctl forwarding failed: errno=%d\n", errno);
    return 1;
  }

  errno = 0;
  if (ioctl(-1, TIOCSCTTY, 1UL) != -1 || errno != EBADF)
  {
    fprintf(stderr, "scalar ioctl forwarding failed: errno=%d\n", errno);
    return 1;
  }
  return 0;
}

static int test_fault_safe_output(const char *path)
{
  const int fd = open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  if (fd < 0)
  {
    perror(path);
    return 1;
  }

  errno = 0;
  if (ioctl(fd, FS_IOC_GETFLAGS, (void *)1) != -1 || errno != EFAULT)
  {
    fprintf(stderr, "invalid output pointer did not return EFAULT: errno=%d\n",
            errno);
    return 1;
  }

  const long page_size = sysconf(_SC_PAGESIZE);
  void *readonly = mmap(NULL, (size_t)page_size, PROT_READ,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (readonly == MAP_FAILED)
  {
    perror("mmap");
    return 1;
  }

  errno = 0;
  const int result = ioctl(fd, FS_IOC_GETFLAGS, readonly);
  const int saved_errno = errno;
  munmap(readonly, (size_t)page_size);
  close(fd);
  if (result != -1 || saved_errno != EFAULT)
  {
    fprintf(stderr, "read-only output did not return EFAULT: errno=%d\n",
            saved_errno);
    return 1;
  }
  return 0;
}

int main(int argc, char **argv)
{
  if (argc < 2)
  {
    fprintf(stderr, "usage: %s DIRECTORY...\n", argv[0]);
    return 2;
  }
  if (test_non_target_forwarding()) return 1;
  if (getenv("TEST_FAULT_SAFE_OUTPUT") &&
      test_fault_safe_output(argv[1])) return 1;
  for (int i = 1; i < argc; ++i)
    if (probe(argv[i])) return 1;
  return 0;
}
