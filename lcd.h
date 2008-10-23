#ifndef __DEFINE_LCD_H__
#define __DEFINE_LCD_H__

extern void lcd_init(void);
extern void lcd_enable(void);
extern void lcd_command(uint8_t data);
extern void lcd_data(uint8_t data);
extern void lcd_puts(char *string);
extern void lcdInt(uint8_t value);
extern void lcd_clear(void);

#endif
