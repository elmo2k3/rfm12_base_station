/* Base station for RFM12 
 *
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
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <avr/sleep.h>
#include <stdlib.h>
#include <portbits.h>
#include <util/delay.h>
#include <avr/wdt.h>
#include <stdio.h>

#include "global.h"
#include "uart.h"
#include "rf12.h"
#include "main.h"
#include "lcd.h"

/* Port usage
 *
 * PORTA: PA0-PA5 LCD
 *        PA7 LCD Backlight active low
 *
 * PORTD: PD3-PD6 Digital inputs
 *        PD7 Buzzer
 */

#define KEY_INPUT (PIND & ((1<<PD3)|(1<<PD4)|(1<<PD5)|(1<<PD6)))

uint8_t relais_port_state;
static volatile uint8_t mili_sec_counter, uartcount;
static volatile char key_state, key_temp; 

int main(void)
{
	unsigned char destination = 0;
	unsigned char rxbyte,txbuf[255],numbytes=0,uart_dest=0;

	uart_init(UART_BAUD_SELECT(UART_BAUDRATE, F_CPU));

	/* now we can use printf. the output goes to uart */
	fdevopen((void*)uart_putc,NULL);

	/* say hello! command to tell had that a hard-reset occured */
	printf("%d;%d;%d;%d\r\n",10,10,0,0);

	lcd_init();
	lcd_clear();
	lcd_puts("Hallo Welt!");
	
	DDRC=255;

	DDRA |= (1<<PA7); // LED Backlight
	DDRD |= (1<<PD7); // Buzzer
	PORTD |= (1<<PD7); // Buzzer off!
	
	DDRD &= ~((1<<PD3)|(1<<PD4)|(1<<PD5)|(1<<PD6));

	PORTD |= (1<<PD3)|(1<<PD4)|(1<<PD5)|(1<<PD6); //Pullups

	
	/* Prescaler 1024 */
	TCCR0 = (1<<CS02) | (1<<CS00);
	TIMSK = (1<<TOIE0);

	/* init rfm12
	 * 1 is for first init (with delay loop) */
	rf12_init(1);

	sei();

	/* Badurate, Channel .... */
	rf12_config(RF_BAUDRATE, CHANNEL, 0, QUIET);

	key_state = KEY_INPUT;

	for (;;)
	{       
		if(!uartcount)	
			mili_sec_counter = 0;

		/* data in the uart buffer? */
		if (uart_data())
		{       
			/* first byte: destination
			 * second byte: number of data bytes */
			rxbyte=uart_getchar();
			switch(uartcount)
			{
				case 0: destination=rxbyte; uart_dest = destination; uartcount++; break; //RF Zieladresse
				case 1: numbytes=rxbyte; uartcount++; break;	//Anzahl zu empfangender Bytes
				default: txbuf[uartcount-2]=rxbyte; uartcount++;
			}
			/* last byte received? */
			if(numbytes==uartcount-2)
			{
				/* is the packet for me? */
				if(uart_dest == MY_ADDRESS)
				{
					/* txbuf[0] = command */
					switch(txbuf[0])
					{
						case COMMAND_SET_RELAIS: relais_port_state = txbuf[1];
									 PORTC = relais_port_state;
									 break;
						case COMMAND_ACTIVATE_LCD: 
									 PORTA &= ~(1<<PA7);
									 break;
						case COMMAND_DEACTIVATE_LCD: 
									 PORTA |= (1<<PA7);
									 break;
						case COMMAND_BEEP_ON: 
									 PORTD &= ~(1<<PD7);
									 break;
						case COMMAND_BEEP_OFF: 
									 PORTD |= (1<<PD7);
									 break;
						case COMMAND_LCD_TEXT: 
									 lcd_clear();
									 lcd_puts(&txbuf[1]);
									 break;
					}
				}
				/* packet is not for me, send it via rf */
				else
				{
					rf12_txpacket(txbuf, numbytes, destination, 0);
				}
				uartcount=0;
			}
		}
		
		/* got data from rfm12? */
		if (rf12_data())
		{
			unsigned char rxbyte = rf12_getchar();
			/* put every byte to uart */
			uart_putc(rxbyte);	
		}

		/* digital input changed? */
		key_temp = KEY_INPUT;
		if(key_state != key_temp)
		{
			if((key_temp & (1<<PD3)) != (key_state & (1<<PD3)))
			{
				if(key_temp& (1<<PD3)) // now open
					printf("10;30;0;0\r\n");
				else // now closed
					printf("10;31;0;0\r\n");
			}
			if((key_temp & (1<<PD4)) != (key_state & (1<<PD4)))
			{
				if(key_temp & (1<<PD4)) // now open
					printf("10;32;0;0\r\n");
				else // now closed
					printf("10;33;0;0\r\n");
			}
			if((key_temp & (1<<PD5)) != (key_state & (1<<PD5)))
			{
				if(key_temp & (1<<PD5)) // now open
					printf("10;34;0;0\r\n");
				else // now closed
					printf("10;35;0;0\r\n");
			}
			if((key_temp & (1<<PD6)) != (key_state & (1<<PD6)))
			{
				if(key_temp & (1<<PD6)) // now open
					printf("10;36;0;0\r\n");
				else // now closed
					printf("10;37;0;0\r\n");
			}
			key_state = key_temp;
			_delay_ms(100);
		}
	}
}


/* 100 mal pro Sekunde (ungefaehr) */
ISR(TIMER0_OVF_vect)
{
	static uint8_t timeout_counter = 0;
	TCNT0 = 255-156;

	if(100 == mili_sec_counter++)
	{
		timeout_counter++;
		printf("%d;%d;%d;%d\r\n",10,12,0,0);
		uartcount = 0;
		mili_sec_counter = 0;
	}
	if(timeout_counter > 3) // hard reset
	{
		wdt_enable(WDTO_15MS);
		while(1);
	}
/*	static char ct0, ct1;
	char p;
	
	p = key_state ^ ~key_temp;	// key changed ?
	ct0 = ~( ct0 & p );		// reset or count ct0
	ct1 = (ct0 ^ ct1) & p;		// reset or count ct1
	p &= ct0 & ct1;		// count until roll over 
	key_state ^= p;		// then toggle debounced state
	key_press |= key_state & p;	// 0->1: key pressing detect
*/
}

