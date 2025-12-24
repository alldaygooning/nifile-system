#include "nifs.h"

#include <linux/printk.h>

// ====== CREATED FILES TRACKING ======

struct nifs_file_entry {
  char* name;             // File name
  int inode_number;       // Inode number
  struct list_head list;  // Linux kernel list node
};
static LIST_HEAD(nifs_file_list);
static int nifs_next_inode = NIFS_NEXT_INODE;

// ====== ====================== ======

static struct dentry* nifs_lookup(
    struct inode* parent_inode, struct dentry* child_dentry, unsigned int flag
);

static int nifs_create(struct mnt_idmap*, struct inode*, struct dentry*, umode_t, bool);

static int nifs_unlink(struct inode* parent_inode, struct dentry* child_dentry);

static int nifs_iterate(struct file* filp, struct dir_context* ctx);

struct inode_operations nifs_inode_ops = {
    .lookup = nifs_lookup,
    .create = nifs_create,
    .unlink = nifs_unlink,
};

static const struct file_operations nifs_dir_operations = {
    .owner = THIS_MODULE,
    .iterate_shared = nifs_iterate,
};

static struct inode* nifs_get_inode(
    struct super_block* sb, const struct inode* dir, umode_t mode, int i_ino
) {
  struct inode* inode = new_inode(sb);
  if (inode != NULL) {
    inode_init_owner(&nop_mnt_idmap, inode, dir, mode);
    inode->i_mode = mode | (S_IRWXU | S_IRWXG | S_IRWXO);
  }

  inode->i_ino = i_ino;
  if (S_ISDIR(mode)) {
    inode->i_op = &nifs_inode_ops;
    inode->i_fop = &nifs_dir_operations;
  }
  return inode;
}

// ====== FILE MANAGEMENT =======

static int nifs_create(
    struct mnt_idmap* idmap,
    struct inode* parent_inode,
    struct dentry* child_dentry,
    umode_t mode,
    bool b
) {
  ino_t root = parent_inode->i_ino;
  const char* name = child_dentry->d_name.name;

  // Prevent file creation outside of root directory
  if (root != NIFS_ROOT_INODE) {
    return -EPERM;
  }

  // Prevent duplicate creation of "test.txt"
  if (!strcmp(name, NIFS_TESTFILE_NAME)) {
    return 0;
  }

  struct nifs_file_entry* file_entry;
  list_for_each_entry(file_entry, &nifs_file_list, list) {
    if (!strcmp(name, file_entry->name)) {
      return 0;  // File already exists!
    }
  }

  // Allocate memory for new file entry
  struct nifs_file_entry* new_entry = kmalloc(sizeof(struct nifs_file_entry), GFP_KERNEL);
  if (!new_entry) {
    return -ENOMEM;
  }

  new_entry->name = kmalloc(strlen(name) + 1, GFP_KERNEL /*GET FREE PAGES*/);
  if (!new_entry->name) {
    kfree(new_entry);
    return -ENOMEM;
  }
  strcpy(new_entry->name, name);

  new_entry->inode_number = nifs_next_inode++;
  INIT_LIST_HEAD(&new_entry->list);
  list_add_tail(&new_entry->list, &nifs_file_list);

  struct inode* inode =
      nifs_get_inode(parent_inode->i_sb, NULL, S_IFREG | S_IRWXUGO, new_entry->inode_number);
  if (!inode) {
    // Clean up on error!!!
    list_del(&new_entry->list);
    kfree(new_entry->name);
    kfree(new_entry);
    return -ENOMEM;
  }

  inode->i_op = &nifs_inode_ops;
  inode->i_fop = NULL;

  d_add(child_dentry, inode);
  LOG("Created file: %s (inode %d)\n", name, new_entry->inode_number);
  return 0;
}

static int nifs_unlink(struct inode* parent_inode, struct dentry* child_dentry) {
  const char* name = child_dentry->d_name.name;
  ino_t root = parent_inode->i_ino;

  // Disable unlinking outside root directory
  if (root != NIFS_ROOT_INODE) {
    return -EPERM;
  }

  // Don't allow removing "test.txt"
  if (!strcmp(name, NIFS_TESTFILE_NAME)) {
    return -EPERM;
  }

  // Find and remove the file entry from tracking list
  struct nifs_file_entry* file_entry;
  struct nifs_file_entry* tmp;
  list_for_each_entry_safe(file_entry, tmp, &nifs_file_list, list) {
    if (!strcmp(name, file_entry->name)) {
      LOG("Removing file: %s (inode %d)\n", name, file_entry->inode_number);
      list_del(&file_entry->list);
      kfree(file_entry->name);
      kfree(file_entry);
      return 0;
    }
  }

  // File not found :(
  return -ENOENT;
}

// ====== =============== =======

static int nifs_iterate(struct file* filp, struct dir_context* ctx) {
  struct dentry* dentry = filp->f_path.dentry;
  struct inode* inode = dentry->d_inode;
  loff_t pos = ctx->pos;
  struct nifs_file_entry* file_entry;

  // "." entry
  if (pos == 0) {
    if (!dir_emit(ctx, NIFS_DOT_ENTRY, 1, inode->i_ino, DT_DIR)) {
      return 0;
    }
    ctx->pos = 1;
    pos = 1;
  }

  // ".." entry
  if (pos == 1) {
    struct dentry* parent = dentry->d_parent;
    struct inode* parent_inode = parent->d_inode;
    if (!dir_emit(ctx, NIFS_DOTDOT_ENTRY, 2, parent_inode->i_ino, DT_DIR)) {
      return 0;
    }
    ctx->pos = 2;
    pos = 2;
  }

  // "test.txt" entry
  if (pos == 2) {
    if (!dir_emit(
            ctx, NIFS_TESTFILE_NAME, strlen(NIFS_TESTFILE_NAME), NIFS_TESTFILE_INODE, DT_REG
        )) {
      return 0;
    }
    ctx->pos = 3;
    pos = 3;
  }

  // List all created files
  list_for_each_entry(file_entry, &nifs_file_list, list) {
    if (pos == 3) {
      if (!dir_emit(
              ctx, file_entry->name, strlen(file_entry->name), file_entry->inode_number, DT_REG
          )) {
        return 0;
      }
      ctx->pos = 4;
      pos = 4;
    }
  }

  return 0;
}

static struct dentry* nifs_lookup(
    struct inode* parent_inode,  // родительская нода
    struct dentry* child_dentry,  // объект, к которому мы пытаемся получить доступ
    unsigned int flag  // неиспользуемое значение
) {
  ino_t root = parent_inode->i_ino;
  const char* name = child_dentry->d_name.name;
  struct nifs_file_entry* file_entry;

  if (root == NIFS_ROOT_INODE) {
    if (!strcmp(name, NIFS_TESTFILE_NAME)) {
      struct inode* inode = nifs_get_inode(parent_inode->i_sb, NULL, S_IFREG, NIFS_TESTFILE_INODE);
      d_add(child_dentry, inode);
      return NULL;
    }

    if (!strcmp(name, NIFS_DIR_NAME)) {
      struct inode* inode = nifs_get_inode(parent_inode->i_sb, NULL, S_IFDIR, NIFS_DIR_INODE);
      d_add(child_dentry, inode);
      return NULL;
    }

    list_for_each_entry(file_entry, &nifs_file_list, list) {
      if (!strcmp(name, file_entry->name)) {
        struct inode* inode =
            nifs_get_inode(parent_inode->i_sb, NULL, S_IFREG, file_entry->inode_number);
        d_add(child_dentry, inode);
        return NULL;
      }
    }

    d_add(child_dentry, NULL);
  } else {
    d_add(child_dentry, NULL);
  }
  return NULL;
}

static int nifs_fill_super(struct super_block* sb, void* data, int silent) {
  struct inode* inode = nifs_get_inode(sb, NULL, S_IFDIR, NIFS_ROOT_INODE);

  sb->s_root = d_make_root(inode);
  if (sb->s_root == NULL) {
    return -ENOMEM;
  }

  printk(KERN_INFO "return 0\n");
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
  struct nifs_file_entry* file_entry;
  struct nifs_file_entry* tmp;

  list_for_each_entry_safe(file_entry, tmp, &nifs_file_list, list) {
    list_del(&file_entry->list);
    kfree(file_entry->name);
    kfree(file_entry);
  }
  nifs_next_inode = NIFS_NEXT_INODE;

  printk(KERN_INFO "nifs super block is destroyed. Unmount successfully.\n");
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