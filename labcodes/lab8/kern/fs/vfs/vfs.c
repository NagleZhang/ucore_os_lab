#include <defs.h>
#include <stdio.h>
#include <string.h>
#include <vfs.h>
#include <inode.h>
#include <sem.h>
#include <kmalloc.h>
#include <error.h>

/*
 * step1:
 * 开始阅读这一章最主要的问题就是. 这个东西的位置是什么?
 * 在 proc 的过程当中, 每一个 proc 都会有相应的 file.
 * 然后进行 file create.
 * 然后 file create 的时候, 存在 fs, file, vfs, sfs
 * 这些的区别是什么? 调用链是如何工作的?
 * 这个是首先要去进行阅读的.
 *
 * 而,最主要的, 就是 iload 的这个 function到底是如何工作的, 我们把它补齐.
 */

static semaphore_t bootfs_sem;
static struct inode *bootfs_node = NULL;

extern void vfs_devlist_init(void);

// __alloc_fs - allocate memory for fs, and set fs type
struct fs *
__alloc_fs(int type) {
    struct fs *fs;
    if ((fs = kmalloc(sizeof(struct fs))) != NULL) {
        fs->fs_type = type;
    }
    return fs;
}

// vfs_init -  vfs initialize
void
vfs_init(void) {
    sem_init(&bootfs_sem, 1);
    vfs_devlist_init();
}

// lock_bootfs - lock  for bootfs
static void
lock_bootfs(void) {
    down(&bootfs_sem);
}
// ulock_bootfs - ulock for bootfs
static void
unlock_bootfs(void) {
    up(&bootfs_sem);
}

// change_bootfs - set the new fs inode 
static void
change_bootfs(struct inode *node) {
    struct inode *old;
    lock_bootfs();
    {
        old = bootfs_node, bootfs_node = node;
    }
    unlock_bootfs();
    if (old != NULL) {
        vop_ref_dec(old);
    }
}

// vfs_set_bootfs - change the dir of file system
int
vfs_set_bootfs(char *fsname) {
    struct inode *node = NULL;
    if (fsname != NULL) {
        char *s;
        if ((s = strchr(fsname, ':')) == NULL || s[1] != '\0') {
            return -E_INVAL;
        }
        int ret;
        // vfs_lookup: Set current directory, as a pathname. Use vfs_lookup to translate
        // it to a inode.
        if ((ret = vfs_chdir(fsname)) != 0) {
            return ret;
        }
        if ((ret = vfs_get_curdir(&node)) != 0) {
            return ret;
        }
    }
    change_bootfs(node);
    return 0;
}

// vfs_get_bootfs - get the inode of bootfs
int
vfs_get_bootfs(struct inode **node_store) {
    struct inode *node = NULL;
    if (bootfs_node != NULL) {
        lock_bootfs();
        {
            if ((node = bootfs_node) != NULL) {
                vop_ref_inc(bootfs_node);
            }
        }
        unlock_bootfs();
    }
    if (node == NULL) {
        return -E_NOENT;
    }
    *node_store = node;
    return 0;
}

