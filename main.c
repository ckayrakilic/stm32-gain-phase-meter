#include <stm32f10x.h>
#include "lcd.h"
#include <math.h>

//PA0-PA7 (LCD data) and PA8-10 (RS, RW, EN)

#define N 2048

volatile uint32_t buf[N]; // ADC1 in low 16 bits, ADC2 in high 16 bits
volatile uint16_t cnt = 0;

#define X(i) ((int)(buf[i] & 0xFFFF)) // input,  ADC1
#define Y(i) ((int)(buf[i] >> 16)) // output, ADC2
	
void adc_init(void);
void ADC1_2_IRQHandler(void);
void print_sdecimal(int32_t x);
void mean(int *mx, int *my);
void find_peaks(int *pkx, int *pky, int mx, int my);

int find_phase(int my, int first, float period, int *phase_d);
int find_period(int mx, float *period, int *first);
	
int main() {

	RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_ADC1EN | RCC_APB2ENR_ADC2EN; //enable GPIOA,B and ADC1,2
	RCC->APB1ENR |= RCC_APB1ENR_TIM4EN; //enable TIM4
	RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_ADCPRE) | RCC_CFGR_ADCPRE_DIV6; // ADC clock 12 MHz

	GPIOB->CRL &= ~((0xFu << 0) | (0xFu << 4)); // PB0, PB1 analog inputs
	
	
	GPIOA->CRL = 0x33333333; //set push-pull output
	GPIOA->CRH = (GPIOA->CRH & ~0x00000FFFu) | 0x00000333u; 


	//free running 1MHz TIM4
	TIM4->PSC = 71; //72MHz/72=1MHz
	TIM4->ARR = 0xFFFF;
	TIM4->EGR = TIM_EGR_UG;
	TIM4->CR1 |= TIM_CR1_CEN;
	
	lcd_init();
	print_str("Setting up");
	adc_init();
	
	ADC1->CR2 |= ADC_CR2_SWSTART;
	
	while (cnt < N) {}
	
	clear_screen();
	while (1) {
		cnt = 0;
		ADC1->CR1 |= ADC_CR1_EOCIE; // turn on interrupt
		while (cnt < N) {}
	
		//take mean
		int mx = 0, my = 0;
		mean(&mx, &my);	//find means
			
		int pkx = 0, pky = 0;
		find_peaks(&pkx, &pky, mx, my);

		movcursor(0, 0);
		if (pkx < 100 || pky < 20) { //to avoid division by zero and noise
			print_str("No signal       ");
			movcursor(0, 1);
			print_str("                ");
		} 
		else {
			float gain = (float)pky / (float)pkx;
			
			//round and multiply by 200 instead of 20 since print_sdecimal uses it to give decibel in one decimal digit.
			int gain_db10 = (int)floorf(200.0f * log10f(gain) + 0.5f); 
			
			
			float period; 
			int first, phase10;
			if (find_period(mx, &period, &first) && find_phase(my, first, period, &phase10)) {
				print_str("G:"); 
				print_sdecimal(gain_db10); 
				print_str("dB");
				print_str("   ");
				
				movcursor(0,1);
				print_str("P:"); 
				print_sdecimal(phase10); 
				print_str("  ");
			}
			
			else {
				print_str("No cycles       ");
				movcursor(0, 1);
				print_str("                ");
			}
		}
		for (int k = 0; k < 6; k++) {
			delay_us(64000);
		}
	}
};

void adc_init(void){
	//first wake up ADCs
	ADC1->CR2 |= ADC_CR2_ADON; 
	ADC2->CR2 |= ADC_CR2_ADON;
	
	//configure continuous mode, and enable external trigger.
	ADC1->CR2 |= ADC_CR2_EXTTRIG | ADC_CR2_CONT;
	ADC2->CR2 |= ADC_CR2_EXTTRIG | ADC_CR2_CONT;
	
	//select one conversion.
	ADC1->SQR1 = 0;
	ADC2->SQR1 = 0;
 
	// ch8 = PB0, ch9 = PB1	
  ADC1->SQR3 = 8;
	ADC2->SQR3 = 9;
  
	//set conversion time 239.5 cycles for better accuracy.
	ADC1->SMPR2 = 7 << 24;
	ADC2->SMPR2 = 7 << 27;
	
	//set dual simultaneous and enable EOC interrupt
	ADC1->CR1 = (6 << 16) | ADC_CR1_EOCIE;
	
	//set dual channel mode, in simulatenous sampling. ADC1 acts as master.
	ADC1->CR2 = ADC_CR2_CONT | ADC_CR2_EXTSEL | ADC_CR2_EXTTRIG;
	ADC2->CR2 = ADC_CR2_CONT | ADC_CR2_EXTSEL | ADC_CR2_EXTTRIG;
	
	//power on ADCs
	ADC1->CR2 |= ADC_CR2_ADON;
	ADC2->CR2 |= ADC_CR2_ADON;
  delay_us(10);
	
	ADC1->CR2 |= ADC_CR2_CAL;
	while (ADC1->CR2 & ADC_CR2_CAL) {};
	
	ADC2->CR2 |= ADC_CR2_CAL;
	while (ADC2->CR2 & ADC_CR2_CAL) {};

  NVIC_EnableIRQ(ADC1_2_IRQn);
}

void ADC1_2_IRQHandler(void)
{
    //uint32_t w = ADC1->DR; //read to clear EOC
    if (cnt < N){
		
			buf[cnt++] = (ADC1->DR & 0xFFFF) | (ADC2->DR << 16);
		}
    else {
			ADC1->CR1 &= ~ADC_CR1_EOCIE; //once buffer is filled, stop interrupting
		}
}

void print_sdecimal(int32_t x)
{
    if (x < 0) { 
			lcd_write('-', 1); 
			x = -x; 
		}
		
    print_number(x / 10);
    lcd_write('.', 1);
    lcd_write('0' + (x % 10), 1);
}

void mean(int *mx, int *my){
	long sumx = 0, sumy = 0;
	for (int i = 0; i < N; i++) 
	{ 
		sumx += X(i);
		sumy += Y(i);
	}
	*mx = sumx / N;
	*my = sumy / N;
};

void find_peaks(int *pkx, int *pky, int mx, int my) {
	int d = 0;
	for (int i = 0; i < N; i++) {
		d = X(i) - mx;  
		if (d < 0) {
			d = -d; 
		}
		if (d > *pkx) {
			*pkx = d;
		}
		
		d = Y(i) - my;  
		if (d < 0) {
			d = -d;  
		}
		if (d > *pky) {
			*pky = d;
		};
	}
	
}

int find_period(int mx, float *period, int *first)
{
	int count = 0, last = -1;
	*first = -1;
	for (int i = 1; i < N; i++) {
		if (X(i-1) < mx && X(i) >= mx) {
			if (*first < 0) {
				*first = i;
			}
			last = i;
			count++;
		}
	}
	if (count < 2) {
		return 0; // not enough cycles
	}
	
	*period = (float)(last - *first) / (float)(count - 1);
	return 1;
}

int find_phase(int my, int first, float period, int *phase_d)
{
	int fy = -1;
	for (int i = first; i < N; i++) {
		if (Y(i-1) < my && Y(i) >= my) {
			fy = i; break; 
		}
	}
	
	if (fy < 0) 
		return 0;
	
	float delay = (float)(fy - first);
	if (delay > period / 2) {
		delay -= period;
	}
	
	*phase_d = (int)floorf(-3600.0f * delay / period + 0.5f); //scaled by 10 to keep one decimal point
	
	return 1;
}

