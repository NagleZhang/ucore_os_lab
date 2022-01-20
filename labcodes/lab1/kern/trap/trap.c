#include <defs.h>
#include <mmu.h>
#include <memlayout.h>
#include <clock.h>
#include <trap.h>
#include <x86.h>
#include <stdio.h>
#include <assert.h>
#include <console.h>
#include <kdebug.h>

#define TICK_NUM 1000

static void print_ticks() {
    cprintf("%d ticks\n",TICK_NUM);
#ifdef DEBUG_GRADE
    cprintf("End of Test.\n");
    panic("EOT: kernel seems ok.");
#endif
}

/* *
 * Interrupt descriptor table:
 *
 * Must be built at run time because shifted function addresses can't
 * be represented in relocation records.
 * */
static struct gatedesc idt[256] = {{0}};

static struct pseudodesc idt_pd = {
    sizeof(idt) - 1, (uintptr_t)idt
};

/* idt_init - initialize IDT to each of the entry points in kern/trap/vectors.S */
void
idt_init(void) {
     /* LAB1 YOUR CODE : STEP 2 */
     /* (1) Where are the entry addrs of each Interrupt Service Routine (ISR)?
      *     All ISR's entry addrs are stored in __vectors. where is uintptr_t __vectors[] ?
      *     __vectors[] is in kern/trap/vector.S which is produced by tools/vector.c
      *     (try "make" command in lab1, then you will find vector.S in kern/trap DIR)
      *     You can use  "extern uintptr_t __vectors[];" to define this extern variable which will be used later.
      * (2) Now you should setup the entries of ISR in Interrupt Description Table (IDT).
      *     Can you see idt[256] in this file? Yes, it's IDT! you can use SETGATE macro to setup each item of IDT
      * (3) After setup the contents of IDT, you will let CPU know where is the IDT by using 'lidt' instruction.
      *     You don't know the meaning of this instruction? just google it! and check the libs/x86.h to know more.
      *     Notice: the argument of lidt is idt_pd. try to find it!
      */
    /*
      struct gatedesc {
      unsigned gd_off_15_0 : 16;        // low 16 bits of offset in segment
      unsigned gd_ss : 16;            // segment selector
      unsigned gd_args : 5;            // # args, 0 for interrupt/trap gates
      unsigned gd_rsv1 : 3;            // reserved(should be zero I guess)
      unsigned gd_type : 4;            // type(STS_{TG,IG32,TG32})
      unsigned gd_s : 1;                // must be 0 (system)
      unsigned gd_dpl : 2;            // descriptor(meaning new) privilege level
      unsigned gd_p : 1;                // Present
      unsigned gd_off_31_16 : 16;        // high bits of offset in segment
      };

      * Set up a normal interrupt/trap gate descriptor
      *   - istrap: 1 for a trap (exception) gate, 0 for an interrupt gate
      *   - sel: Code segment selector for interrupt/trap handler
      *   - off: Offset in code segment for interrupt/trap handler
      *   - dpl: Descriptor Privilege Level - the privilege level required
      *          for software to invoke this interrupt/trap gate explicitly
      *          using an int instruction.
     */

    /* 应该是把 idt 的 256 个空间填充上内容。
     * vectors.s 里面， 有一个 global __alltrap, 里面会 call trap， 也就是本文件当中定义的 trap。
     * trap 又会 call trap_dispatch, 进而有具体的服务例程的实现。
     */

    extern uintptr_t __vectors[];
    for (int i = 0; i < 256; i++ ) {
        SETGATE(idt[i],       // gate , trap 会通过此门， 然后根据门内信息决定中断处理例程, 也就是 vector 里面对应的处理例程。
                0,            // 这个应该是用来处理 trap， 而非 exception 的。
                GD_KTEXT,     // 既然是 trap， 那么 idt 的代码应该是属于内核， 所以 code 的位置应该也是属于内核段。
                __vectors[i], // 这个内容会放到寄存器的后面 16 位， 从而根据这个 16 位， 加上 gdt 里面的 idt 的地址，确定执行哪一个例程
                DPL_KERNEL    // 与代码段的理解同理， 这个也应该是属于 kernel 态权限。
                );
    }

    // SETGATE(idt[T_SWITCH_TOK], 0,GD_UTEXT , __vectors[T_SWITCH_TOK], DPL_USER);

    // 告诉寄存器一声， 我好了；
    lidt(&idt_pd);
}

static const char *
trapname(int trapno) {
    static const char * const excnames[] = {
        "Divide error",
        "Debug",
        "Non-Maskable Interrupt",
        "Breakpoint",
        "Overflow",
        "BOUND Range Exceeded",
        "Invalid Opcode",
        "Device Not Available",
        "Double Fault",
        "Coprocessor Segment Overrun",
        "Invalid TSS",
        "Segment Not Present",
        "Stack Fault",
        "General Protection",
        "Page Fault",
        "(unknown trap)",
        "x87 FPU Floating-Point Error",
        "Alignment Check",
        "Machine-Check",
        "SIMD Floating-Point Exception"
    };

    if (trapno < sizeof(excnames)/sizeof(const char * const)) {
        return excnames[trapno];
    }
    if (trapno >= IRQ_OFFSET && trapno < IRQ_OFFSET + 16) {
        return "Hardware Interrupt";
    }
    return "(unknown trap)";
}

/* trap_in_kernel - test if trap happened in kernel */
bool
trap_in_kernel(struct trapframe *tf) {
    return (tf->tf_cs == (uint16_t)KERNEL_CS);
}

static const char *IA32flags[] = {
    "CF", NULL, "PF", NULL, "AF", NULL, "ZF", "SF",
    "TF", "IF", "DF", "OF", NULL, NULL, "NT", NULL,
    "RF", "VM", "AC", "VIF", "VIP", "ID", NULL, NULL,
};

void
print_trapframe(struct trapframe *tf) {
    cprintf("trapframe at %p\n", tf);
    print_regs(&tf->tf_regs);
    cprintf("  ds   0x----%04x\n", tf->tf_ds);
    cprintf("  es   0x----%04x\n", tf->tf_es);
    cprintf("  fs   0x----%04x\n", tf->tf_fs);
    cprintf("  gs   0x----%04x\n", tf->tf_gs);
    cprintf("  trap 0x%08x %s\n", tf->tf_trapno, trapname(tf->tf_trapno));
    cprintf("  err  0x%08x\n", tf->tf_err);
    cprintf("  eip  0x%08x\n", tf->tf_eip);
    cprintf("  cs   0x----%04x\n", tf->tf_cs);
    cprintf("  flag 0x%08x ", tf->tf_eflags);

    int i, j;
    for (i = 0, j = 1; i < sizeof(IA32flags) / sizeof(IA32flags[0]); i ++, j <<= 1) {
        if ((tf->tf_eflags & j) && IA32flags[i] != NULL) {
            cprintf("%s,", IA32flags[i]);
        }
    }
    cprintf("IOPL=%d\n", (tf->tf_eflags & FL_IOPL_MASK) >> 12);

    if (!trap_in_kernel(tf)) {
        cprintf("  esp  0x%08x\n", tf->tf_esp);
        cprintf("  ss   0x----%04x\n", tf->tf_ss);
    }
}

void
print_regs(struct pushregs *regs) {
    cprintf("  edi  0x%08x\n", regs->reg_edi);
    cprintf("  esi  0x%08x\n", regs->reg_esi);
    cprintf("  ebp  0x%08x\n", regs->reg_ebp);
    cprintf("  oesp 0x%08x\n", regs->reg_oesp);
    cprintf("  ebx  0x%08x\n", regs->reg_ebx);
    cprintf("  edx  0x%08x\n", regs->reg_edx);
    cprintf("  ecx  0x%08x\n", regs->reg_ecx);
    cprintf("  eax  0x%08x\n", regs->reg_eax);
}

/* trap_dispatch - dispatch based on what type of trap occurred */
static void
trap_dispatch(struct trapframe *tf) {
    char c;

    switch (tf->tf_trapno) {
    case IRQ_OFFSET + IRQ_TIMER:
        /* LAB1 YOUR CODE : STEP 3 */
        /* handle the timer interrupt */
        /* (1) After a timer interrupt, you should record this event using a global variable (increase it), such as ticks in kern/driver/clock.c
         * (2) Every TICK_NUM cycle, you can print some info using a funciton, such as print_ticks().
         * (3) Too Simple? Yes, I think so!
         */
        ticks++;
        if (ticks % TICK_NUM == 0 ) {
            print_ticks();
        }
        break;
    case IRQ_OFFSET + IRQ_COM1:
        c = cons_getc();
        cprintf("serial [%03d] %c\n", c, c);
        break;
    case IRQ_OFFSET + IRQ_KBD:
        c = cons_getc();
        cprintf("kbd [%03d] %c\n", c, c);
        break;
    //LAB1 CHALLENGE 1 : YOUR CODE you should modify below codes.
    case T_SWITCH_TOU:
      /*
        struct pushregs {
            uint32_t reg_edi;
            uint32_t reg_esi;
            uint32_t reg_ebp;
            uint32_t reg_oesp;
            uint32_t reg_ebx;
            uint32_t reg_edx;
            uint32_t reg_ecx;
            uint32_t reg_eax;
        };
        struct trapframe {
          struct pushregs tf_regs;
          uint16_t tf_gs;
          uint16_t tf_padding0;
          uint16_t tf_fs;
          uint16_t tf_padding1;
          uint16_t tf_es;
          uint16_t tf_padding2;
          uint16_t tf_ds;
          uint16_t tf_padding3;
          uint32_t tf_trapno;
          // below here defined by x86 hardware
          uint32_t tf_err;
          uintptr_t tf_eip;
          uint16_t tf_cs;
          uint16_t tf_padding4;
          uint32_t tf_eflags;
          // below here only when crossing rings, such as from user to kernel
          uintptr_t tf_esp;
          uint16_t tf_ss;
          uint16_t tf_padding5;
        } __attribute__((packed));
      */

        // panic("T_SWITCH_TOU** ??\n");
        /* 切换的话， 需要以下步骤：
             1. 当前是用户态吗？ 如果是用户态，就不需要切换了。
             2. tf 这个结构当中， 主要是需要把其值修改掉。 
         */
        cprintf("T_SWITCH_TO_USER\n");
        cprintf("cs: 0x%x\n", tf->tf_cs);
        cprintf("eip: 0x%x\n", tf->tf_eip);
        cprintf("ss: 0x%x\n", tf->tf_ss);
        cprintf("esp: 0x%x\n", tf->tf_esp);
        if (tf->tf_cs != USER_CS) {
            tf->tf_cs = USER_CS;
            tf-> tf_ds = USER_DS;
            tf->tf_ss = 
        }
        break;
    case T_SWITCH_TOK:
        cprintf("T_SWITCH_TO_KERNEL");
        //panic("T_SWITCH_TOK** ??\n");
        break;
    case IRQ_OFFSET + IRQ_IDE1:
    case IRQ_OFFSET + IRQ_IDE2:
        /* do nothing */
        break;
    default:
        // in kernel, it must be a mistake
        if ((tf->tf_cs & 3) == 0) {
            print_trapframe(tf);
            panic("unexpected trap in kernel.\n");
        }
    }
}

/* *
 * trap - handles or dispatches an exception/interrupt. if and when trap() returns,
 * the code in kern/trap/trapentry.S restores the old CPU state saved in the
 * trapframe and then uses the iret instruction to return from the exception.
 * */
void
trap(struct trapframe *tf) {
    // dispatch based on what type of trap occurred
    trap_dispatch(tf);
}

