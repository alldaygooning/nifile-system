#include "nifs.h"

#include <linux/printk.h>

static struct inode* nifs_get_inode(
    struct super_block* sb, const struct inode* dir, umode_t mode, int i_ino
) {
  struct inode* inode = new_inode(sb);
  if (inode != NULL) {
    inode_init_owner(&nop_mnt_idmap, inode, dir, mode);
  }

  inode->i_ino = i_ino;
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
    printk(KERN_INFO "Mounted successfuly\n");
  }
  return ret;
}

static void nifs_kill_sb(struct super_block* sb) {
  printk(KERN_INFO "nifs super block is destroyed. Unmount successfully.\n");
}

struct file_system_type nifs_fs_type = {
    .name = "nifs",
    .mount = nifs_mount,
    .kill_sb = nifs_kill_sb,
};

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

module_init(nifs_init);  // NOLINT
module_exit(nifs_exit);  // NOLINT
