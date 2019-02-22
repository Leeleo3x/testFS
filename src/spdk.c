#include "spdk/stdinc.h"

#include "spdk/env.h"
#include "spdk/nvme.h"

#include "block.h"
#include "testfs.h"

static char zero[BLOCK_SIZE] = {0};

struct ctrlr_entry {
  struct spdk_nvme_ctrlr *ctrlr;
  struct ctrlr_entry *next;
  char name[1024];
};

struct ns_entry {
  struct spdk_nvme_ctrlr *ctrlr;
  struct spdk_nvme_ns *ns;
  struct ns_entry *next;
  struct spdk_nvme_qpair *qpair;
};

static struct ctrlr_entry *g_controllers = NULL;
static struct ns_entry *g_namespaces = NULL;

static void register_ns(struct spdk_nvme_ctrlr *ctrlr,
                        struct spdk_nvme_ns *ns) {
  struct ns_entry *entry;
  const struct spdk_nvme_ctrlr_data *cdata;

  /*
   * spdk_nvme_ctrlr is the logical abstraction in SPDK for an NVMe
   *  controller.  During initialization, the IDENTIFY data for the
   *  controller is read using an NVMe admin command, and that data
   *  can be retrieved using spdk_nvme_ctrlr_get_data() to get
   *  detailed information on the controller.  Refer to the NVMe
   *  specification for more details on IDENTIFY for NVMe controllers.
   */
  cdata = spdk_nvme_ctrlr_get_data(ctrlr);

  if (!spdk_nvme_ns_is_active(ns)) {
    printf("Controller %-20.20s (%-20.20s): Skipping inactive NS %u\n",
           cdata->mn, cdata->sn, spdk_nvme_ns_get_id(ns));
    return;
  }

  entry = malloc(sizeof(struct ns_entry));
  if (entry == NULL) {
    perror("ns_entry malloc");
    exit(1);
  }

  entry->ctrlr = ctrlr;
  entry->ns = ns;
  entry->next = g_namespaces;
  g_namespaces = entry;

  printf("  Namespace ID: %d size: %juGB\n", spdk_nvme_ns_get_id(ns),
         spdk_nvme_ns_get_size(ns) / 1000000000);
  printf(" sector size: %d\n", spdk_nvme_ns_get_sector_size(ns));
  entry->qpair = spdk_nvme_ctrlr_alloc_io_qpair(entry->ctrlr, NULL, 0);
  if (entry->qpair == NULL) {
    printf("ERROR: spdk_nvme_ctrlr_alloc_io_qpair() failed\n");
    return;
  }
}

struct hello_world_sequence {
  struct ns_entry *ns_entry;
  char *buf;
  unsigned using_cmb_io;
  int is_completed;
};

static void complete(void *arg, const struct spdk_nvme_cpl *completion) {
  struct hello_world_sequence *sequence = arg;
  sequence->is_completed = 1;
}

void read_blocks(struct super_block *sb, char *blocks, int start,
                        int nr) {
  printf("READ BLOCKS: %d %d\n", start, nr);
  struct ns_entry *entry= g_namespaces;
  struct hello_world_sequence sequence;
  entry->qpair = spdk_nvme_ctrlr_alloc_io_qpair(entry->ctrlr, NULL, 0);
  if (entry->qpair == NULL) {
    printf("ERROR: spdk_nvme_ctrlr_alloc_io_qpair() failed\n");
    return;
  }

  sequence.buf = spdk_zmalloc(nr * BLOCK_SIZE, 0, NULL, SPDK_ENV_SOCKET_ID_ANY,
                               SPDK_MALLOC_DMA);
  int rc = spdk_nvme_ns_cmd_read(entry->ns, entry->qpair, sequence.buf,
                                 start, /* LBA start */
                                 nr,    /* number of LBAs */
                                 complete, &sequence, 0);
  if (rc != 0) {
    fprintf(stderr, "starting read I/O failed\n");
    exit(1);
  }
  sequence.is_completed = 0;
  sequence.ns_entry = entry;
  while (!sequence.is_completed) {
    spdk_nvme_qpair_process_completions(entry->qpair, 0);
  }
  memcpy(blocks, sequence.buf, nr * BLOCK_SIZE);
  spdk_free(sequence.buf);
  // spdk_nvme_ctrlr_free_io_qpair(entry->qpair);
}

void write_blocks(struct super_block *sb, char *blocks, int start,
                         int nr) {
  printf("WRITE BLOCKS: %d %d\n", start, nr);
  struct ns_entry *entry = g_namespaces;
  struct hello_world_sequence sequence;
  sequence.using_cmb_io = 1;
  sequence.buf =
      spdk_nvme_ctrlr_alloc_cmb_io_buffer(entry->ctrlr, nr * BLOCK_SIZE);
  if (sequence.buf == NULL) {
    sequence.using_cmb_io = 0;
    sequence.buf = spdk_zmalloc(nr * BLOCK_SIZE, 0, NULL,
                                SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA);
  }
  if (sequence.buf == NULL) {
    printf("ERROR: write buffer allocation failed\n");
    return;
  }
  if (sequence.using_cmb_io) {
    printf("INFO: using controller memory buffer for IO\n");
  } else {
    printf("INFO: using host memory buffer for IO\n");
  }
  sequence.is_completed = 0;
  sequence.ns_entry = entry;

  memcpy(sequence.buf, blocks, nr * BLOCK_SIZE);
  int rc = spdk_nvme_ns_cmd_write(entry->ns, entry->qpair, sequence.buf,
                                  start, /* LBA start */
                                  nr,    /* number of LBAs */
                                  complete, &sequence, 0);
  if (rc != 0) {
    fprintf(stderr, "starting write I/O failed\n");
    exit(1);
  }
  while (!sequence.is_completed) {
    spdk_nvme_qpair_process_completions(entry->qpair, 0);
  }
  if (sequence.using_cmb_io) {
    spdk_nvme_ctrlr_free_cmb_io_buffer(entry->ctrlr, sequence.buf, nr * BLOCK_SIZE);
  } else {
    spdk_free(sequence.buf);
  }
  // spdk_nvme_ctrlr_free_io_qpair(entry->qpair);
}

void zero_blocks(struct super_block *sb, int start, int nr) {
  int i;

  for (i = 0; i < nr; i++) {
    write_blocks(sb, zero, start + i, 1);
  }
}

static bool probe_cb(void *cb_ctx, const struct spdk_nvme_transport_id *trid,
                     struct spdk_nvme_ctrlr_opts *opts) {
  printf("Attaching to %s\n", trid->traddr);

  return true;
}

static void attach_cb(void *cb_ctx, const struct spdk_nvme_transport_id *trid,
                      struct spdk_nvme_ctrlr *ctrlr,
                      const struct spdk_nvme_ctrlr_opts *opts) {
  int nsid, num_ns;
  struct ctrlr_entry *entry;
  struct spdk_nvme_ns *ns;
  const struct spdk_nvme_ctrlr_data *cdata = spdk_nvme_ctrlr_get_data(ctrlr);

  entry = malloc(sizeof(struct ctrlr_entry));
  if (entry == NULL) {
    perror("ctrlr_entry malloc");
    exit(1);
  }

  printf("Attached to %s\n", trid->traddr);

  snprintf(entry->name, sizeof(entry->name), "%-20.20s (%-20.20s)", cdata->mn,
           cdata->sn);

  entry->ctrlr = ctrlr;
  entry->next = g_controllers;
  g_controllers = entry;

  /*
   * Each controller has one or more namespaces.  An NVMe namespace is basically
   *  equivalent to a SCSI LUN.  The controller's IDENTIFY data tells us how
   *  many namespaces exist on the controller.  For Intel(R) P3X00 controllers,
   *  it will just be one namespace.
   *
   * Note that in NVMe, namespace IDs start at 1, not 0.
   */
  num_ns = spdk_nvme_ctrlr_get_num_ns(ctrlr);
  printf("Using controller %s with %d namespaces.\n", entry->name, num_ns);
  for (nsid = 1; nsid <= num_ns; nsid++) {
    ns = spdk_nvme_ctrlr_get_ns(ctrlr, nsid);
    if (ns == NULL) {
      continue;
    }
    register_ns(ctrlr, ns);
  }
}

static void cleanup(void) {
  struct ns_entry *ns_entry = g_namespaces;
  struct ctrlr_entry *ctrlr_entry = g_controllers;

  while (ns_entry) {
    struct ns_entry *next = ns_entry->next;
    free(ns_entry);
    ns_entry = next;
  }

  while (ctrlr_entry) {
    struct ctrlr_entry *next = ctrlr_entry->next;

    spdk_nvme_detach(ctrlr_entry->ctrlr);
    free(ctrlr_entry);
    ctrlr_entry = next;
  }
}

int spdk_init() {
  int rc;
  struct spdk_env_opts opts;

  /*
   * SPDK relies on an abstraction around the local environment
   * named env that handles memory allocation and PCI device operations.
   * This library must be initialized first.
   *
   */
  spdk_env_opts_init(&opts);
  opts.name = "hello_world";
  opts.shm_id = 0;
  if (spdk_env_init(&opts) < 0) {
    fprintf(stderr, "Unable to initialize SPDK env\n");
    return 1;
  }

  printf("Initializing NVMe Controllers\n");

  /*
   * Start the SPDK NVMe enumeration process.  probe_cb will be called
   *  for each NVMe controller found, giving our application a choice on
   *  whether to attach to each controller.  attach_cb will then be
   *  called for each controller after the SPDK NVMe driver has completed
   *  initializing the controller we chose to attach.
   */
  rc = spdk_nvme_probe(NULL, NULL, probe_cb, attach_cb, NULL);
  if (rc != 0) {
    fprintf(stderr, "spdk_nvme_probe() failed\n");
    cleanup();
    return 1;
  }

  if (g_controllers == NULL) {
    fprintf(stderr, "no NVMe controllers found\n");
    cleanup();
    return 1;
  }

  printf("Initialization complete.\n");
  return 0;
}
