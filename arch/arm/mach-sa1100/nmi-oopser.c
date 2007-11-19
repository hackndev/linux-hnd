#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/gfp.h>
#include <linux/module.h>
#include <linux/delay.h>

#include <asm/ptrace.h>
// #include <asm/proc/domain.h>
#include <asm/hardware.h>

static void *nmi_stack;

asm("						\n\
nmi_start:					\n\
	mrs	r8, spsr			\n\
	ldr	r9, .Lstack			\n\
	ldr	sp, [r9]			\n\
	sub	sp, sp, #18 * 4			\n\
	str	r8, [sp, #16 * 4]		\n\
	str	lr, [sp, #15 * 4]		\n\
	stmia	sp, {r0 - r7}			\n\
	add	r0, sp, #8 * 4			\n\
	mrs	r2, cpsr			\n\
	bic	r1, r2, #0x1f			\n\
	orr	r1, r1, #0x13			\n\
	msr	cpsr_c, r1			\n\
	mov	r0, r0				\n\
	stmia	r0, {r8 - lr}			\n\
	mov	r0, r0				\n\
	msr	cpsr_c, r2			\n\
	mov	r0, r0				\n\
	mov	r0, sp				\n\
	mov	lr, pc				\n\
	ldr	pc, .Lfn			\n\
	ldmia	sp, {r0 - r7}			\n\
	ldr	r8, [sp, #16 * 4]		\n\
	ldr	lr, [sp, #15 * 4]		\n\
	add	sp, sp, #18 * 4			\n\
	msr	spsr, r8			\n\
	movs	pc, lr				\n\
						\n\
.Lstack: .long nmi_stack			\n\
.Lfn:	.long	nmi_fn				\n\
nmi_end:");

extern unsigned char nmi_start, nmi_end;

extern void dump_backtrace(struct pt_regs *regs, struct task_struct *tsk);
extern void dump_instr(struct pt_regs *regs);

static void __attribute__((unused)) nmi_fn(struct pt_regs *regs)
{
	struct thread_info *thread;
	struct task_struct *tsk;
	unsigned long osmr0, osmr1, oscr, ossr, icmr, icip;

	oscr = OSCR;
	osmr0 = OSMR0;
	osmr1 = OSMR1;
	ossr = OSSR;
	icmr = ICMR;
	icip = ICIP;

	OSSR = OSSR_M1;
	ICMR &= ~IC_OST1;

	thread = (struct thread_info *)(regs->ARM_sp & ~8191);
	tsk = thread->task;

	bust_spinlocks(1);
	printk("OSMR0:%08lx OSMR1:%08lx OSCR:%08lx OSSR:%08lx ICMR:%08lx ICIP:%08lx\n",
		osmr0, osmr1, oscr, ossr, icmr, icip);
	printk("Internal error: NMI watchdog\n");
	print_modules();
	printk("CPU: %d\n", smp_processor_id());
	show_regs(regs);
	printk("Process %s (pid: %d stack limit = 0x%p)\n",
		tsk->comm, tsk->pid, thread + 1);
	show_stack(tsk, (unsigned long *)regs->ARM_sp);
	dump_backtrace(regs, tsk);
	dump_instr(regs);
	bust_spinlocks(0);

	OSSR = OSSR_M1;
	OSMR1 = OSSR + 36864000;
	ICMR |= IC_OST1;
}

static int nmi_init(void)
{
	unsigned char *vec_base = (unsigned char *)vectors_base();
return 0;
	nmi_stack = (void *)__get_free_page(GFP_KERNEL);
	if (!nmi_stack)
		return -ENOMEM;

	nmi_stack += PAGE_SIZE;

	modify_domain(DOMAIN_USER, DOMAIN_MANAGER);
	memcpy(vec_base + 0x1c, &nmi_start, &nmi_end - &nmi_start);
	modify_domain(DOMAIN_USER, DOMAIN_CLIENT);

	/*
	 * Ensure timer 1 is set to FIQ, and enabled.
	 */
	OSMR1 = OSCR - 1;
	OSSR = OSSR_M1;
	OIER |= OIER_E1;
	ICLR |= IC_OST1;
	ICMR |= IC_OST1;

	return 0;
}

__initcall(nmi_init);
