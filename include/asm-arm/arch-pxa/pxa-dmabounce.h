/*
 * PXA-specific dma_needs_bounce
 *
 * Some PXA platforms (h5400, eseries) have different requirements for DMA
 * bouncing, so we define a dma_needs_bounce() that can call a
 * platform-specific function set by pxa_set_dma_needs_bounce().
 *
 * Better would be to teach dmabounce.c to allow the dma_needs_bounce function
 * to be set at run-time; this is just a stop-gap.
 */

extern void pxa_set_dma_needs_bounce(void *needs_bounce_func);
