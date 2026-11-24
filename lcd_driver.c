#include <stm32f10x.h>

#define RS 8 //A8
#define RW 9 //A9
#define EN 10 //A10

void delay_us(uint16_t t)
{
	uint16_t t0 = TIM4->CNT;
	while ((uint16_t)(TIM4->CNT - t0) < t) 
	{}
}

void lcd_write(unsigned char val, uint8_t rs)	
{
	if (rs)
	{
		GPIOA->BSRR = (1<<RS);
	}
	else
	{
		GPIOA->BRR = (1<<RS);
	}
	
	GPIOA->BSRR = (0xFFu << 16) | val; // D0-D7: PA0-PA7
	
	GPIOA->BSRR = (1<<EN); // send data/command by enabling, holding, disabling, waiting.
	delay_us(2);
	GPIOA->BRR = (1<<EN);
	delay_us(50);
	
}

void print_number(int32_t num)
{
	char buf[12];
	int i = 0;
	int32_t n;
	
	if (num < 0)
	{
		lcd_write('-', 1);
		n = -num;
	}

	else if (num == 0)
	{
		lcd_write('0', 1);
		return;
	}
	else
	{
		n = num;
	}
	
	while (n>0)
	{
		buf[i++] = '0' + (n % 10);
		n/= 10;
	}
	
	while (i>0)
	{
		i--;
		lcd_write(buf[i], 1);
	}
}

void clear_screen()
{
	lcd_write(0x01, 0);
	delay_us(3000);
}	

void movcursor(unsigned char x, unsigned char y) //move cursor to an address
{
	const unsigned char first_adr[] ={0x80, 0xC0};
	lcd_write((first_adr[y] + x), 0);
	delay_us(100);
}

void print_str(char *str) //print string using pointers
{
	while (*str)
	{
		lcd_write(*str, 1);
		str++;
	}
	
}


void lcd_init() //initialize the lcd
{
	GPIOA->BRR = (1<<EN); 
	GPIOA->BRR = (1<<RW);
	delay_us(60000);	
	lcd_write(0x38, 0);	
	delay_us(50);
	lcd_write(0x0C, 0); // display on, cursor off, blink off	 
	delay_us(50);
	lcd_write(0x01, 0);  
	delay_us(50);
	delay_us(2000);
	lcd_write(0x06, 0);  
	delay_us(6000);
}


