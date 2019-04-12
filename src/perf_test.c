#include "device.h"
#include "super.h"
#include "testfs.h"

#include <stdio.h>
#include <stdlib.h>
#include <dir.h>
#include <sys/time.h>

char *read_file(const char *file, size_t *size) {
  FILE *f = fopen(file, "rb");
  fseek(f, 0, SEEK_END);
  *size = ftell(f);
  fseek(f, 0, SEEK_SET);  /* same as rewind(f); */
  char *string = malloc(*size + 1);
  fread(string, 1, *size, f);
  fclose(f);
  string[*size] = 0;
  return string;
}

int test_write(struct super_block *sb, struct context *c, char *filename, char *buf, size_t size) {
  int ret = testfs_create_file_or_dir(sb, c->cur_dir, I_FILE, filename);
  int inode_nr = testfs_dir_name_to_inode_nr(c->cur_dir, filename);
  if (inode_nr < 0) return inode_nr;
  struct inode *in = testfs_get_inode(sb, inode_nr);
  testfs_tx_start(sb, TX_WRITE);
//  testfs_write_data(in, 0, buf, size);
  testfs_write_data_no_lock(in, 0, buf, size);
  wait_context(sb->fs->contexts[INODE_LUN]);
  testfs_sync_inode(in);
  testfs_tx_commit(sb, TX_WRITE);
  testfs_put_inode(in);
}


#define N 40
#define M 40

void perf_main(struct filesystem *fs) {
  struct context *c = malloc(sizeof(struct context));
  struct super_block *sb = malloc(sizeof(struct super_block));
  c->fs = fs;
  fs->sb = sb;
  sb->fs = fs;
  cmd_mkfs(fs->sb, c);
  sb = fs->sb;
  size_t size;
  char *data = read_file("cmake_install.cmake", &size);
  struct timeval t0, t1;
  gettimeofday(&t0, NULL);
  for (int i = 0; i < N; i++) {
	char file[6];
	sprintf(file, "%d", i);
	testfs_create_file_or_dir(fs->sb, c->cur_dir, I_DIR, file);
	int inode_nr = testfs_dir_name_to_inode_nr(c->cur_dir, file);
	struct inode *dir_inode = testfs_get_inode(fs->sb, inode_nr);
	testfs_put_inode(c->cur_dir);
	c->cur_dir = dir_inode;
    for (int j = 0; j < M; j++) {
	  sprintf(file, "%d", j);
	  test_write(fs->sb, c, file, data, size);
    }
	dir_inode = testfs_get_inode(fs->sb, 0);
	testfs_put_inode(c->cur_dir);
	c->cur_dir = dir_inode;
  }
  wait_context(fs->contexts[INODE_LUN]);
  wait_context(fs->contexts[DATA_LUN]);
  gettimeofday(&t1, NULL);
  printf("Did %u calls in %.2g seconds\n", N * M, t1.tv_sec - t0.tv_sec + 1E-6 * (t1.tv_usec - t0.tv_usec));
  dev_stop(fs);
}


int main(int argc, char *const argv[]) {
  dev_init("", perf_main);
  return 0;
}
