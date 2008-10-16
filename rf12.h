/*
 * Copyright (C) 2007-2008 Bjoern Biesenbach <bjoern@bjoern-b.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 *
 * 14. Oct 2008
 * Based on lib of Benedikt benedikt83 at gmx.net
 */

/** initialize module
 */
extern void rf12_init(void);

/** configure module
 */
extern void rf12_config(unsigned short baudrate, 
		unsigned char channel,
		unsigned char power,
		unsigned char environment);

/** transmit a packet
 */
extern void rf12_txpacket(uint8_t *data, uint8_t count, uint8_t address, uint8_t ack_required);

/** data in receive buffer?
 */
extern unsigned char rf12_data(void);

/** get one byte from receive buffer
 */
extern unsigned char rf12_getchar(void);

/** transmit buffer full?
 */
extern unsigned char rf12_busy(void);

/** macro for calculating frequency value out of frequency in MHz
 */
#define RF12FREQ(freq)	((unsigned short)((freq-430.0)/0.0025))

#define QUIET		1
#define NORMAL		2
#define NOISY		3

#define RECEIVED_OK		1		// Daten erfolgreich empfangen
#define RECEIVED_FAIL		2		// Daten fehlerhaft empfangen -> bitte nochmal senden
#define DATAINBUFFER		4		// Empfänger möchte Daten senden

