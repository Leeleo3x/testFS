#include "device.h"

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

struct device *dev_init(const char *file) {
  int sock;
  struct device dss;
  if ((sock = open(file, O_RDWR
#ifndef DISABLE_OSYNC
                             | O_SYNC
#endif
                   )) < 0) {
    return NULL;
  } 
  FILE *f;
  if ((f = fdopen(sock, "r+")) == NULL) {
    return NULL;
  };
  struct device *dev = malloc(sizeof(struct device));
  dev->raw = f;
  return dev;
}

void dflush(struct device *dev) {
  fflush(dev->raw);
}

void dclose(struct device *dev) {
  fclose(dev->raw);
}