#ifndef _SPDK_H
#define _SPDK_H

struct ns_entry {
  struct spdk_nvme_ctrlr *ctrlr;
  struct spdk_nvme_ns *ns;
  struct ns_entry *next;
  struct spdk_nvme_qpair *qpair;
};

struct hello_world_sequence {
  struct ns_entry *ns_entry;
  char *buf;
  unsigned using_cmb_io;
  int is_completed;
};

#endif /* _SPDK_H */
