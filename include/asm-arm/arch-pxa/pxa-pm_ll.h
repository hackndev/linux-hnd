struct pxa_ll_pm_ops {
	void (*suspend)(unsigned long);
	void (*resume)(void);
};

extern struct pxa_ll_pm_ops *pxa_pm_set_ll_ops(struct pxa_ll_pm_ops *new_ops);
