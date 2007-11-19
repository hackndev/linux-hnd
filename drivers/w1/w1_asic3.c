
/*
 * To initialize asic3 for onewire operation:
 * 
 * 1) turn on 32.768KHz and 24.576MHz clocks and owm clock enable in asic3 cdex register
 * 2) assert owm reset bit in extcf reset register for 1ms
 * 3) clear owm_smb bit in extcf sel register, set owm_en bit in same register
 * 4) set clkdiv.pre to 2 and clkdiv.div to 0
 * 
 * 5) set inten to 0x13
 * 6) read owm clkdiv register
 * 7) set asic3 intr status owm bit to 0
 * 
 */

/* 
 * To deinit onewire
 * 
 * 1) turn off 24.576MHz clock and clear owm clock enable bit in asic3 cdex register
 * 2) clear extcf sel owm_en bit
 * 3) read owm clkdiv register
*/

/* OWM reset: 
 *  set owm clkdiv.pre to 2, owm clkdiv.div to 0
 *  set owm inten epd bit
 *  wait 400us 
 */

/* 
 * To reset bus
 *
 * set reset bit in owm cmd register
 * loop 30 times:
 *   read owm intr register
 *   if intr_pd is set
 *     if intr_pdr is clear, then a device is found
 *     else no device
 *     break
 *   else wait 50 us
 * read owm clkdiv register
 */

/* onewire write byte
 *
 * write the byte to the owm dat register
 * read owm clkdiv register
 * poll owm intr TBE bit until it is set (for up to 30ms)
 * read owm clkdiv register
 */

/* onewire read byte
 * 
 * write 0xFF to owm dat register
 * read owm clkdiv register
 * poll owm intr RBF bit until it is set (for up to 30ms)
 *
 * while owm intr RBF bit is set (up to 1000 times)
 *   read the byte from owm dat register
 *   read the owm clkdiv register
 *   wait 1ms
 *
 * return the last byte that was read
 */

/* note, windows driver seems to read owm clkdiv register at unusual points in the code */
