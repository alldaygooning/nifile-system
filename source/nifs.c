#include "nifs.h"

#include <linux/dcache.h>
#include <linux/fs.h>
#include <linux/printk.h>

static int nifs_iterate(struct file* filp, struct dir_context* ctx) {
  char name[16];
  struct dentry* dentry = filp->f_path.dentry;
  struct inode* inode = dentry->d_inode;
  loff_t pos = ctx->pos;

  if (pos == 0) {
    if (!dir_emit(ctx, ".", 1, inode->i_ino, DT_DIR)) {
      return 0;
    }
    ctx->pos = 1;
    pos = 1;
  }

  if (pos == 1) {
    struct dentry* parent = dentry->d_parent;
    struct inode* parent_inode = parent->d_inode;
    if (!dir_emit(ctx, "..", 2, parent_inode->i_ino, DT_DIR)) {
      return 0;
    }
    ctx->pos = 2;
    pos = 2;
  }

  if (pos == 2) {
    strcpy(name, "test.txt");
    if (!dir_emit(ctx, name, strlen(name), 101, DT_REG)) {
      return 0;
    }
    ctx->pos = 3;
  }

  return 0;
}

static const struct file_operations nifs_dir_operations = {
    .owner = THIS_MODULE,
    .iterate_shared = nifs_iterate,
};

static struct dentry* nifs_lookup(
    struct inode* parent_inode,  // родительская нода
    struct dentry* child_dentry,  // объект, к которому мы пытаемся получить доступ
    unsigned int flag  // неиспользуемое значение
) {
  d_add(child_dentry, NULL);
  return NULL;
}

struct inode_operations nifs_inode_ops = {
    .lookup = nifs_lookup,
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

static int nifs_fill_super(struct super_block* sb, void* data, int silent) {
  struct inode* inode = nifs_get_inode(sb, NULL, S_IFDIR, 1000);

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
