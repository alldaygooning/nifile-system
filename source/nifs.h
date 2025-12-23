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

#endif
