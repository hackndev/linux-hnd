enum adc_state {
	ADC_STATE_IDLE,
	ADC_STATE_TOUCHSCREEN,  // Servicing the touchscreen
	ADC_STATE_USER          // Servicing a user-level request
};

struct touchscreen_data;

struct adc_data {
	enum adc_state     state;
	struct semaphore   lock;      // Mutex for access from user-level
	wait_queue_head_t  waitq;     // Waitq for user-level access (waits for interrupt service)
	int                last;      // Return value for user-level acces
	int                user_mux;  // Requested mux for the next user read
	int                ts_mux;    // Requested mux for the next touchscreen read
	unsigned long      shared;    // Shared resources
	unsigned int	   adc_irq;
	struct device	   *asic;
	struct device	   *adc;

	struct touchscreen_data *ts;
};

extern void asic2_adc_start_touchscreen (struct adc_data *adc, int mux);
