#include <pmm.h>
#include <list.h>
#include <string.h>
#include <default_pmm.h>

/*  In the First Fit algorithm, the allocator keeps a list of free blocks
 * (known as the free list). Once receiving a allocation request for memory,
 * it scans along the list for the first block that is large enough to satisfy
 * the request. If the chosen block is significantly larger than requested, it
 * is usually splitted, and the remainder will be added into the list as
 * another free block.
 *  Please refer to Page 196~198, Section 8.2 of Yan Wei Min's Chinese book
 * "Data Structure -- C programming language".
*/
// LAB2 EXERCISE 1: YOUR CODE
// you should rewrite functions: `default_init`, `default_init_memmap`,
// `default_alloc_pages`, `default_free_pages`.
/*
 * Details of FFMA
 * (1) Preparation:
 *  In order to implement the First-Fit Memory Allocation (FFMA), we should
 * manage the free memory blocks using a list. The struct `free_area_t` is used
 * for the management of free memory blocks.
 *  First, you should get familiar with the struct `list` in list.h. Struct
 * `list` is a simple doubly linked list implementation. You should know how to
 * USE `list_init`, `list_add`(`list_add_after`), `list_add_before`, `list_del`,
 * `list_next`, `list_prev`.
 *  There's a tricky method that is to transform a general `list` struct to a
 * special struct (such as struct `page`), using the following MACROs: `le2page`
 * (in memlayout.h), (and in future labs: `le2vma` (in vmm.h), `le2proc` (in
 * proc.h), etc).
 * (2) `default_init`:
 *  You can reuse the demo `default_init` function to initialize the `free_list`
 * and set `nr_free` to 0. `free_list` is used to record the free memory blocks.
 * `nr_free` is the total number of the free memory blocks.
 * (3) `default_init_memmap`:
 *  CALL GRAPH: `kern_init` --> `pmm_init` --> `page_init` --> `init_memmap` -->
 * `pmm_manager` --> `init_memmap`.
 *  This function is used to initialize a free block (with parameter `addr_base`,
 * `page_number`). In order to initialize a free block, firstly, you should
 * initialize each page (defined in memlayout.h) in this free block. This
 * procedure includes:

 *  - Setting the bit `PG_property` of `p->flags`, which means this page is
 * valid. P.S. In function `pmm_init` (in pmm.c), the bit `PG_reserved` of
 * `p->flags` is already set.
 *  - If this page is free and is not the first page of a free block,
 * `p->property` should be set to 0.
 *  - If this page is free and is the first page of a free block, `p->property`
 * should be set to be the total number of pages in the block.
 *  - `p->ref` should be 0, because now `p` is free and has no reference.
 *  After that, We can use `p->page_link` to link this page into `free_list`.
 * (e.g.: `list_add_before(&free_list, &(p->page_link));` )
 *  Finally, we should update the sum of the free memory blocks: `nr_free += n`.

 * (4) `default_alloc_pages`:
 *  首先去链表当中, 找到第一个空闲的分区.
 *  Page struct 里面定义了, reserved 为0 , 就是没有被占用, 第一个, 就是我们想要的, 所以我们要从  pages 这个数组里面去
 *  取出自己想要的大小.
 *  然后, 就把这个区域, 从 freemem 当中 unlink 出来.
 *  Page 这个 struct 有一个 trick, 就是放在一个列表当中, 然后专门配备了一个字段, 叫做 Priority , 如果 >0 , 代表了后面的空闲page 的数量.
 *  相当于领头 page 的感觉. 代表局部 page 数量的.
 *  而 nr_free 则像一个 global 的 page 数量.
 *  Search for the first free block (block size >= n) in the free list and reszie
 * the block found, returning the address of this block as the address required by
 * `malloc`.
 *  (4.1)
 *      So you should search the free list like this:
 *          list_entry_t le = &free_list;
 *          while((le=list_next(le)) != &free_list) {
 *          ...
 *      (4.1.1)
 *          In the while loop, get the struct `page` and check if `p->property`
 *      (recording the num of free pages in this block) >= n.
 *              struct Page *p = le2page(le, page_link);
 *              if(p->property >= n){ ...
 *      (4.1.2)
 *          If we find this `p`, it means we've found a free block with its size
 *      >= n, whose first `n` pages can be malloced. Some flag bits of this page
 *      should be set as the following: `PG_reserved = 1`, `PG_property = 0`.
 *      Then, unlink the pages from `free_list`.
 *          (4.1.2.1)
 *              If `p->property > n`, we should re-calculate number of the rest
 *          pages of this free block. (e.g.: `le2page(le,page_link))->property
 *          = p->property - n;`)
 *          (4.1.3)
 *              Re-caluclate `nr_free` (number of the the rest of all free block).
 *          (4.1.4)
 *              return `p`.
 *      (4.2)
 *          If we can not find a free block with its size >=n, then return NULL.
 * (5) `default_free_pages`:
 *  re-link the pages into the free list, and may merge small free blocks into
 * the big ones.
 *  (5.1)
 *      According to the base address of the withdrawed blocks, search the free
 *  list for its correct position (with address from low to high), and insert
 *  the pages. (May use `list_next`, `le2page`, `list_add_before`)
 *  (5.2)
 *      Reset the fields of the pages, such as `p->ref` and `p->flags` (PageProperty)
 *  (5.3)
 *      Try to merge blocks at lower or higher addresses. Notice: This should
 *  change some pages' `p->property` correctly.
 */
free_area_t free_area;

#define free_list (free_area.free_list) // the list header
#define nr_free (free_area.nr_free) // free pages in this free list

static void
default_init(void) {
    list_init(&free_list);
    //  所以这个地方, nr_free 应该是不为零, 应该设置为 page 的数量?
    nr_free = 0;
    // 这个 free_list 还没有指向到 page. 所以还和 free_page 之间建立连接.
}

// 这个  function 在 pmm_init 的时候会被调用.
// 15mb 的起始位置, 以及 page 的数量.
static void
default_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);
    struct Page *p = base;
    for (; p != base + n; p ++) {
        assert(PageReserved(p));
        p->flags = p->property = 0;
        set_page_ref(p, 0);
    }
    base->property = n;
    // page property 记录了
    SetPageProperty(base);
    // 修改 free 的 page 的数量.
    nr_free += n;
    // 链表的正常添加操作.
    /* *
     * result 里面, 使用的是 list_add_before. list_add 默认使用的是 list_add_after
     * 为什么?
     * free_list -> page_link -> free_list.next
     * list_add(&free_list, &(base->page_link));
     * free_list.prev -> page_link -> free_list
     * 这个时候, 链表里面只有一个节点, 也就是 page_link 这个地址, 也就是 15mb.
     * 如果需要申请, 就是看看申请的大小是多少, 解析成对应的页面数量, 然后修改 page 的 struct
     * 然后把这个再放入到链表当中.
    * */
    list_add_before(&free_list, &(base->page_link));
    // 这个地方, 实际上就是把初始化出来的链表, 与页的 array 进行关联.
    /*
    assert(n > 0);
    struct Page *p = base;
    for (; p != base + n; p ++) {
        assert(PageReserved(p));
        p->flags = p->property = 0;
        set_page_ref(p, 0);
    }
    base->property = n;
    SetPageProperty(base);
    nr_free += n;
    list_add_before(&free_list, &(base->page_link));
    */

}

static struct Page *
buddy_alloc_pages(size_t n) {
    assert (n>0);
    if (n>nr_free) {
        return NULL;
    }
    // 修改 size_t n 为 2 的 n 次方.

    // 遍历链表, 把链表当中的 Page 遍历一遍, 看看能否找到需要的内存块.
    // 如果找不到;
}
static struct Page *
default_alloc_pages(size_t n) {
    assert(n > 0);
    if (n > nr_free) {
        return NULL;
    }
    struct Page *page = NULL;
    list_entry_t *le = &free_list;
    // 这个地方, 就开始把物理地址, 链表地址记录的page, 开始遍历, 给到需要 allocate 的进程.
    int count = 0;
    while ((le = list_next(le)) != &free_list) {
        count++;
        struct Page *p = le2page(le, page_link);
        if (p->property >= n) {
            // 如果这个页够 size 的话, 执行下面的
            //cprintf("page property: %d\n", p->property);
            page = p;
            break;
        }
    }
    // unlink allocate 到的 page. 应该根据空间进行 unlink
    //cprintf("allocated page property: %d\n", page->property);
    //cprintf("allocated page address: %x\n", page);
    if (page != NULL) {
        if (page->property > n) {
            struct Page *q = page + n;
            q->property = page->property - n;
            SetPageProperty(q);
            //cprintf("get property after allocated: %d\n", q->property);
            //cprintf("get flag after allocated: %d\n", q->flags);
            list_add_after(&(page->page_link), &(q->page_link));
        }
        // 不理解了， 这个地方不应该还再创建一个新的节点，用来放空闲的区域么？
        // 看起来是，申请的区域直接返回回去，链表里面，仅仅保存了空闲的。
        list_del(&(page->page_link));
        nr_free -= n;
        //cprintf("nr_free is: %d\n", nr_free);
        ClearPageProperty(page);
    }
    return page;
}

static void
default_free_pages(struct Page *base, size_t n) {
    assert(n > 0);
    struct Page *p = base;
    for (; p != base + n; p ++) {
        assert(!PageReserved(p) && !PageProperty(p));
        p->flags = 0;
        set_page_ref(p, 0);
    }
    base->property = n;
    SetPageProperty(base);
    // :upon:, 就完成了需要 free 页面的初始化工作.
    // :below:, 需要把这些初始化的 page 放到链表当中管理起来;
    list_entry_t *le = list_next(&free_list);
    while (le != &free_list) {
        // 这个地方如何理解? 应该是说, 遍历链表的每一项.
        // 找到检查需要释放的 page 在 free_list 的隔壁.
        // 有两种情况:
        //  第一种就是 base 在释放的前面, 进入第一个 if.需要执行的事情是, 把 base 和 page 组合起来,
        //        base 作为起始地址, 加入链表.
        //  第二种是 base 在遍历的 page 后面, 进入第二个 if. 需要做的事情是, 把 page 和 base 组合起来,
        //        page 作为起始地址, 加入链表.
        p = le2page(le, page_link);
        le = list_next(le);
        if (base + base->property == p) {
            base->property += p->property;
            ClearPageProperty(p);
            list_del(&(p->page_link));
        }
        else if (p + p->property == base) {
            p->property += base->property;
            ClearPageProperty(base);
            base = p;
            list_del(&(p->page_link));
        }
    }
    nr_free += n;

    // 再一次遍历链表, 在链表当中找到合适的位置来存放, 确保顺序.
    le = list_next(&free_list);
    while(le != &free_list) {
        p = le2page(le, page_link);
        if (base + base->property <= p) {
            assert(base + base->property != p);
            break;
        }
        le = list_next(le);
    }
    // 把我们释放出来的 page ,放在我们前面得到的位置前面, 因为 <=
    list_add_before(le, &(base->page_link));

}

static size_t
default_nr_free_pages(void) {
    return nr_free;
}

static void
basic_check(void) {
    struct Page *p0, *p1, *p2;
    p0 = p1 = p2 = NULL;
    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);

    assert(p0 != p1 && p0 != p2 && p1 != p2);
    assert(page_ref(p0) == 0 && page_ref(p1) == 0 && page_ref(p2) == 0);

    assert(page2pa(p0) < npage * PGSIZE);
    assert(page2pa(p1) < npage * PGSIZE);
    assert(page2pa(p2) < npage * PGSIZE);

    list_entry_t free_list_store = free_list;
    list_init(&free_list);
    assert(list_empty(&free_list));

    unsigned int nr_free_store = nr_free;
    nr_free = 0;

    assert(alloc_page() == NULL);

    free_page(p0);
    free_page(p1);
    free_page(p2);
    assert(nr_free == 3);

    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);

    assert(alloc_page() == NULL);

    free_page(p0);
    assert(!list_empty(&free_list));

    struct Page *p;
    assert((p = alloc_page()) == p0);
    assert(alloc_page() == NULL);

    assert(nr_free == 0);
    free_list = free_list_store;
    nr_free = nr_free_store;

    free_page(p);
    free_page(p1);
    free_page(p2);
}

// LAB2: below code is used to check the first fit allocation algorithm (your EXERCISE 1) 
// NOTICE: You SHOULD NOT CHANGE basic_check, default_check functions!
static void
default_check(void) {
    int count = 0, total = 0;
    list_entry_t *le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        assert(PageProperty(p));
        count ++, total += p->property;
    }
    assert(total == nr_free_pages());

    basic_check();

    struct Page *p0 = alloc_pages(5), *p1, *p2;
    assert(p0 != NULL);
    assert(!PageProperty(p0));

    list_entry_t free_list_store = free_list;
    list_init(&free_list);
    assert(list_empty(&free_list));
    assert(alloc_page() == NULL);

    unsigned int nr_free_store = nr_free;
    nr_free = 0;

    free_pages(p0 + 2, 3);
    assert(alloc_pages(4) == NULL);
    assert(PageProperty(p0 + 2) && p0[2].property == 3);
    assert((p1 = alloc_pages(3)) != NULL);
    assert(alloc_page() == NULL);
    assert(p0 + 2 == p1);

    p2 = p0 + 1;
    free_page(p0);
    free_pages(p1, 3);
    assert(PageProperty(p0) && p0->property == 1);
    assert(PageProperty(p1) && p1->property == 3);

    assert((p0 = alloc_page()) == p2 - 1);
    free_page(p0);
    assert((p0 = alloc_pages(2)) == p2 + 1);

    free_pages(p0, 2);
    free_page(p2);

    assert((p0 = alloc_pages(5)) != NULL);
    assert(alloc_page() == NULL);

    assert(nr_free == 0);
    nr_free = nr_free_store;

    free_list = free_list_store;
    free_pages(p0, 5);

    le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        assert(le->next->prev == le && le->prev->next == le);
        struct Page *p = le2page(le, page_link);
        count --, total -= p->property;
    }
    assert(count == 0);
    assert(total == 0);
}

const struct pmm_manager default_pmm_manager = {
    .name = "default_pmm_manager",
    .init = default_init,
    .init_memmap = default_init_memmap,
    .alloc_pages = default_alloc_pages,
    .free_pages = default_free_pages,
    .nr_free_pages = default_nr_free_pages,
    .check = default_check,
};

