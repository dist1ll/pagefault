#define _GNU_SOURCE

#include "time.h"
#include <sched.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/sysinfo.h>
#include <threads.h>

#define MADV_POPULATE_WRITE 23
#define MAP_HUGE_2MB (21 << MAP_HUGE_SHIFT)

/* We always allocate 4GB, in both anon & file-backed mappings */
const size_t MAP_SIZE = ((size_t)1) << 32;

struct args {
  char *addr;
  size_t len;
  int cpu;
};

int thread_prefault(void *arg) {
  struct args *a = (struct args *)arg;
  /* set affinity */
  cpu_set_t my_set;
  CPU_ZERO(&my_set);
  CPU_SET(a->cpu, &my_set);
  sched_setaffinity(0, sizeof(cpu_set_t), &my_set);

  int err = madvise(a->addr, a->len, MADV_POPULATE_WRITE);
  if (err != 0) {
    printf("madvise: error %d\n", err);
  }
  return 0;
}

/**
 * Pre-fault a 4GB block of memory, starting at the given address,
 * and in parallel with the given number of core-pinned threads.
 */
void run(char *addr, int threads) {
  /* Spawn prefaulting threads */
  char *tmp = addr;
  struct args as[threads];
  thrd_t ids[threads];
  size_t sum = 0;
  for (int i = 0; i < threads; i++) {
    as[i].addr = tmp;
    as[i].len = MAP_SIZE / threads;
    as[i].cpu = i;
    thrd_create(&ids[i], thread_prefault, (void *)&as[i]);
    tmp += MAP_SIZE / threads;
    sum += as[i].len;
  }
  /* Wait for all prefaulting THREADS */
  for (int i = 0; i < threads; i++) {
    thrd_join(ids[i], NULL);
  }
}

void err_usage_exit(const char *err) {
  fprintf(stderr, "\e[1;31merror\e[0m: %s\n\n", err);
  printf("\
USAGE: ./prog <MODE> <THREAD_COUNT> [PATH]\n\n\
MODE:\n\
    anon   Mmaps an anonymous, shared 4GB region of memory, and immediately\n\
           prefaults it all using madvise (one call per core).\n\n\
    file   Mmaps a file-backed 4GB region of memory and prefaults it.\n\
           Requires a PATH argument, at which the file is created. If the\n\
           file exists, we check that it's 4GB large, and fault it in.");
  exit(1);
}

int main(int argc, char *argv[]) {
  if (argc <= 1) {
    err_usage_exit("no argument given.");
  } else if (argc == 2) {
    err_usage_exit("missing thread count.");
  }

  int threads = atoi(argv[2]);
  if ((threads & -threads) != threads) {
    fprintf(stderr, "\e[1;31merror\e[0m: thread count must be power-of-two.");
    exit(1);
  }

  char *addr;
  switch (argc) {
  case 3:
    addr = (char *)mmap(0, MAP_SIZE, PROT_WRITE | PROT_READ,
                        MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    break;
  case 4:
    addr = (char *)mmap(0, MAP_SIZE, PROT_WRITE | PROT_READ,
                        MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    break;
  default:
    err_usage_exit("too many arguments given");
  }

  if (addr == MAP_FAILED) {
    /* NOTE: If you want to use huge pages, make sure vm.nr_hugepages
     * is large enough to fully allocate 4GB. E.g. if you have 2MB huge
     * pages, vm.nr_hugepages needs to be at least 2048. */
    fprintf(stderr, "fatal: mmap failed");
    exit(1);
  }
  run(addr, threads);
  munmap((void *)addr, MAP_SIZE);
}
