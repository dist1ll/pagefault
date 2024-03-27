#include "time.h"
#include <stddef.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/sysinfo.h>
#include <threads.h>

struct args {
  char *addr;
  size_t len;
};

int prefault(void *arg) {
  struct args *a = (struct args *)arg;
  madvise(a->addr, a->len, MADV_POPULATE_WRITE);
  // for (size_t i = 0; i < a->len; i += 4096) {
  //   a->addr[i] = 0xff;
  // }
  return 0;
}

void iter(int threads) {
  size_t map_size = ((size_t)1) << 32;
  char *addr = (char *)mmap(0, map_size, PROT_WRITE | PROT_READ,
                            MAP_SHARED | MAP_ANONYMOUS, -1, 0);

  /* Spawn prefaulting threads */
  char *tmp = addr;
  struct args as[threads];
  thrd_t ids[threads];
  for (int i = 0; i < threads; i++) {
    as[i].addr = tmp;
    as[i].len = map_size / threads;
    thrd_create(&ids[i], prefault, (void *)&as[i]);
    tmp += map_size / threads;
  }

  /* Wait for all prefaulting THREADS */
  for (int i = 0; i < threads; i++) {
    thrd_join(ids[i], NULL);
  }

  munmap((void *)addr, map_size);
}

int main(int argc, char *argv[]) {
  iter(..)
}
