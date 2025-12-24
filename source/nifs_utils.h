#ifndef _NIFS_UTILS_H
#define _NIFS_UTILS_H

#include "nifs.h"

#include <linux/types.h>
#include <linux/list.h>

struct nifs_dir_entry* nifs_find_directory(ulong inode);
struct nifs_file_entry* nifs_find_file_in_dir(struct nifs_dir_entry* dir, const char* name);
struct nifs_dir_entry* nifs_find_subdir(struct nifs_dir_entry* dir, const char* name);

#endif
