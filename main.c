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

unsigned char getAddress(void);
unsigned char myAddress;
unsigned char destination;
//uint8_t relais_port_state __attribute__ ((section (".noinit")));
uint8_t relais_port_state;
static volatile uint8_t mili_sec_counter, uartcount;

int main(void)
{
	unsigned char mcucsr;

	mcucsr = MCUCSR;
	MCUCSR = 0;

	lcd_init();
	lcd_clear();
	lcd_puts("Hallo Welt!");
//	lcdInt(123);

	//PORTD=255;
	//DDRD=230;
	PORTC = relais_port_state;
	DDRC=255;

	unsigned char rxbyte,txbuf[255],counter,numbytes=0,uart_dest=0;
	#ifdef DIP_KEYBOARD
	myAddress = getAddress();
	#else
	myAddress = MY_ADDRESS;
	#endif
	
	/* Prescaler 1024 */
	TCCR0 = (1<<CS02) | (1<<CS00);
	TIMSK = (1<<TOIE0);

	sei();
	rf12_init();									// ein paar Register setzen (z.B. CLK auf 10MHz)

	//wdt_enable(WDTO_1S);

	uart_init(UART_BAUD_SELECT(UART_BAUDRATE, F_CPU));

	fdevopen((void*)uart_putc,NULL);

	/* Falls der Reset nicht vom Watchdog kam */
	if(!(mcucsr & (1<<WDRF)))
	{
		printf("%d;%d;%d;%d\r\n",10,10,0,0);
	}
	else
		printf("%d;%d;%d;%d\r\n",10,11,0,0);

	rf12_config(RF_BAUDRATE, CHANNEL, 0, QUIET);	// Baudrate, Kanal (0-3), Leistung (0=max, 7=min), Umgebungsbedingungen (QUIET, NORMAL, NOISY)
	for (;;)
	{       
	//	wdt_reset();		//Falls Daten im Uartpuffer dann kein watchdog reset! Watchdog wird ausgelöst wenn Paket nicht komplett
		if(!uartcount)	
			mili_sec_counter = 0;
		if (uart_data())                                        // Daten im UART Empfangspuffer ?
		{       
			rxbyte=uart_getchar();
			switch(uartcount)
			{
				case 0: destination=rxbyte; uart_dest = destination; uartcount++; break; //RF Zieladresse
				case 1: numbytes=rxbyte; uartcount++; break;	//Anzahl zu empfangender Bytes
				default: txbuf[uartcount-2]=rxbyte; uartcount++;
			}
			if(numbytes==uartcount-2)
			{
				if(uart_dest == myAddress)
				{
					/* txbuf[0] = command */
					switch(txbuf[0])
					{
						case COMMAND_SET_RELAIS: relais_port_state = txbuf[1];
									 PORTC = relais_port_state;
									 break;
					}
				}
				else
				{
					/*for(counter=0;counter<numbytes;counter++)
					{
						rf12_putc(txbuf[counter]);
					}
					delaycnt = 1;*/
					static uint8_t txid = 1;
					rf12_stoprx();
					rf12_txdata(txbuf, numbytes, 0, txid++, destination);
					rf12_rxmode();
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
/* 100 mal pro Sekunde (ungefaehr) */
ISR(TIMER0_OVF_vect)
{
	static uint16_t helloCounter=0;
	TCNT0 = 255-156;

	if(100 == mili_sec_counter++)
	{
		printf("%d;%d;%d;%d\r\n",10,12,0,0);
		rf12_RxHead = 1;
		uartcount = 0;
		mili_sec_counter = 0;
	}
	//if(helloCounter++ == 200)
	//{
	//	PORTC ^= 1;
	//	helloCounter = 0;
	//}
}

