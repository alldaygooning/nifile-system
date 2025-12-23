#ifndef _NIFS_H
#define _NIFS_H

#include <linux/init.h>
#include <linux/module.h>

#define MODULE_NAME "nifs"
#define LOG(fmt, ...) pr_info("[" MODULE_NAME "]: " fmt, ##__VA_ARGS__)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("allgaygooning");
MODULE_DESCRIPTION("G-E-N-I-U-S file system");
MODULE_VERSION("pre-beta.snapshot.a1.0.1");

extern struct file_system_type nifs_fs_type; // NOLINT

#define NIFS_ROOT_INODE     1000
#define NIFS_TESTFILE_INODE 101
#define NIFS_DIR_INODE      200

#define NIFS_DOT_ENTRY      "."
#define NIFS_DOTDOT_ENTRY   ".."
#define NIFS_TESTFILE_NAME  "test.txt"
#define NIFS_DIR_NAME       "dir"

#endif
