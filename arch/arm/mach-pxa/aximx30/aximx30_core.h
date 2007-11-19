/* Core Hardware driver for X30 (ASIC3, EGPIOs)
 *
* Authors Giuseppe Zompatori <giuseppe_zompatori@yahoo.it>
 *
 * based on previews work, see bellow:
 *
 * Copyright (c) 2005 SDG Systems, LLC
 *
 * 2005-03-29   Todd Blumer             Converted  basic structure to support hx4700
 */

struct x30_core_funcs {
    int (*udc_detect)( void );
};

