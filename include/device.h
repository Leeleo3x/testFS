#ifndef _DEVICE_H
#define _DEVICE_H


#ifdef FILE_DEVICE
#include <stdio.h>
struct device {
  FILE *raw;
};
#elif BDEV_DEVICE
#include "device_bdev.h"
struct device {
  struct ns_entry *raw;
};
#else
#include "device_nvme.h"
struct device {
  struct ns_entry *raw;
};
#endif

typedef void (* device_init_cb)(void *arg1, void *arg2);

void dev_init(const char *file, device_init_cb cb);


void dflush(struct device *);
void dclose(struct device *);

#endif