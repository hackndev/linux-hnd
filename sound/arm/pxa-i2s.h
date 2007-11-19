/*
 *  Generic I2S code for pxa250
 *
 *  Phil Blundell <pb@nexus.co.uk>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 */

#ifndef PXA_I2S_H
#define PXA_I2S_H

#include "pxa-audio.h"

extern void pxa_i2s_init (void);

extern void pxa_i2s_shutdown (void);

extern int pxa_i2s_set_samplerate (long val);

extern audio_stream_t pxa_i2s_output_stream;
extern audio_stream_t pxa_i2s_input_stream;

#endif
