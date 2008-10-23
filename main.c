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

unsigned char getAddress(void);
unsigned char myAddress;
uint8_t relais_port_state;
static volatile uint8_t mili_sec_counter, uartcount;

int main(void)
{
	unsigned char destination;
	unsigned char mcucsr;

	mcucsr = MCUCSR;
	MCUCSR = 0;

	uart_init(UART_BAUD_SELECT(UART_BAUDRATE, F_CPU));

	fdevopen((void*)uart_putc,NULL);
	
	/* Falls der Reset nicht vom Watchdog kam */
	if(!(mcucsr & (1<<WDRF)))
	{
		printf("%d;%d;%d;%d\r\n",10,10,0,0);
	}
	else
		printf("%d;%d;%d;%d\r\n",10,11,0,0);

	lcd_init();
	lcd_clear();
	lcd_puts("Hallo Welt!");
//	lcdInt(123);

	//PORTD=255;
	//DDRD=230;
	PORTC = relais_port_state;
	DDRC=255;

	DDRA |= (1<<PA7); // LED Backlight
	DDRD |= (1<<PD7); // Buzzer
	PORTD |= (1<<PD7); // Buzzer off!

	unsigned char rxbyte,txbuf[255],counter,numbytes=0,uart_dest=0;
	#ifdef DIP_KEYBOARD
	myAddress = getAddress();
	#else
	myAddress = MY_ADDRESS;
	#endif
	
	/* Prescaler 1024 */
	TCCR0 = (1<<CS02) | (1<<CS00);
	TIMSK = (1<<TOIE0);

	rf12_init(1);									// ein paar Register setzen (z.B. CLK auf 10MHz)
	sei();

	//wdt_enable(WDTO_1S);


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
				else
				{
					/*for(counter=0;counter<numbytes;counter++)
					{
						rf12_putc(txbuf[counter]);
					}
					delaycnt = 1;*/
					rf12_txpacket(txbuf, numbytes, destination, 0);
				}
				uartcount=0;
			}
		}
		
		if (rf12_data())                                        // Daten im RF12 Empfangspuffer ?
		{
			unsigned char rxbyte = rf12_getchar();
			uart_putc(rxbyte);	
		}
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
		uartcount = 0;
		mili_sec_counter = 0;
	}
	//if(helloCounter++ == 200)
	//{
	//	PORTC ^= 1;
	//	helloCounter = 0;
	//}
}

