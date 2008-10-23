/* Minimalistic lcd lib
 * 16x2 lcd
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
#include <util/delay.h>
#include "lcd.h"

#define LCD_PORT PORTA
#define LCD_DDR DDRA

#define LCD_E PA5
#define LCD_RS PA4

static uint8_t x,y;
static void lcd_enable(void);
static void lcd_command(uint8_t data);
static void lcd_data(uint8_t data);

static void lcd_enable()
{
	LCD_PORT |= (1<<LCD_E);	//LCD-Enable
	_delay_loop_1(3);
	LCD_PORT &= ~(1<<LCD_E);	//LCD-Enable wieder low
}

static void lcd_data(uint8_t data)
{
	uint8_t temp=data;
	data = data>>4;		//Die beiden Nibbles vertauschen
	data &= 15;		//Oberes Nibble auf Null setzen (0b00001111)
	data |= 16;		//0b00010000 also Bit 5 auf 1
	LCD_PORT = (data & 0x3F) | (LCD_PORT & 0xC0);
	lcd_enable();
	temp &= 15;		//Nibble schon an der richtigen Stelle, oberes wieder
				// auf Null
	temp |= 16;
	LCD_PORT = (temp & 0x3F) | (LCD_PORT & 0xC0);
	lcd_enable();
	_delay_us(50);
}

static void lcd_command(uint8_t data) //Wie lcd_data nur ohne RS zu setzen
{
	uint8_t temp=data;
	data = data>>4;		//Die beiden Nibbles vertauschen
	data &= 15;		//Oberes Nibble auf Null setzen (0b00001111)
	LCD_PORT = (data & 0x3F) | (LCD_PORT & 0xC0);
	lcd_enable();
	temp &= 15;		//Nibble schon an der richtigen Stelle, oberes wieder
				// auf Null
	LCD_PORT = (temp & 0x3F) | (LCD_PORT & 0xC0);
	lcd_enable();
	_delay_us(50);
}

void lcd_init()
{
	uint8_t i;
	for(i=0;i<=250;i++);
	{
		_delay_ms(1);
	}
	LCD_DDR = (1<<LCD_E) | (1<<LCD_RS) | (1<<PA0)| (1<<PA1)| (1<<PA2)| (1<<PA3);
	LCD_PORT |= 3;		//0b00000011
	lcd_enable();		//Muss dreimal gesendet werden
	_delay_ms(5);
	lcd_enable();
	_delay_ms(5);
	lcd_enable();
	_delay_ms(5);
	LCD_PORT |= 2;		// 4bit-Modus einstellen
	lcd_enable();
	_delay_ms(5);
	lcd_command(40);
	lcd_command(12);
	lcd_command(4);
}

void lcd_clear()
{
	lcd_command(1);
	_delay_ms(5);
	x=0;
	y=0;
}

void lcdInt(uint8_t value)
{
	if(value<10)
	{
		lcd_data('0');
		lcd_data(value+48);
	}
	if(value>=10 && value<100)
	{
		lcd_data(value/10+48);
		lcd_data(value%10+48);
	}
	if(value>=100)
	{
		lcd_data(value/100+48);
		value %=100;
		lcd_data(value/10+48);
		lcd_data(value%10+48);
	}
}

void lcd_puts(char *string)
{
	while(*string)
	{
		lcd_data(*string++);
		if(++x == 16)
		{
			lcd_command(0x80+0x40);
		}
		else if(x == 32)
		{
			lcd_command(0x80);
			x = 0;
		}
	}
}

