#include "kernel/types.h"
#include "user/user.h"

#define ROUNDS 5
#define DURATION 40 // ticks (~4 seconds) per round

// Busy-loop from the shared start tick for DURATION ticks, then
// write the iteration count to wfd and exit.
void
busy(int tickets, int wfd, uint start)
{
  unsigned long n = 0;

  if (settickets(tickets) < 0) {
    printf("testlottery: settickets(%d) failed\n", tickets);
    exit(1);
  }

  // Spin until the shared start tick, so both children measure
  // the same wall-clock window and compete from the same tick.
  while (uptime() < start)
    ;
  while (uptime() - start < DURATION)
    n++;
  if (write(wfd, &n, sizeof(n)) != sizeof(n)) {
    printf("testlottery: write failed\n");
    exit(1);
  }
  exit(0);
}

int
main(int argc, char *argv[])
{
  int ta = 10;
  int tb = 1;

  if (argc > 1)
    ta = atoi(argv[1]);
  if (argc > 2)
    tb = atoi(argv[2]);

  int pid[2];
  unsigned long n[2];
  unsigned long total[2] = {0, 0};

  printf("testlottery: %d rounds of %d ticks, tickets %d vs %d\n",
         ROUNDS, DURATION, ta, tb);
  for (int r = 0; r < ROUNDS; r++) {
    int pfd[2][2];
    uint start = uptime() + 5;
    for (int i = 0; i < 2; i++) {
      if (pipe(pfd[i]) < 0) {
        printf("testlottery: pipe failed\n");
        exit(1);
      }
      pid[i] = fork();
      if (pid[i] < 0) {
        printf("testlottery: fork failed\n");
        exit(1);
      }
      if (pid[i] == 0) {
        close(pfd[i][0]);
        busy(i == 0 ? ta : tb, pfd[i][1], start);
      }
      close(pfd[i][1]);
    }
    for (int i = 0; i < 2; i++) {
      if (read(pfd[i][0], &n[i], sizeof(n[i])) != sizeof(n[i])) {
        printf("testlottery: read failed\n");
        exit(1);
      }
      close(pfd[i][0]);
      if (wait(0) < 0) {
        printf("testlottery: wait failed\n");
        exit(1);
      }
    }
    unsigned long x10 = n[0] * 10 / n[1];
    printf("round %d: tickets %d: %lu, tickets %d: %lu, ratio ~%lu.%lu\n",
           r + 1, ta, n[0], tb, n[1], x10 / 10, x10 % 10);
    total[0] += n[0];
    total[1] += n[1];
  }
  unsigned long x10 = total[0] * 10 / total[1];
  printf("testlottery: average ratio ~%lu.%lu (expect ~%d.0)\n",
         x10 / 10, x10 % 10, ta / tb);
  exit(0);
}