#ifndef _COMMON_H
#define _COMMON_H

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EXIT(error)                                                        \
  do {                                                                     \
    fprintf(stdout, "%s: %s: %s\n", __FUNCTION__, error, strerror(errno)); \
    fflush(stdout);                                                        \
    exit(1);                                                               \
  } while (0)

#define WARN(error)                                                        \
  do {                                                                     \
    fprintf(stdout, "%s: %s: %s\n", __FUNCTION__, error, strerror(errno)); \
    fflush(stdout);                                                        \
  } while (0)

#define MAX(a, b) ((a) >= (b) ? (a) : (b))

#define DIVROUNDUP(a, b) (((a) + (b)-1) / (b))
#define ROUNDUP(a, b) (DIVROUNDUP(a, b) * b)

#endif /* _COMMON_H */
