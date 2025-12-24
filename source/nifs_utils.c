#include "nifs_utils.h"

struct nifs_dir_entry* nifs_find_directory(ulong inode) {
  struct nifs_dir_entry* dir;
  list_for_each_entry(dir, &nifs_directories, global_list) {
    if (dir->inode_number == inode) {
      return dir;
    }
  }
  return NULL;
}

struct nifs_file_entry* nifs_find_file_in_dir(struct nifs_dir_entry* dir, const char* name) {
  if (!dir) {
    return NULL;
  }

  struct nifs_file_entry* file;
  list_for_each_entry(file, &dir->files, parent_list) {
    if (strcmp(file->name, name) == 0) {
      return file;
    }
  }
  return NULL;
}

struct nifs_dir_entry* nifs_find_subdir(struct nifs_dir_entry* dir, const char* name) {
  if (!dir) {
    return NULL;
  }

  struct nifs_dir_entry* subdir;
  list_for_each_entry(subdir, &dir->subdirs, parent_list) {
    if (strcmp(subdir->name, name) == 0) {
      return subdir;
    }
  }
  return NULL;
}
