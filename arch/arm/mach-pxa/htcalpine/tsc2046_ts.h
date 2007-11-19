/*
 * temporary TSC2046 touchscreen hack
 */

#ifndef _TSC2046_TS_H
#define _TSC2046_TS_H

struct tsc2046_mach_info {
    int port;  
    int clock;
    int pwrbit_X;
    int pwrbit_Y;
    int irq;
};

#define TSC2046_SAMPLE_X 0xd0
#define TSC2046_SAMPLE_Y 0x90

#endif
