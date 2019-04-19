#include "dir.h"
#include "inode.h"
#include "testfs.h"
#include "tx.h"
#include "stdio.h"
#include "stdint.h"
#include "async.h"
#include "inode_alternate.h"

#define MIN(x, y) (((x) < (y)) ? (x) : (y))


int cmd_cat(struct super_block *sb, struct context *c) {
  char *buf;
  int inode_nr;
  struct inode *in;
  int ret = 0;
  int sz;
  int i;

  if (c->nargs < 2) {
    return -EINVAL;
  }

  for (i = 1; ret == 0 && i < c->nargs; i++) {
    inode_nr = testfs_dir_name_to_inode_nr(c->cur_dir, c->cmd[i]);
    if (inode_nr < 0) return inode_nr;
    in = testfs_get_inode(sb, inode_nr);
    if (testfs_inode_get_type(in) == I_DIR) {
      ret = -EISDIR;
      goto out;
    }
    sz = testfs_inode_get_size(in);
    if (sz > 0) {
      buf = malloc(sz + 1);
      if (!buf) {
        ret = -ENOMEM;
        goto out;
      }
      testfs_read_data(in, 0, buf, sz);
      buf[sz] = 0;
      printf("%s\n", buf);
      free(buf);
    }
  out:
    testfs_put_inode(in);
  }

  return ret;
}

int cmd_catr(struct super_block *sb, struct context *c) {
  char *cdir = ".";
  int inode_nr;
  struct inode *in;
  struct inode *tmp_inode;
  int offset = 0;
  struct dirent *d;
  int ret = 0;
  int sz;
  char *buf;

  if (c->nargs > 2) {
    return -EINVAL;
  }

  if (c->nargs == 2) {
    cdir = c->cmd[1];
  }
  assert(c->cur_dir);

  /* Store the current directory. */
  tmp_inode = c->cur_dir;

  /* Get the inode number that corresponds to the provided
   * directory name. If no directory name is specified, search
   * for the current directory. */
  inode_nr = testfs_dir_name_to_inode_nr(c->cur_dir, cdir);
  if (inode_nr < 0) return inode_nr;

  /* Get the corresponding inode object. */
  in = testfs_get_inode(sb, inode_nr);

  for (; (d = testfs_next_dirent(in, &offset)); free(d)) {
    struct inode *cin;

    if (d->d_inode_nr < 0) continue;

    if ((strcmp(D_NAME(d), ".") != 0) && (strcmp(D_NAME(d), "..") != 0)) {
      cin = testfs_get_inode(testfs_inode_get_sb(in), d->d_inode_nr);

      if (testfs_inode_get_type(cin) == I_DIR) {
        c->cur_dir = cin;
        ret = cmd_catr(sb, c);
        if (ret < 0) {
          testfs_put_inode(cin);
          goto out;
        }
        c->cur_dir = tmp_inode;
      } else {
        printf("%s:\n", D_NAME(d));
        sz = testfs_inode_get_size(cin);
        if (sz > 0) {
          buf = malloc(sz + 1);
          if (!buf) {
            testfs_put_inode(cin);
            ret = -ENOMEM;
            goto out;
          }
          testfs_read_data(cin, 0, buf, sz);
          buf[sz] = 0;
          printf("%s\n", buf);
          free(buf);
        }
      }
      testfs_put_inode(cin);
    }
  }
out:
  testfs_put_inode(in);

  return 0;
}

int cmd_write(struct super_block *sb, struct context *c) {
  int inode_nr;
  struct inode *in;
  int size;
  int ret = 0;
  char *filename = NULL;
  char *content = NULL;

  if (c->nargs != 3) {
    return -EINVAL;
  }

  filename = c->cmd[1];
  content = c->cmd[2];

  inode_nr = testfs_dir_name_to_inode_nr(c->cur_dir, filename);
  if (inode_nr < 0) return inode_nr;
  in = testfs_get_inode(sb, inode_nr);
  if (testfs_inode_get_type(in) == I_DIR) {
    ret = -EISDIR;
    goto out;
  }
  size = strlen(content);
  struct future f;
  future_init(&f);
  testfs_tx_start(sb, TX_WRITE);
  ret = testfs_write_data_alternate_async(in, &f, 0, content, size);
  spin_wait(&f);
  if (ret >= 0) {
    testfs_truncate_data(in, size);
  }
  testfs_sync_inode(in);
  testfs_tx_commit(sb, TX_WRITE);
out:
  testfs_put_inode(in);
  return ret;
}

int cmd_import(struct super_block *sb, struct context *c) {
  if (c->nargs != 3) {
    return -EINVAL;
  }
  char *filename = c->cmd[1];
  int ret = testfs_create_file_or_dir(sb, c->cur_dir, I_FILE, filename);
  if (ret < 0) return ret;
  int inode_nr = testfs_dir_name_to_inode_nr(c->cur_dir, filename);
  if (inode_nr < 0) return inode_nr;
  struct inode *in = testfs_get_inode(sb, inode_nr);
  if (testfs_inode_get_type(in) == I_DIR) {
    ret = -EISDIR;
    goto out;
  }

  uint8_t buffer[BLOCK_SIZE];
  FILE *fp = fopen(c->cmd[2], "r");
  if (fp == NULL) {
    ret = -ENOENT;
    goto out;
  }
  int fd;
  int start = 0;
  testfs_tx_start(sb, TX_WRITE);
  while (!feof(fp)) {
    fd = fread(buffer, sizeof(uint8_t), BLOCK_SIZE, fp);
    if (fd < 0) {
      ret = fd;
      break;
    }
    ret = testfs_write_data(in, start, buffer, fd);
    start += fd;
    if (ret < 0) {
      break;
    }
  }
  if (ret >= 0) {
    testfs_truncate_data(in, start);
  }
  printf("size=%d\n", start);
  testfs_sync_inode(in);
  testfs_tx_commit(sb, TX_WRITE);
out:
  if (fp != NULL) {
    fclose(fp);
  }
  testfs_put_inode(in);
  return ret;
}

int cmd_export(struct super_block *sb, struct context *c) {
  uint8_t buffer[BLOCK_SIZE];
  int ret = 0;
  int inode_nr = testfs_dir_name_to_inode_nr(c->cur_dir, c->cmd[1]);
  if (inode_nr < 0) return inode_nr;
  struct inode *in = testfs_get_inode(sb, inode_nr);
  FILE *fp = fopen(c->cmd[2], "w");
  if (fp == NULL) {
    ret = -ENOENT;
    goto out;
  }
  if (testfs_inode_get_type(in) == I_DIR) {
    ret = -EISDIR;
    goto out;
  }
  int size = testfs_inode_get_size(in);
  printf("size=%d\n", size);
  int start = 0;
  int nbytes = 0;
  while (start < size) {
    nbytes = MIN(size-start, BLOCK_SIZE);
    testfs_read_data(in, start, buffer, nbytes);
    fwrite(buffer, sizeof(uint8_t), nbytes, fp);
    start += BLOCK_SIZE;
  }
out:
  if (fp != NULL) {
    fclose(fp);
  }
  testfs_put_inode(in);
}

int cmd_owrite(struct super_block *sb, struct context *c) {
  int inode_nr;
  struct inode *in;
  int size;
  int ret = 0;
  long offset;
  char *filename = NULL;
  char *content = NULL;
  char *temp = NULL;

  if (c->nargs != 4) {
    return -EINVAL;
  }

  filename = c->cmd[1];
  offset = strtol(c->cmd[2], &temp, 10);
  if (*temp != '\0') return -1;
  content = c->cmd[3];

  inode_nr = testfs_dir_name_to_inode_nr(c->cur_dir, filename);
  if (inode_nr < 0) return inode_nr;
  in = testfs_get_inode(sb, inode_nr);
  if (testfs_inode_get_type(in) == I_DIR) {
    ret = -EISDIR;
    goto out;
  }
  size = strlen(content);
  testfs_tx_start(sb, TX_WRITE);
  ret = testfs_write_data(in, offset, content, size);
  if (ret >= 0) {
    testfs_truncate_data(in, size + offset);
  }
  testfs_sync_inode(in);
  testfs_tx_commit(sb, TX_WRITE);
out:
  testfs_put_inode(in);
  return ret;
}
