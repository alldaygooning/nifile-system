#include "nifs.h"

#include <linux/printk.h>

#include "nifs_utils.h"

// ====== CREATED FILES TRACKING ======

LIST_HEAD(nifs_directories);
LIST_HEAD(nifs_files);
int nifs_next_inode = NIFS_NEXT_INODE;

// ====== ====================== ======

static struct dentry* nifs_lookup(
    struct inode* parent_inode, struct dentry* child_dentry, unsigned int flag
);

static struct nifs_file_data* nifs_alloc_file_data(void);

static void nifs_free_file_data(struct nifs_file_data* fd);

static int nifs_create(struct mnt_idmap*, struct inode*, struct dentry*, umode_t, bool);

static int nifs_unlink(struct inode* parent_inode, struct dentry* child_dentry);

static int nifs_iterate(struct file* filp, struct dir_context* ctx);

static ssize_t nifs_read(struct file* filp, char __user* buffer, size_t len, loff_t* offset);

static ssize_t nifs_write(struct file* filp, const char __user* buffer, size_t len, loff_t* offset);

static struct dentry* nifs_mkdir(
    struct mnt_idmap* idmap, struct inode* parent_inode, struct dentry* child_dentry, umode_t mode
);

static int nifs_rmdir(struct inode* parent_inode, struct dentry* child_dentry);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wincompatible-pointer-types"
struct inode_operations nifs_inode_ops = {
    .lookup = nifs_lookup,
    .create = nifs_create,
    .unlink = nifs_unlink,
    .mkdir = nifs_mkdir,
    .rmdir = nifs_rmdir,
};
#pragma clang diagnostic pop

static const struct file_operations nifs_dir_operations = {
    .owner = THIS_MODULE,
    .iterate_shared = nifs_iterate,
};

static const struct file_operations nifs_file_operations = {
    .owner = THIS_MODULE,
    .read = nifs_read,
    .write = nifs_write,
    .llseek = generic_file_llseek,
    .open = simple_open,
};

static struct inode* nifs_get_inode(
    struct super_block* sb, const struct inode* dir, umode_t mode, ulong i_ino
) {
  struct inode* inode = new_inode(sb);
  if (!inode) {
    return NULL;
  }

  inode->i_ino = i_ino;

  inode_init_owner(&nop_mnt_idmap, inode, dir, mode);

  if (S_ISDIR(mode)) {
    inode->i_op = &nifs_inode_ops;
    inode->i_fop = &nifs_dir_operations;
    set_nlink(inode, 2);
  } else if (S_ISREG(mode)) {
    inode->i_op = &nifs_inode_ops;
    inode->i_fop = &nifs_file_operations;
    set_nlink(inode, 1);
  }

  return inode;
}

// ====== FILE MANAGEMENT ======

static int nifs_create(
    struct mnt_idmap* idmap,
    struct inode* parent_inode,
    struct dentry* child_dentry,
    umode_t mode,
    bool b
) {
  const char* name = child_dentry->d_name.name;
  struct nifs_dir_entry* parent_dir = nifs_find_directory(parent_inode->i_ino);

  if (!parent_dir) {
    return -ENOENT;  // Parent directory does not exist...
  }

  if (nifs_find_file_in_dir(parent_dir, name)) {
    return -EEXIST;  // File already exists!
  }

  if (nifs_find_subdir(parent_dir, name)) {
    return -EEXIST;  // Directory already exists!
  }

  struct nifs_file_entry* new_entry = kmalloc(sizeof(struct nifs_file_entry), GFP_KERNEL);
  if (!new_entry) {
    return -ENOMEM;
  }

  new_entry->name = kmalloc(strlen(name) + 1, GFP_KERNEL);
  new_entry->data = nifs_alloc_file_data();
  if (!new_entry->name || !new_entry->data) {
    nifs_free_file_data(new_entry->data);
    kfree(new_entry->name);
    kfree(new_entry);
    return -ENOMEM;
  }
  strcpy(new_entry->name, name);

  new_entry->inode_number = nifs_next_inode++;
  new_entry->parent_inode = parent_inode->i_ino;
  INIT_LIST_HEAD(&new_entry->parent_list);
  INIT_LIST_HEAD(&new_entry->global_list);

  list_add_tail(&new_entry->parent_list, &parent_dir->files);
  list_add_tail(&new_entry->global_list, &nifs_files);

  struct inode* inode = nifs_get_inode(
      parent_inode->i_sb, parent_inode, S_IFREG | (mode & ~S_IFMT), new_entry->inode_number
  );
  if (!inode) {
    list_del(&new_entry->parent_list);
    list_del(&new_entry->global_list);
    nifs_free_file_data(new_entry->data);
    kfree(new_entry->name);
    kfree(new_entry);
    return -ENOMEM;
  }

  inode->i_op = &nifs_inode_ops;
  inode->i_fop = &nifs_file_operations;

  d_add(child_dentry, inode);
  LOG("Created file: %s (inode %lu) in directory %lu\n",
      name,
      new_entry->inode_number,
      parent_inode->i_ino);
  return 0;
}

static int nifs_unlink(struct inode* parent_inode, struct dentry* child_dentry) {
  const char* name = child_dentry->d_name.name;
  struct nifs_dir_entry* parent_dir = nifs_find_directory(parent_inode->i_ino);

  if (!parent_dir)
    return -ENOENT;

  struct nifs_file_entry* file = nifs_find_file_in_dir(parent_dir, name);
  if (!file)
    return -ENOENT;

  LOG("Removing file: %s (inode %lu)\n", name, file->inode_number);

  list_del(&file->parent_list);
  list_del(&file->global_list);

  nifs_free_file_data(file->data);
  kfree(file->name);
  kfree(file);
  return 0;
}
// ====== =============== ======

// ====== DIR MANAGEMENT ======

static struct dentry* nifs_mkdir(
    struct mnt_idmap* idmap, struct inode* parent_inode, struct dentry* child_dentry, umode_t mode
) {
  LOG("MKDIR!");
  const char* name = child_dentry->d_name.name;
  struct nifs_dir_entry* parent_dir = nifs_find_directory(parent_inode->i_ino);

  if (!parent_dir) {
    return ERR_PTR(-ENOENT);  // Parent directory does not exist...
  }

  if (nifs_find_file_in_dir(parent_dir, name)) {
    return ERR_PTR(-EEXIST);  // File already exists!
  }

  if (nifs_find_subdir(parent_dir, name)) {
    return ERR_PTR(-EEXIST);  // Directory already exists!
  }

  struct nifs_dir_entry* new_dir = kmalloc(sizeof(struct nifs_dir_entry), GFP_KERNEL);
  if (!new_dir) {
    return ERR_PTR(-ENOMEM);
  }

  new_dir->name = kmalloc(strlen(name) + 1, GFP_KERNEL);
  if (!new_dir->name) {
    kfree(new_dir);
    return ERR_PTR(-ENOMEM);
  }
  strcpy(new_dir->name, name);

  new_dir->inode_number = nifs_next_inode++;
  new_dir->parent_inode = parent_inode->i_ino;
  INIT_LIST_HEAD(&new_dir->files);
  INIT_LIST_HEAD(&new_dir->subdirs);
  INIT_LIST_HEAD(&new_dir->parent_list);
  INIT_LIST_HEAD(&new_dir->global_list);

  list_add_tail(&new_dir->parent_list, &parent_dir->subdirs);
  list_add_tail(&new_dir->global_list, &nifs_directories);

  struct inode* inode = nifs_get_inode(
      parent_inode->i_sb, parent_inode, S_IFDIR | (mode & ~S_IFMT), new_dir->inode_number
  );
  if (!inode) {
    list_del(&new_dir->parent_list);
    list_del(&new_dir->global_list);
    kfree(new_dir->name);
    kfree(new_dir);
    return ERR_PTR(-ENOMEM);
  }

  inode->i_op = &nifs_inode_ops;
  inode->i_fop = &nifs_dir_operations;

  d_add(child_dentry, inode);
  LOG("Created directory: %s (inode %lu) in parent %lu\n",
      name,
      new_dir->inode_number,
      parent_inode->i_ino);

  return child_dentry;
}

static int nifs_rmdir(struct inode* parent_inode, struct dentry* child_dentry) {
  const char* name = child_dentry->d_name.name;
  struct nifs_dir_entry* parent_dir = nifs_find_directory(parent_inode->i_ino);

  if (!parent_dir) {
    return -ENOENT;
  }

  struct nifs_dir_entry* dir = nifs_find_subdir(parent_dir, name);
  if (!dir) {
    return -ENOENT;
  }

  if (!list_empty(&dir->files) || !list_empty(&dir->subdirs)) {
    return -ENOTEMPTY;
  }

  list_del(&dir->parent_list);
  list_del(&dir->global_list);

  kfree(dir->name);
  kfree(dir);
  LOG("Removed directory: %s\n", name);
  return 0;
}
// ====== ============== ======

// ====== FILE DATA MANAGEMENT ======

static struct nifs_file_data* nifs_alloc_file_data(void) {
  struct nifs_file_data* fd = kmalloc(sizeof(struct nifs_file_data), GFP_KERNEL);
  if (!fd) {
    return NULL;
  }

  fd->size = 0;
  fd->capacity = 0;
  fd->data = NULL;
  return fd;
}

static int nifs_resize_file_data(struct nifs_file_data* fd, size_t new_size) {
  if (new_size <= fd->capacity) {
    fd->size = new_size;
    return 0;
  }

  size_t new_capacity = new_size;
  char* new_data = krealloc(fd->data, new_capacity, GFP_KERNEL);
  if (!new_data) {
    return -ENOMEM;
  }

  fd->data = new_data;
  fd->capacity = new_capacity;

  size_t old_size = fd->size;
  fd->size = new_size;

  if (new_size > old_size) {
    memset(fd->data + old_size, 0, new_size - old_size);
  }

  return 0;
}

static void nifs_free_file_data(struct nifs_file_data* fd) {
  if (fd) {
    kfree(fd->data);
    kfree(fd);
  }
}

// ====== ==================== ======

// ====== FILE OPERATIONS ======

static ssize_t nifs_read(struct file* filp, char __user* buffer, size_t len, loff_t* offset) {
  struct inode* inode = file_inode(filp);
  struct nifs_file_entry* entry;

  list_for_each_entry(entry, &nifs_files, global_list) {
    if (entry->inode_number == inode->i_ino) {
      if (*offset >= entry->data->size) {
        return 0;  // EOF
      }

      size_t to_read = min(len, entry->data->size - *offset);
      if (copy_to_user(buffer, entry->data->data + *offset, to_read)) {
        return -EFAULT;
      }

      *offset += to_read;
      return to_read;
    }
  }

  return -ENOENT;
}

static ssize_t nifs_write(
    struct file* filp, const char __user* buffer, size_t len, loff_t* offset
) {
  struct inode* inode = file_inode(filp);
  struct nifs_file_entry* entry;
  int ret;
  loff_t pos = *offset;

  list_for_each_entry(entry, &nifs_files, global_list) {
    if (entry->inode_number == inode->i_ino) {
      if (filp->f_flags & O_APPEND) {
        pos = entry->data->size;
      }

      size_t new_size = pos + len;

      ret = nifs_resize_file_data(entry->data, new_size);
      if (ret) {
        return ret;
      }

      if (copy_from_user(entry->data->data + pos, buffer, len)) {
        return -EFAULT;
      }

      *offset = pos + len;
      return (ssize_t)len;
    }
  }

  return -ENOENT;
}

// ====== =============== ======

static int nifs_iterate(struct file* filp, struct dir_context* ctx) {
  struct dentry* dentry = filp->f_path.dentry;
  struct inode* inode = dentry->d_inode;
  loff_t pos = ctx->pos;

  struct nifs_dir_entry* dir = nifs_find_directory(inode->i_ino);
  if (!dir) {
    return 0;
  }

  // "." entry
  if (pos == 0) {
    if (!dir_emit(ctx, ".", 1, inode->i_ino, DT_DIR)) {
      return 0;
    }
    ctx->pos = 1;
    return 0;
  }

  // ".." entry
  if (pos == 1) {
    struct dentry* parent = dentry->d_parent;
    struct inode* parent_inode = parent->d_inode;
    if (!dir_emit(ctx, "..", 2, parent_inode->i_ino, DT_DIR)) {
      return 0;
    }
    ctx->pos = 2;
    return 0;
  }

  // Count subdirectories first
  int subdir_count = 0;
  struct nifs_dir_entry* subdir;
  list_for_each_entry(subdir, &dir->subdirs, parent_list) {
    subdir_count++;
  }

  // Subdirectories
  int subdir_idx = 0;
  list_for_each_entry(subdir, &dir->subdirs, parent_list) {
    if (pos == 2 + subdir_idx) {
      if (!dir_emit(ctx, subdir->name, strlen(subdir->name), subdir->inode_number, DT_DIR)) {
        return 0;
      }
      ctx->pos++;
      return 0;
    }
    subdir_idx++;
  }

  // Files
  struct nifs_file_entry* file;
  int file_idx = 0;
  list_for_each_entry(file, &dir->files, parent_list) {
    if (pos == 2 + subdir_count + file_idx) {
      if (!dir_emit(ctx, file->name, strlen(file->name), file->inode_number, DT_REG)) {
        return 0;
      }
      ctx->pos++;
      return 0;
    }
    file_idx++;
  }

  // Nothing more to emit
  return 0;
}

static struct dentry* nifs_lookup(
    struct inode* parent_inode,  // родительская нода
    struct dentry* child_dentry,  // объект, к которому мы пытаемся получить доступ
    unsigned int flag  // неиспользуемое значение
) {
  LOG("LOOKUP!");
  const char* name = child_dentry->d_name.name;
  struct nifs_dir_entry* parent_dir = nifs_find_directory(parent_inode->i_ino);

  if (!parent_dir) {
    d_add(child_dentry, NULL);
    return NULL;
  }

  if (!strcmp(name, ".")) {
    struct inode* inode = igrab(parent_inode);
    d_add(child_dentry, inode);
    return NULL;
  }

  if (!strcmp(name, "..")) {
    struct nifs_dir_entry* grandparent = nifs_find_directory(parent_dir->parent_inode);
    if (grandparent) {
      struct inode* inode = ilookup(parent_inode->i_sb, grandparent->inode_number);
      if (inode) {
        d_add(child_dentry, inode);
        return NULL;
      }
    }
    d_add(child_dentry, igrab(parent_inode));
    return NULL;
  }

  struct nifs_dir_entry* subdir = nifs_find_subdir(parent_dir, name);
  if (subdir) {
    struct inode* inode =
        nifs_get_inode(parent_inode->i_sb, parent_inode, S_IFDIR, subdir->inode_number);
    if (inode) {
      d_add(child_dentry, inode);
      return NULL;
    }
  }

  struct nifs_file_entry* file = nifs_find_file_in_dir(parent_dir, name);
  if (file) {
    struct inode* inode =
        nifs_get_inode(parent_inode->i_sb, parent_inode, S_IFREG, file->inode_number);
    if (inode) {
      d_add(child_dentry, inode);
      return NULL;
    }
  }

  d_add(child_dentry, NULL);
  return NULL;
}

static int nifs_fill_super(struct super_block* sb, void* data, int silent) {
  struct nifs_dir_entry* root_dir = kmalloc(sizeof(struct nifs_dir_entry), GFP_KERNEL);
  if (!root_dir) {
    return -ENOMEM;
  }

  root_dir->name = kstrdup("", GFP_KERNEL);  // NO NAME FOR ROOT
  root_dir->inode_number = NIFS_ROOT_INODE;
  root_dir->parent_inode = 0;  // NO PARENT FOR ROOT (sad)
  INIT_LIST_HEAD(&root_dir->files);
  INIT_LIST_HEAD(&root_dir->subdirs);
  INIT_LIST_HEAD(&root_dir->parent_list);
  INIT_LIST_HEAD(&root_dir->global_list);

  list_add_tail(&root_dir->global_list, &nifs_directories);

  struct inode* inode = nifs_get_inode(sb, NULL, S_IFDIR, NIFS_ROOT_INODE);
  sb->s_root = d_make_root(inode);
  if (sb->s_root == NULL) {
    kfree(root_dir->name);
    kfree(root_dir);
    return -ENOMEM;
  }

  LOG("Root inode created: mode=%o, uid=%d, gid=%d\n",
      inode->i_mode,
      i_uid_read(inode),
      i_gid_read(inode));

  LOG("Root directory created\n");
  return 0;
}

static struct dentry* nifs_mount(
    struct file_system_type* fs_type, int flags, const char* token, void* data
) {
  struct dentry* ret = mount_nodev(fs_type, flags, data, nifs_fill_super);
  if (ret == NULL) {
    printk(KERN_ERR "Can't mount file system\n");
  } else {
    printk(KERN_INFO "Mounted successfully\n");
  }
  return ret;
}

static void nifs_kill_sb(struct super_block* sb) {
  struct nifs_dir_entry* dir;
  struct nifs_dir_entry* tmp_dir;

  list_for_each_entry_safe(dir, tmp_dir, &nifs_directories, global_list) {
    struct nifs_file_entry* file;
    struct nifs_file_entry* tmp_file;

    list_for_each_entry_safe(file, tmp_file, &dir->files, parent_list) {
      list_del(&file->parent_list);
      list_del(&file->global_list);
      nifs_free_file_data(file->data);
      kfree(file->name);
      kfree(file);
    }

    list_del(&dir->global_list);
    if (!list_empty(&dir->parent_list)) {
      list_del(&dir->parent_list);
    }
    kfree(dir->name);
    kfree(dir);
  }

  nifs_next_inode = NIFS_NEXT_INODE;
  LOG("nifs super block destroyed\n");
}

static int __init nifs_init(void) {
  LOG("NIFS joined the kernel\n");
  int err = register_filesystem(&nifs_fs_type);
  if (err == 0) {
    LOG("NIFS file system registered.\n");
    return 0;
  }
  LOG("Failed to register file system: %d\n", err);
  return err;
}

static void __exit nifs_exit(void) {
  int err = unregister_filesystem(&nifs_fs_type);
  if (err) {
    LOG("Failed to unregister file system: %d\n", err);
  } else {
    LOG("File system unregistered\n");
  }
  LOG("NIFS left the kernel\n");
}

struct file_system_type nifs_fs_type = {
    .name = "nifs",
    .mount = nifs_mount,
    .kill_sb = nifs_kill_sb,
};

module_init(nifs_init);  // NOLINT
module_exit(nifs_exit);  // NOLINT
