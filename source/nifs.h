#ifndef _NIFS_H
#define _NIFS_H

#include <linux/init.h>
#include <linux/module.h>

#define MODULE_NAME "nifs"
#define LOG(fmt, ...) pr_info("[" MODULE_NAME "]: " fmt, ##__VA_ARGS__)
#define DEBUG(fmt, ...) pr_info("DEBUG %s: " fmt, __func__, ##__VA_ARGS__)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("allgaygooning");
MODULE_DESCRIPTION("G-E-N-I-U-S file system");
MODULE_VERSION("pre-beta.snapshot.a1.0.1");

extern struct file_system_type nifs_fs_type; // NOLINT

#define NIFS_ROOT_INODE     1000
#define NIFS_DIR_INODE      200
#define NIFS_NEXT_INODE     100

#define NIFS_DOT_ENTRY      "."
#define NIFS_DOTDOT_ENTRY   ".."
#define NIFS_DIR_NAME       "dir"

struct nifs_file_data {
  char* data;
  size_t size;
  size_t capacity;
};

struct nifs_file_entry {
  char* name;                   // File name
  ulong inode_number;           // Inode number
  ulong parent_inode;           // Parent inode number
  struct nifs_file_data* data;  // Pointer to file data
  struct list_head parent_list; // For parent directory's files list
  struct list_head global_list; // For global nifs_files list
};

struct nifs_dir_entry {
  char* name;
  ulong inode_number;
  ulong parent_inode;
  struct list_head files;
  struct list_head subdirs;
  struct list_head parent_list;
  struct list_head global_list;
};

extern struct list_head nifs_directories;
extern struct list_head nifs_files;
extern int nifs_next_inode;

#endif
