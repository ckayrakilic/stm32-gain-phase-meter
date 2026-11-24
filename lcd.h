#include <stm32f10x.h>


#ifndef lcd_driver
#define lcd_driver

void lcd_init(void);
void movcursor(unsigned char x, unsigned char y);
void print_str(char *str);
void clear_screen(void);
void lcd_write(unsigned char val, uint8_t rs);
void print_number(int32_t num);
void delay_us(uint16_t t);

#endif
