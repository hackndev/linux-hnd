/*
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

int recon_get_card_detect1(void);
int recon_get_card_detect2(void);
int recon_get_card_detect3(void);
void recon_register_pcmcia_callback(void (*f)(int slot, int in));
void recon_unregister_pcmcia_callback(void);
void recon_register_suspres_callback(void (*callback)(int));
void recon_unregister_suspres_callback(void (*callback)(int));
void recon_enable_i2s(int enable);
