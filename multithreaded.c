#define _GNU_SOURCE

#include "time.h"
#include <sched.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/sysinfo.h>
#include <threads.h>

#define MAP_HUGE_2MB (21 << MAP_HUGE_SHIFT)

struct args {
  char *addr;
  size_t len;
  int cpu;
};

int prefault(void *arg) {
  struct args *a = (struct args *)arg;
  /* set affinity */
  cpu_set_t my_set;         /* Define your cpu_set bit mask. */
  CPU_ZERO(&my_set);        /* Initialize it all to 0, i.e. no CPUs selected. */
  CPU_SET(a->cpu, &my_set); /* set the bit that represents core 7. */
  sched_setaffinity(0, sizeof(cpu_set_t), &my_set);

  madvise(a->addr, a->len, MADV_POPULATE_WRITE);
  // for (size_t i = 0; i < a->len; i += 4096) {
  // a->addr[i] = 0xff;
  // }
  return 0;
}

void iter(int threads) {
  size_t map_size = ((size_t)1) << 32;
  char *addr = (char *)mmap(
      0, map_size, PROT_WRITE | PROT_READ,
      MAP_SHARED | MAP_ANONYMOUS | MAP_HUGETLB | MAP_HUGE_2MB, -1, 0);

  if (addr == MAP_FAILED) {
    printf("mmap failed");
    exit(1);
  }

  /* Spawn prefaulting threads */
  char *tmp = addr;
  struct args as[threads];
  thrd_t ids[threads];
  size_t sum = 0;
  for (int i = 0; i < threads; i++) {

    as[i].addr = tmp;
    as[i].len = map_size / threads;
    as[i].cpu = i;
    thrd_create(&ids[i], prefault, (void *)&as[i]);
    tmp += map_size / threads;
    sum += as[i].len;
  }
  printf("total size: %zu\n", sum);

  /* Wait for all prefaulting THREADS */
  for (int i = 0; i < threads; i++) {
    thrd_join(ids[i], NULL);
  }

  munmap((void *)addr, map_size);
}

int main(int argc, char *argv[]) { iter(atoi(argv[1])); }
