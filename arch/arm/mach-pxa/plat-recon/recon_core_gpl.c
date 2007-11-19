/*
 * Recon Core Driver for Source Distribution.
 *
 * This file contains only symbols required to compile the kernel
 * for the TDS Recon. The user should replace the resulting
 * recon_core.ko file with the one from the binary distribution
 * from SDG Systems.
 *
 * Copyright 2005, SDG Systems, LLC.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/module.h>

int
recon_get_card_detect1(void)
{
    return 0;
}
EXPORT_SYMBOL(recon_get_card_detect1);

int
recon_get_card_detect2(void)
{
    return 0;
}
EXPORT_SYMBOL(recon_get_card_detect2);

int
recon_get_card_detect3(void)
{
    return 0;
}
EXPORT_SYMBOL(recon_get_card_detect3);

void
recon_register_pcmcia_callback(void (*f)(int slot, int in))
{
}
EXPORT_SYMBOL(recon_register_pcmcia_callback);

void
recon_unregister_pcmcia_callback(void)
{
}
EXPORT_SYMBOL(recon_unregister_pcmcia_callback);

