#include "nifs.h"

#include <linux/printk.h>

static int __init nifs_init(void) {
  LOG("NIFS joined the kernel\n");
  return 0;
}

static void __exit nifs_exit(void) {
  LOG("NIFS left the kernel\n");
}

module_init(nifs_init);
module_exit(nifs_exit);
