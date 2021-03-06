#ifndef _DIR_H
#define _DIR_H

#include "inode.h"
#include "super.h"

struct dirent {
  int d_name_len;
  int d_inode_nr;
};

#define D_NAME(d) ((char *)(d) + sizeof(struct dirent))

struct dirent *testfs_next_dirent(struct inode *dir, int *offset);
int testfs_dir_name_to_inode_nr(struct inode *dir, char *name);
int testfs_create_file_or_dir(struct super_block *sb, struct inode *dir,
                              inode_type type, char *name);
int testfs_make_root_dir(struct super_block *sb);

#endif /* _DIR_H */
