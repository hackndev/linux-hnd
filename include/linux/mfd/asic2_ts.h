extern int asic2_touchscreen_attach (struct adc_data *adc);
extern void asic2_touchscreen_detach (struct adc_data *adc);
extern int asic2_touchscreen_suspend (struct adc_data *adc, pm_message_t state);
extern int asic2_touchscreen_resume (struct adc_data *adc);
extern int asic2_touchscreen_sample (struct adc_data *adc, int data);
