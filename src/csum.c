#include "csum.h"
#include <assert.h>
#include "block.h"
#include "super.h"

/* returns 0 on error */
int testfs_get_csum(struct super_block *sb, int block_nr) {
  assert(sb);
  assert(sb->csum_table);

  if (block_nr < MAX_NR_CSUMS) {
    return sb->csum_table[block_nr];
  }

  return 0;
}

static void testfs_write_csum(struct super_block *sb, int block_nr) {
  int nr = block_nr * sizeof(int) / BLOCK_SIZE;
  char *table = (char *)sb->csum_table;

  assert(table);
  write_blocks(sb, table + (nr * BLOCK_SIZE), sb->sb.csum_table_start + nr, 1);
}

static void testfs_write_csum_asnyc(
    struct super_block *sb, struct future *f, int block_nr) {
  int nr = block_nr * sizeof(int) / BLOCK_SIZE;
  char *table = (char *)sb->csum_table;

  assert(table);
  write_blocks_async(
    sb,
    METADATA_REACTOR,
    f,
    table + (nr * BLOCK_SIZE),
    sb->sb.csum_table_start + nr,
    1
  );
}

void testfs_put_csum_async(
    struct super_block *sb, struct future *f, int phy_block_nr, int csum) {
  int block_nr = phy_block_nr - sb->sb.data_blocks_start;
  assert(sb);
  assert(sb->csum_table);

  assert(block_nr >= 0 && block_nr < MAX_NR_CSUMS);
  sb->csum_table[block_nr] = csum;
  testfs_write_csum_asnyc(sb, f, block_nr);
}

void testfs_put_csum(struct super_block *sb, int phy_block_nr, int csum) {
  int block_nr = phy_block_nr - sb->sb.data_blocks_start;
  assert(sb);
  assert(sb->csum_table);

  assert(block_nr >= 0 && block_nr < MAX_NR_CSUMS);
  sb->csum_table[block_nr] = csum;
  testfs_write_csum(sb, block_nr);
}

/* could use other algorithm if you want */
int testfs_calculate_csum(const char *buf, const int size) {
  const int *ibuf = (const int *)buf;
  const int count = size / sizeof(int);
  int csum = 0;
  int i;

  assert(size % sizeof(int) == 0);
  for (i = 0; i < count; i++) {
    csum ^= ibuf[i];
  }

  return csum;
}

int testfs_verify_csum(struct super_block *sb, int phy_block_nr) {
  char block[BLOCK_SIZE];
  int csum;
  int block_nr = phy_block_nr - sb->sb.data_blocks_start;

  assert(block_nr >= 0 && block_nr < MAX_NR_CSUMS);
  read_blocks(sb, block, phy_block_nr, 1);
  csum = testfs_calculate_csum(block, sizeof(block));

  if (csum != sb->csum_table[block_nr]) {
    printf("checksum error at block %d\n", phy_block_nr);
    return -EINVAL;
  }

  return 0;
}

void testfs_set_csum(struct super_block *sb, int phy_block_nr, int csum) {
  int csum_offset = phy_block_nr - sb->sb.data_blocks_start;
  assert(csum_offset >= 0 && csum_offset < MAX_NR_CSUMS);
  int csum_block_nr = csum_offset * sizeof(int) / BLOCK_SIZE;
  assert(csum_block_nr < CSUM_TABLE_SIZE);
  sb->csum_table[csum_offset] = csum;
  sb->csum_block_dirty[csum_block_nr] = true;
}

void testfs_flush_csum_async(struct super_block *sb, struct future *f) {
  char *table = (char *)sb->csum_table;

  for (int csum_block_nr = 0;
      csum_block_nr < CSUM_TABLE_SIZE; csum_block_nr++) {
    if (!(sb->csum_block_dirty[csum_block_nr])) {
      continue;
    }
    write_blocks_async(
      sb,
      METADATA_REACTOR,
      f,
      table + (csum_block_nr * BLOCK_SIZE),
      sb->sb.csum_table_start + csum_block_nr,
      1
    );
    sb->csum_block_dirty[csum_block_nr] = false;
  }
}

void testfs_flush_csum(struct super_block *sb) {
  char *table = (char *)sb->csum_table;

  for (int csum_block_nr = 0;
      csum_block_nr < CSUM_TABLE_SIZE; csum_block_nr++) {
    if (!(sb->csum_block_dirty[csum_block_nr])) {
      continue;
    }
    write_blocks(
      sb,
      table + (csum_block_nr * BLOCK_SIZE),
      sb->sb.csum_table_start + csum_block_nr,
      1
    );
    sb->csum_block_dirty[csum_block_nr] = false;
  }
}
