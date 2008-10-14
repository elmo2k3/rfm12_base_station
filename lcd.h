#ifndef __DEFINE_LCD_H__
#define __DEFINE_LCD_H__

void lcd_init();
void lcd_enable();
void lcd_command(uint8_t data);
void lcd_data(uint8_t data);
void lcd_puts(char *string);
void lcdInt(uint8_t value);
void lcd_clear();

#endif
