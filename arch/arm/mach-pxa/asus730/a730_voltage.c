/*
 * Helper module for cpufreq. Is't responsible for changing core voltage
 * of cpu. 
 * Send commands to MAX1586B through second i2c (pwr_i2c) controller
 * integrated in pxa270. We don't want to use integrated voltage-change
 * sequencer, because it works @40kbit/s only.
 *
 * Use consistent with the GNU GPL is permitted,
 * provided that this copyright notice is
 * preserved in its entirety in all copies and derived works.
 *
 */
#include <linux/module.h>
#include <linux/moduleparam.h>

#ifdef CONFIG_CPU_FREQ

#include <linux/proc_fs.h>
#include <linux/cpufreq.h>

#include <asm/hardware.h>
#include <asm/arch/pxa-regs.h>
#include <asm/delay.h>

#define PCFR_GPR_EN	(1 << 4)
#define PWRMODE_VC	(1 << 3)
#define ICR_FM		(1 << 15)

typedef void (*change_voltage_fun)(unsigned curr_f, unsigned new_f,
				   unsigned mV, unsigned post_chg);
extern void set_change_voltage_function(change_voltage_fun);

/* pwr i2c cmd send time = 125us(500us)
 * v change min 100mV - 10us
 * v change max 650mV - 65us
 */

/*static unsigned f2cmd (const unsigned khz) {
	int i;
	unsigned cmd = -1;
	for (i = 0; pxa_v && pxa_v[i].freq && pxa_v[i].freq * 1000 < khz; i++)
		;
	if (pxa_v && pxa_v[i].freq) {
		cmd = v2cmd (pxa_v[i].mV);
		printk("chg_V %umV %02X!\n", pxa_v[i].mV, cmd);
	}
	return cmd;	
}*/
static inline unsigned v2cmd (const unsigned mV) {
	unsigned val = (mV - 700) / 25;
	if (val > 31)
		val = 31;
	return val;
}

/*
 * PCMD0-31: 0
 * PIBMR: 0x03
 * PIDBR: 0x1C
 * PICR:  0x1060
 * PISR:  0x0
 * PISAR: 0x0
 * PVCR: 0
 * PCFR: 0x70
 */
static void send_cmd(unsigned cmd) {
	unsigned long flags, unused;
	/* voltage change seq. @ speed 40kbit/s */
	PVCR = 0x14; /* MAX1586B address */
	PCMD0 = PCMD_LC | cmd;
	/* PCFR [FVC] = 0; */
	PCFR = PCFR_PI2C_EN | PCFR_GPR_EN;
	local_irq_save(flags);
	__asm__ __volatile__("	\n"
"	b   2f			\n"
"	.align  7		\n"
"1:	mcr p14, 0, %1, c7, c0, 0\n"/* set PWRMODE[VC] */
"	b   3f			\n"
"2:	b   1b			\n"
"3:	nop			\n"
	: "=&r" (unused) : "r" (PWRMODE_VC));
	local_irq_restore(flags);
	/* wait while command is in progress */
	while (PVCR & 0x4000) {
		udelay(1);
	}
}

static void send_cmd_reg(unsigned cmd) {
	/* this is faster version: 160kbit/s == 125us */
	/* maybe we shoult wait after increasing voltage ? */
	/*
	ICR = ICR_START | ICR_TB | (ICR & ~(ICR_STOP | ICR_ALDIE));
	 */
	PWRICR = (ICR_IUE | ICR_SCLE | ICR_FM) | (ICR_BEIE | ICR_ITEIE);
	PWRIDBR = (0x14 << 1) | 0/*write*/;
	//PWRICR &= ~(ICR_STOP | ICR_ALDIE);
	PWRICR |= ICR_START | ICR_TB;
	/* wait for transmit-buf-empty int */
	while ((PWRICR & ICR_TB))
		udelay(1);
	PWRISR &= ~(ISR_ITE);
	//PWRISR |= ISR_ALD; //why?
	PWRIDBR = cmd;
	PWRICR &= ~(ICR_START);
	PWRICR |= ICR_STOP | ICR_TB | ICR_ALDIE;
	/* wait for transmit-buf-empty int */
	while ((PWRICR & ICR_TB))
		udelay(1);
	
	PWRISR &= ~(ISR_ITE);
	PWRICR &= ~(ICR_STOP);
}

static unsigned core_v;

static void send_decrease_v(void* data) {
	unsigned mV = (unsigned)data;
	unsigned cmd;

	printk ("%s(%u)\n\n\n",__FUNCTION__,mV);
	cmd = v2cmd (mV);
	send_cmd_reg (cmd);
	core_v = mV;
}

DECLARE_WORK (v_workqueue, send_decrease_v, NULL);

static void change_voltage(const unsigned f_old, const unsigned f_new,
			   const unsigned mV, const unsigned state)
{
//	struct timeval tv1,tv2;
	unsigned cmd;

	switch (state) {
	case CPUFREQ_PRECHANGE:
		if (f_new <= f_old) {
			break;
		}
		/* if freq rised we want ramp up v before */
		//do_gettimeofday(&tv1);
		cmd = v2cmd (mV);
		send_cmd_reg (cmd);
		core_v = mV;
		//printk("PRE chg_V inc %02X!\n", cmd);
//		do_gettimeofday(&tv2);
//		printk("%s(%u,%u,%u,%u) = %ld\n",__FUNCTION__,f_old,f_new,
//		       mV,state, (tv2.tv_sec-tv1.tv_sec) * 1000 * 1000
//		       + tv2.tv_usec-tv1.tv_usec);
		break;
	case CPUFREQ_POSTCHANGE:
		if (f_new >= f_old) {
			break;
		}
		/* if freq falled we want decrease v after */
		/* we don't have to do that immediatelly */
		//do_gettimeofday(&tv1);
		v_workqueue.data = (void*)mV;
		schedule_work(&v_workqueue);
		//printk("POST chg_V dec %02X!\n", cmd);
//		do_gettimeofday(&tv2);
//		printk("%s(%u,%u,%u,%u) = %ld\n",__FUNCTION__,f_old,f_new,
//		       mV,state, (tv2.tv_sec-tv1.tv_sec) * 1000 * 1000
//		       + tv2.tv_usec-tv1.tv_usec);
		break;
	}
}

/* E.28:POWER MANAGER: Core hangs during voltage change when there are
	outstanding transactions on the bus
   E.50:POWER MANAGER: The processor does not exit from sleep/deep-sleep mode. 
   E.65:POWER MANAGER: Voltage change coupled with power mode change causes
	the processor to be unable to wake from sleep mode.
   E.68:POWER MANAGER: Pins selected in the Keyboard Wake-Up Enable Register
	(PKWR) may not cause the processor to wake up from sleep mode.
*/

static int v_freq_transition(struct notifier_block *nb,
				 unsigned long val, void *data)
{
	struct cpufreq_freqs* f = data;

	printk("%s v=%ld cpu=%u old=%u new=%u flags=%hu\n",
	       __FUNCTION__,val,f->cpu,f->old,f->new,f->flags);
	switch (val) {
	case CPUFREQ_PRECHANGE:
		break;

	case CPUFREQ_POSTCHANGE:
		break;
	}
	return 0;
}

static int v_freq_policy(struct notifier_block *nb,
			     unsigned long val, void *data)
{
	struct cpufreq_freqs* f = data;

	printk("%s v=%ld cpu=%u old=%u new=%u flags=%hu\n",
	       __FUNCTION__,val,f->cpu,f->old,f->new,f->flags);
	switch (val) {
	case CPUFREQ_ADJUST:
	case CPUFREQ_INCOMPATIBLE:
		//printk(KERN_DEBUG "new clock %d kHz\n", policy->max);
		break;
	case CPUFREQ_NOTIFY:
		printk(KERN_ERR "%s: got CPUFREQ_NOTIFY\n", __FUNCTION__);
		break;
	}
	return 0;
}

struct notifier_block freq_transition = {
	.notifier_call = v_freq_transition,
};
struct notifier_block freq_policy = {
	.notifier_call = v_freq_policy,
};

int read_voltage(char *buf, char **start, off_t offset,
		 int count, int *eof, void *data)
{
	int len = 0;

	len = sprintf(buf,"%d mV\n",core_v > 1500 ? 1475 : core_v);
	*eof = 1;
	return len;
}

static int __init v_init(void)
{
	printk("%s\n",__FUNCTION__);
	pxa_set_cken(CKEN15_PWRI2C,1);
	cpufreq_register_notifier(&freq_policy, CPUFREQ_POLICY_NOTIFIER);
	cpufreq_register_notifier(&freq_transition, CPUFREQ_TRANSITION_NOTIFIER);
	set_change_voltage_function(change_voltage);
	/* it should be sysfs rather than proc */
	create_proc_read_entry("core_voltage", 0 /* default mode */,
			       NULL /* parent dir */, read_voltage,
			       NULL /* client data */);
	return 0;
}

static void __exit v_exit(void)
{
	remove_proc_entry("core_voltage", NULL /* parent dir */);
	set_change_voltage_function(NULL);
	cpufreq_unregister_notifier(&freq_transition, CPUFREQ_TRANSITION_NOTIFIER);
	cpufreq_unregister_notifier(&freq_policy, CPUFREQ_POLICY_NOTIFIER);
	pxa_set_cken(CKEN15_PWRI2C,0);
}

module_init(v_init);
module_exit(v_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Michal Sroczynski <msroczyn@elka.pw.edu.pl>");
MODULE_DESCRIPTION("Core-voltage change helper for Asus MyPal 730");
#endif
