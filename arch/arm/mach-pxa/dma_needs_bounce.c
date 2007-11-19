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

#include <linux/device.h>
#include <linux/dma-mapping.h>

static int (*dma_needs_bounce_p)(struct device*, dma_addr_t, size_t) = NULL;

void
pxa_set_dma_needs_bounce(void *needs_bounce_func)
{
	dma_needs_bounce_p = needs_bounce_func;
}
EXPORT_SYMBOL(pxa_set_dma_needs_bounce);

int
dma_needs_bounce(struct device *dev, dma_addr_t addr, size_t size) {

	return dma_needs_bounce_p ? dma_needs_bounce_p(dev, addr, size) : 0;
}
