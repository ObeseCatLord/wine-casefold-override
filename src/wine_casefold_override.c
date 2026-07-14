#define _GNU_SOURCE

#include <errno.h>
#include <limits.h>
#include <linux/fs.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <unistd.h>

#ifndef FS_CASEFOLD_FL
#define FS_CASEFOLD_FL 0x40000000
#endif

#define MAX_ROOTS 16

static char *root_storage;
static const char *roots[MAX_ROOTS];
static size_t root_count;
static bool trace_enabled;
static atomic_ulong bypass_count;
static atomic_bool first_hit_logged;

static void trace_message(const char *message)
{
  if (trace_enabled) dprintf(STDERR_FILENO, "wine-casefold: %s\n", message);
}

static bool path_is_within(const char *path, const char *root)
{
  const size_t root_len = strlen(root);
  return !strncmp(path, root, root_len) &&
         (path[root_len] == '\0' || path[root_len] == '/');
}

static bool fd_is_in_casefold_root(int fd)
{
  char proc_path[64];
  char target[PATH_MAX + 1];
  int length = snprintf(proc_path, sizeof(proc_path), "/proc/self/fd/%d", fd);
  if (length <= 0 || (size_t)length >= sizeof(proc_path)) return false;

  const ssize_t target_len = readlink(proc_path, target, PATH_MAX);
  if (target_len <= 0) return false;
  target[target_len] = '\0';

  for (size_t i = 0; i < root_count; ++i)
    if (path_is_within(target, roots[i])) return true;
  return false;
}

static void report_stats(void)
{
  const unsigned long count = atomic_load_explicit(&bypass_count, memory_order_relaxed);
  if (trace_enabled && count)
    dprintf(STDERR_FILENO, "wine-casefold: bypassed %lu casefold ioctls\n", count);
}

static void initialize(void) __attribute__((constructor));
static void initialize(void)
{
  const char *trace = getenv("WINE_CASEFOLD_TRACE");
  trace_enabled = trace && trace[0] && strcmp(trace, "0");

  const char *configured = getenv("WINE_CASEFOLD_ROOTS");
  if (!configured || !configured[0]) return;

  root_storage = strdup(configured);
  if (!root_storage) return;

  char *save = NULL;
  for (char *token = strtok_r(root_storage, ":", &save);
       token && root_count < MAX_ROOTS;
       token = strtok_r(NULL, ":", &save))
  {
    if (token[0] != '/')
    {
      if (trace_enabled)
        dprintf(STDERR_FILENO, "wine-casefold: ignoring non-absolute root\n");
      continue;
    }

    size_t length = strlen(token);
    while (length > 1 && token[length - 1] == '/') token[--length] = '\0';

    char resolved[PATH_MAX];
    if (realpath(token, resolved))
    {
      char *canonical = strdup(resolved);
      if (canonical) roots[root_count++] = canonical;
    }
    else
    {
      roots[root_count++] = token;
    }
  }

  if (trace_enabled)
    dprintf(STDERR_FILENO, "wine-casefold: configured %zu scoped root(s)\n", root_count);
  atexit(report_stats);
}

static int write_casefold_result(uintptr_t address)
{
  const int flags = FS_CASEFOLD_FL;
  const struct iovec local = {
    .iov_base = (void *)&flags,
    .iov_len = sizeof(flags),
  };
  const struct iovec remote = {
    .iov_base = (void *)address,
    .iov_len = sizeof(flags),
  };
  const ssize_t written = syscall(SYS_process_vm_writev, getpid(), &local, 1,
                                  &remote, 1, 0);
  if (written == (ssize_t)sizeof(flags)) return 0;
  if (written >= 0) errno = EFAULT;
  return -1;
}

/*
 * A fixed-width third argument preserves the raw ABI bits for pointer, scalar,
 * and legacy ioctl callers. For two-argument ioctls the unused register/stack
 * slot is passed to the kernel unchanged; requests with no argument ignore it.
 * Do not include sys/ioctl.h here because its variadic prototype conflicts with
 * this interposition boundary.
 */
int ioctl(int fd, unsigned long request, uintptr_t arg)
{
  if (request == FS_IOC_GETFLAGS && root_count && fd_is_in_casefold_root(fd))
  {
    if (write_casefold_result(arg)) return -1;
    atomic_fetch_add_explicit(&bypass_count, 1, memory_order_relaxed);
    if (trace_enabled &&
        !atomic_exchange_explicit(&first_hit_logged, true, memory_order_relaxed))
      trace_message("first scoped FS_IOC_GETFLAGS bypass");
    return 0;
  }

  return (int)syscall(SYS_ioctl, fd, request, arg);
}
