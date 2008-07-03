#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include "led.h"

uint8_t Red,Green,Blue;


ISR(SIG_OVERFLOW0)
{
   static volatile uint8_t Ticker;

   Ticker++;
   if (Ticker == 255) Ticker = 0;

   //--------------------------

   if (Red > Ticker)   LED_PORT |= LED_RED;
   else LED_PORT &= ~LED_RED;

   if (Green > Ticker) LED_PORT |= LED_GREEN;
   else LED_PORT &= ~LED_GREEN;

   if (Blue > Ticker)  LED_PORT |= LED_BLUE;
   else LED_PORT &= ~LED_BLUE;
}

void led_init()
{
	LED_DDRD = (1<<PD5)|(1<<PD6)|(1<<PD7);
	
	TCCR0 |= (1<<CS00);		// Timer0 / 1024
  	TIMSK |= (1<<TOIE0);	// int enable on Timer 0 overflow
}
