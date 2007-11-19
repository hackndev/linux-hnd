int samcop_touchscreen_attach (struct samcop_adc *adc);
void samcop_touchscreen_detach (struct samcop_adc *adc);
int samcop_touchscreen_suspend (struct samcop_adc *adc, pm_message_t state);
int samcop_touchscreen_resume (struct samcop_adc *adc);
