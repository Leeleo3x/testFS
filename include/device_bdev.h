#ifndef _BLOCK_DEVICE_BDEV_H
#define _BLOCK_DEVICE_BDEV_H


#define NumberOfLuns 1

struct ns_entry {
  struct bdev_context *contexts[NumberOfLuns];
  struct spdk_nvme_ctrlr *ctrlr;
  struct spdk_nvme_ns *ns;
  struct ns_entry *next;
  struct spdk_nvme_qpair *qpair;
};

struct bdev_context {
  struct spdk_bdev *bdev;
  struct spdk_bdev_desc *bdev_desc;
  struct spdk_io_channel *bdev_io_channel;
  char *buff;
  const char *bdev_name;
  struct spdk_bdev_io_wait_entry *bdev_io_wait;
};


struct hello_world_sequence {
  struct ns_entry *ns_entry;
  char *buf;
  unsigned using_cmb_io;
  int is_completed;
};

#endif 