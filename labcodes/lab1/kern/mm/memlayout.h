#ifndef __KERN_MM_MEMLAYOUT_H__
#define __KERN_MM_MEMLAYOUT_H__

/* This file contains the definitions for memory management in our OS. */

/* global segment number */
#define SEG_KTEXT    1
#define SEG_KDATA    2
#define SEG_UTEXT    3
#define SEG_UDATA    4
#define SEG_TSS        5

/* global descriptor numbers */
#define GD_KTEXT    ((SEG_KTEXT) << 3)        // kernel text 8
#define GD_KDATA    ((SEG_KDATA) << 3)        // kernel data 16
#define GD_UTEXT    ((SEG_UTEXT) << 3)        // user text 24
#define GD_UDATA    ((SEG_UDATA) << 3)        // user data 32
#define GD_TSS        ((SEG_TSS) << 3)        // task segment selector 40

#define DPL_KERNEL    (0)
#define DPL_USER    (3)

// '|' binary or operator
#define KERNEL_CS    ((GD_KTEXT) | DPL_KERNEL)
#define KERNEL_DS    ((GD_KDATA) | DPL_KERNEL)
#define USER_CS        ((GD_UTEXT) | DPL_USER)
#define USER_DS        ((GD_UDATA) | DPL_USER)

#endif /* !__KERN_MM_MEMLAYOUT_H__ */

