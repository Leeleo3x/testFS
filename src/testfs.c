#define _GNU_SOURCE
#include "testfs.h"
#include <getopt.h>
#include <stdbool.h>
#include "dir.h"
#include "inode.h"
#include "super.h"
#include "tx.h"
#include "device.h"
#include "bench.h"

static int cmd_help(struct super_block*, struct context *c);
static int cmd_quit(struct super_block*, struct context *c);
static bool can_quit = false;

#define PROMPT printf("%s", "% ")
#define FS_DOES_NOT_EXIST_ERROR "Cannot execute \"%s\" because a file "\
                                "system does not exist on the underlying "\
                                "device. Please run \"mkfs\" first.\n"

static struct {
  const char *name;
  int (*func)(struct super_block *sb, struct context *c);
  int max_args;
} cmdtable[] = {
    /* menus */
    {
        "?",
        cmd_help,
        1,
    },
    {
        "mkfs",
        cmd_mkfs,
        2,
    },
    {
        "cd",
        cmd_cd,
        2,
    },
    {
        "pwd",
        cmd_pwd,
        1,
    },
    {
        "ls",
        cmd_ls,
        2,
    },
    {
        "lsr",
        cmd_lsr,
        2,
    },
    {
        "touch",
        cmd_create,
        MAX_ARGS,
    },
    {
        "stat",
        cmd_stat,
        MAX_ARGS,
    },
    {
        "rm",
        cmd_rm,
        2,
    },
    {
        "mkdir",
        cmd_mkdir,
        2,
    },
    {
        "cat",
        cmd_cat,
        MAX_ARGS,
    },
    {
        "catr",
        cmd_catr,
        2,
    },
    {
        "write",
        cmd_write,
        2,
    },
    {
        "export",
        cmd_export,
        2,
    },
    {
        "owrite",
        cmd_owrite,
        3,
    },
    {
        "import",
        cmd_import,
        2,
    },
    {
        "checkfs",
        cmd_checkfs,
        1,
    },
    {
        "benchmark",
        cmd_benchmark,
        3,
    },
    {
        "quit",
        cmd_quit,
        1,
    },
    {NULL, NULL}};

// These commands are the only commands that can be executed
// if a file system does not exist (i.e. the user has not run mkfs)
static const char *non_fs_commands[] =
  {"?", "quit", "mkfs", "benchmark", NULL};

static bool fs_exists(struct context *c) {
  return testfs_inode_get_type(c->cur_dir) == I_DIR;
}

static bool can_execute_command(struct context *c, char *command) {
  if (fs_exists(c)) {
    return true;
  }
  for (size_t i = 0; non_fs_commands[i] != NULL; i++) {
    if (strcmp(non_fs_commands[i], command) == 0) {
      return true;
    }
  }
  return false;
}

static int cmd_help(struct super_block *sb, struct context *c) {
  int i = 0;

  printf("Commands:\n");
  for (; cmdtable[i].name; i++) {
    printf("%s\n", cmdtable[i].name);
  }
  return 0;
}

static int cmd_quit(struct super_block *sb, struct context *c) {
  printf("Bye!\n");
  can_quit = true;
  return 0;
}

// tokenize the command, place it in the form of cmd, arg1, arg2 ... in
// context c->cmd[] structure. pass c and superblock sb to cmdtable->func

static void handle_command(struct super_block *sb, struct context *c,
                           char *name, char *args) {
  int i;
  int j = 0;
  if (name == NULL) return;

  /* c->cmd[0] contains the command's name. This statement must be executed
   * at every invocation. Otherwise, if the command does not exist, c->cmd[0]
   * will either contain the last successful executed command, or it will be
   * undefined possibly resulting in a Segmentation fault. */
  c->cmd[j++] = name;

  for (i = 0; cmdtable[i].name; i++) {
    if (strcmp(name, cmdtable[i].name) == 0) {
      if (!can_execute_command(c, name)) {
        printf(FS_DOES_NOT_EXIST_ERROR, name);
        return;
      }

      char *token = args;
      assert(cmdtable[i].func);

      // context->cmd contains the command's arguments, starting from index 1.
      while (j < cmdtable[i].max_args &&
             (c->cmd[j] = strtok(token, " \t\n")) != NULL) {
        j++;
        token = NULL;
      }
      if ((c->cmd[j] = strtok(token, "\n")) != NULL) {
        j++;
      }
      for (c->nargs = j++; j <= MAX_ARGS; j++) {
        c->cmd[j] = NULL;
      }

      errno = cmdtable[i].func(sb, c);
      if (errno < 0) {
        errno = -errno;
        WARN(c->cmd[0]);
      }
      return;
    }
  }
  printf("%s: command not found: type ? for help...\n", c->cmd[0]);
}

static void usage(const char *progname) {
  fprintf(stdout, "Usage: %s [-ch][--help] rawfile\n", progname);
  exit(1);
}

struct args {
  const char *disk;  // name of disk
  int corrupt;       // to corrupt or not
};

static struct args *parse_arguments(int argc, char *const argv[]) {
  static struct args args = {0};
  // struct options -
  // name of the option.
  // has arg {no_argument, required_argument, optional_argument}
  // flag ptr - 0 means val is the value that identifies this option
  // flag ptr - non null - address of int variable which is flag for the option
  // val - c or h
  static struct option long_options[] = {
      {"corrupt", no_argument, 0, 'c'},
      {"help", no_argument, 0, 'h'},
      {0, 0, 0, 0},
  };
  int running = 1;

  while (running) {
    int option_index = 0;
    // getopt_long - decode options from argv.
    // getopt_long (int argc, char *const *argv, const char *shortopts, const
    // struct option *longopts, int *indexptr)
    //
    int c = getopt_long(argc, argv, "ch", long_options, &option_index);
    switch (c) {
      case -1:
        running = 0;
        break;
      case 0:
        break;
      case 'c':
        args.corrupt = 1;
        break;
      case 'h':
        usage(argv[0]);
        break;
      case '?':
        usage(argv[0]);
        break;
      default:
        abort();
    }
  }
  // optind - index of next variable to be processed in argv.
  if (argc - optind == 1)  {
    args.disk = argv[optind];
  }
  else {
    args.disk = "/tmp/file";
  }

  return &args;
}


void testfs_main(struct filesystem *fs) {
  char *line = NULL;
  ssize_t nr;
  size_t line_size = 0;
  struct context c;
  c.fs = fs;
  c.cur_dir = NULL;
  int ret;
  // args->disk contains the name of the disk file.
  // initializes the in memory structure sb with data that is
  // read from the disk. after successful execution, we have
  // sb initialized to dsuper_block read from disk.
  ret = testfs_init_super_block(fs, 0);
  if (ret) {
	EXIT("testfs_init_super_block");
  }
  /* if the inode does not exist in the inode_hash_map (which
   is an inmemory map of all inode blocks, create a new inode by
   allocating memory to it. read the dinode from disk into that
   memory inode
   */
  c.cur_dir = testfs_get_inode(fs->sb, 0); /* root dir */
  for (; PROMPT, (nr = getline(&line, &line_size, stdin)) != EOF;) {
    char *name;
    char *args;

    printf("command: %s\n", line);
    name = strtok(line, " \t\n");
    args = strtok(NULL, "\n");
    handle_command(c.fs->sb, &c, name, args);
    if (can_quit) {
      break;
    }
  }

  free(line);

  // Need to compute this before removing the in-memory inode
  bool file_system_exists = fs_exists(&c);

  // decrement inode count by 1. remove inode from in_memory hash map if
  // inode count has become 0.
  testfs_put_inode(c.cur_dir);

  if (file_system_exists) {
    testfs_close_super_block(fs->sb);
  }
  dev_stop(fs);
}

int main(int argc, char *const argv[]) {
  // context contains command line arguments/parameters,
  // inode of directory from which cmd was issued, and no of args.

  struct args *args = parse_arguments(argc, argv);
  dev_init(args->disk, testfs_main);
  return 0;
}
