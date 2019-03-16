#ifndef _BLOCK_H
#define _BLOCK_H
#include "super.h"

typedef void (* block_read_cb)(char *blocks, void *arg);
typedef void (* block_write_cb)(void *arg);

void write_blocks(struct super_block *sb, char *blocks, uint64_t start, size_t nr, block_write_cb cb, void *arg);
void read_blocks(struct super_block *sb, char *blocks, uint64_t start, size_t nr, block_read_cb cb, void *arg);
void zero_blocks(struct super_block *sb, int start, int nr, block_write_cb cb, void *arg);

#endif /* _BLOCK_H */
