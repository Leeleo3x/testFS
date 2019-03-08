#include "spdk/env.h"
#include "spdk/bdev.h"
#include "spdk/event.h"

#include "device.h"

struct ctrlr_entry {
  struct spdk_nvme_ctrlr *ctrlr;
  struct ctrlr_entry *next;
  char name[1024];
};

static struct ctrlr_entry *g_controllers = NULL;
static struct ns_entry *g_namespaces = NULL;


static void start(void *arg1, void *arg2) {
}

struct device *dev_init(const char *f) {
  int rc;
  struct spdk_app_opts opts = {};
  /*
   * SPDK relies on an abstraction around the local environment
   * named env that handles memory allocation and PCI device operations.
   * This library must be initialized first.
   *
   */
  spdk_app_opts_init(&opts);
  opts.name = "hello_world";
  opts.config_file = "config.conf";
  spdk_app_start(&opts, start, NULL, NULL);
  return NULL;
}

void dflush(struct device *dev) {

}

void dclose(struct device *dev) {

}