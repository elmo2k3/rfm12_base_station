/****************************************************************************
 *																			*
 *							universelle RFM12 lib							*
 *																			*
 *								Version 0.9									*
 *																			*
 *								© by Benedikt								*
 *																			*
 *						Email:	benedikt83 ät gmx.net						*
 *																			*
 ****************************************************************************
 *																			*
 *	Die Software darf frei kopiert und verändert werden, solange sie nicht	*
 *	ohne meine Erlaubnis für kommerzielle Zwecke eingesetzt wird.			*
 *																			*
 ***************************************************************************/

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <avr/sleep.h>
#include <stdlib.h>
#include <portbits.h>
#include <util/delay.h>
#include <avr/wdt.h>

#include "global.h"
#include "uart.h"
#include "rf12.h"
#include "led.h"
#include "main.h"

unsigned char getAddress(void);
unsigned char myAddress;
unsigned char destination;

int main(void)
{
	//PORTD=255;
	//DDRD=230;
	DDRC=255;
	unsigned char uartcount=0,rxbyte,txbuf[255],counter,numbytes=0;
	#ifdef DIP_KEYBOARD
	myAddress = getAddress();
	#else
	myAddress = MY_ADDRESS;
	#endif
	sei();
	rf12_init();									// ein paar Register setzen (z.B. CLK auf 10MHz)
	led_init();
	wdt_enable(WDTO_120MS);

    uart_init(UART_BAUD_SELECT(UART_BAUDRATE, F_CPU));
	rf12_config(RF_BAUDRATE, CHANNEL, 0, QUIET);	// Baudrate, Kanal (0-3), Leistung (0=max, 7=min), Umgebungsbedingungen (QUIET, NORMAL, NOISY)
	for (;;)
	{       
		if(!uartcount)	
			wdt_reset();		//Falls Daten im Uartpuffer dann kein watchdog reset! Watchdog wird ausgelöst wenn Paket nicht komplett
		if (uart_data())                                        // Daten im UART Empfangspuffer ?
		{       
			rxbyte=uart_getchar();
			switch(uartcount)
			{
				case 0: destination=rxbyte; uartcount++; break; //RF Zieladresse
				case 1: numbytes=rxbyte; uartcount++; break;	//Anzahl zu empfangender Bytes
				default: txbuf[uartcount-2]=rxbyte; uartcount++;
			}
			if(numbytes==uartcount-2)
			{
				for(counter=0;counter<numbytes;counter++)
				{
					rf12_putc(txbuf[counter]);
				}
				uartcount=0;
			}
		}
		
		if (rf12_data())                                        // Daten im RF12 Empfangspuffer ?
			uart_putc(rf12_getchar());	
	}
}

#ifdef DIP_KEYBOARD
unsigned char getAddress(void)
{
	ADDRESS_DDR = 0x00; // ganzer Port ist Input
	ADDRESS_PORT = 0xFF; // alle Pullups an
	return ADDRESS_PIN;
}
#endif


