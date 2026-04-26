#define F_CPU 8000000UL
#define GARLAND_DIV 64

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

// порт A
uint8_t ba = 3; // 0b00000011 - горят PA0 и PA1
uint8_t cur_a = 3;
uint8_t ha = 2; // смещение на 2
uint8_t pa = 4; // каждые 4 тика сдвиг
uint8_t da = 0; //с 0 позиции

// порт B
uint8_t bb = 0x0F; //0x00001111 PB0, PB1, PB2, PB3 горят
uint8_t cur_b = 0x0F;
uint8_t hb = 1; //смещение на 1
volatile int pb = 1; //изменяемое значение скорости
uint8_t db = 0; //с 0 смещения

//порт C
uint8_t bc = 6;  // 0x00000110 PC1 и PC2
uint8_t cur_c = 6;
uint8_t hc = 1; // на один смещение
uint8_t pc = 6; // каждые 6 тиков
uint8_t dc = 1; // с 1 св

// переменные режима и индикатора
volatile int mode = 0; // 0 - работает, 1 - режим настройки
//volatile int cur_param = 0; // 0 - > pb, 1 -> db, 2 -> bb.
volatile unsigned int ind[4]= {0, 0, 0, 0};
volatile int cur_ind = 0; // 0-3

// переменные гирлянды
volatile uint8_t garland_tick = 0; //счетчик тиков до следующего шага 
volatile uint8_t count_a = 0, count_b = 0, count_c = 0; // счетчики шагов до следующего сдвига каждого порта 

// переменные АЦП
volatile unsigned int adc_value = 0;
volatile uint8_t adc_ready = 0;

// таблица символов
const unsigned char numb[] = {
	0b11000000, // 0  — индекс 0
	0b11111001, // 1  — индекс 1
	0b10100100, // 2  — индекс 2
	0b10110000, // 3  — индекс 3
	0b10011001, // 4  — индекс 4
	0b10010010, // 5  — индекс 5
	0b10000010, // 6  — индекс 6
	0b11111000, // 7  — индекс 7
	0b10000000, // 8  — индекс 8
	0b10010000, // 9  — индекс 9
	0b00001000, // a. — индекс 10
	0b00000011, // b. — индекс 11
	0b01000110, // c. — индекс 12
	0b10001100, // p  — индекс 13
	0b10001011, // h  — индекс 14
	0b10000011, // b  — индекс 15
	0b10100001, // d  — индекс 16
	0b10111111, // -  — индекс 17
	0b11111111  // ' '— индекс 18
};

void crcl(uint8_t *cur, int value){       // — циклический сдвиг (используется в init и таймере)
	// if (value < 0) *cur = ((*cur >> -value) | (*cur << (8 + value))) & 0xFF;
	*cur = ((*cur << value) | (*cur >> (8 - value))) & 0xFF;
}

void update_garland(void ){ // — вывод cur_a/b/c на порты
	PORTA = cur_a;
	PORTB = cur_b;
	PORTC = cur_c;
}

void init_garlands(){ // — применяет начальные смещения da/db/dc
	//for (int i = 0; i < da; i++) crcl(&cur_a, ha);
	//for (int j = 0; j < db; j++) crcl(&cur_b, hb);
	//for (int k = 0; k < dc; k++) crcl(&cur_c, hc);
	
	crcl(&cur_c, hc);
	update_garland();
}

void timer_init(void){    // — настройка таймеров T0, T1, T2
	TCCR0 = (1<<WGM01) | (1<<CS01) | (1<<CS00); // настраиваем режим СТС, 64
	OCR0 = 154; // до какого числа считать, частота прерываний 8000000 / (64 * 155) ~ 800 Гц
	TIMSK |= (1<<OCIE0); // разрешить прерывание
	
	//используем Timer1 в 8-битном режиме Fast-PWM.
	TCCR1A = (1<<COM1B1)|(1<<WGM10); // включаем ШИМ на ножке PD4 и неинвертированный режим, первый бит настройки режима
	TCCR1B = (1<<WGM12)|(1<<CS10); //второй бит настройки режима, делитель 1 таймер на макс скорости
	
	// регистр сквадности 0% скважности -> PD4 не горит
	OCR1B = 0; // начальное значение
	
	// используем Timer2 (ШИМ на PD7)
	TCCR2 = (1<<WGM21)|(1<<WGM20)|(1<<COM21)|(1<<COM20)|(1<<CS20);
	// регистр скважности
	OCR2 = 0;  // начальное значение
}

void ADC_init(void) {
	// что читаем и как
	ADMUX  = (1<<REFS0)|(1<<MUX2)|(1<<MUX0); //AVCC (5В),  MUX4:MUX0 = 00101 = 5 → канал ADC5 → ножка PA5
	//как работает АЦП
	ADCSRA = (1<<ADEN)|(1<<ADSC)|(1<<ADATE)|(1<<ADPS2)|(1<<ADPS1)|(1<<ADIE);
	//       включить    запуск  авто-режим       делитель/64      разрешить прерывание
}
void take_adc(void){      // — запуск одного замера
	ADCSRA |= (1<<ADSC); // запуск замера
	adc_ready = 0;       // сбрасываем флаг
}

ISR(ADC_vect){
	adc_value = ADC;
	adc_ready = 1;
}

ISR(TIMER0_COMP_vect) {
	//режим настройки
	if (mode == 1) {
		PORTA = 0x00;                     // 1. гасим ВСЕ индикаторы
		if (++cur_ind > 3) cur_ind = 0;   // 2. переходим к следующему
		PORTC = ~numb[ind[3 - cur_ind]];  // 3. устанавливаем символ
		PORTA = (1 << cur_ind);           // 4. включаем нужный индикатор
	}
	//режим гирлянды
	if (mode == 0) {
		garland_tick++; //считаем тики таймера
		if  (garland_tick >= GARLAND_DIV){// каждые 64 тика = один шаг гирлянды
			garland_tick = 0;
			if (++count_a >= pa){ // считаем до pa=4, потом сдвигаем на ha=2, сдвигаем каждые 4 тика
				count_a = 0;
				crcl(&cur_a, ha);
			}
			// 25 сек: 8000000 / (64 × (154+1)) = 806 Гц ≈ 800 раз/сек
			//800 / 64 = 12.5 тиков гирлянды в секунду
			// 12.5 тиков/сек × 2 сек = 25 тиков за 2 секунды
			if (++count_b >= (25/ pb)){ //считаем до (25/pb) потом сдвиаем на hb=1
				count_b = 0;
				crcl(&cur_b, hb);
			}
			if (++count_c >= pc) { // считаем до pc=6, потом сдвигаем на hc=1
				count_c = 0;
				crcl(&cur_c, hc);
			}
			update_garland(); //выводим значения
		}
	}
}
// вызывается когда нажимается кнопка pd2, переключатель между режимами.
ISR(INT0_vect) {
	if (mode == 0) { //режим гирлянды
		mode = 1;         //переключаем режим
		//cur_param = 0;    //начинаем с первого параметра pb
		PORTA = 0x00;    //гасим все светодиоды гирлянды
		PORTB = 0x00;        // гасим
		PORTC = 0x00;        // гасим
		ind[0]=13; ind[1]=11; ind[2]=18; ind[3]=pb; // pb. (1..8)
	}
	else {
		mode = 0;
		cur_b = bb;
		init_garlands();
	}
}


int main(){
	timer_init(); // настраиваем 3 таймера
	ADC_init(); // настраиваем АЦП
	
	DDRA = 0xDF; //// PA5 вход (потенциометр), остальные выходы
	DDRB = DDRC = 0xFF; // все выходы (гирлянда)
	DDRD = 0x90; // PD7 и PD4 выходы (ШИМ), PD2 PD3 входы (кнопки)
	
	MCUCR = (1<<ISC11)|(1<<ISC10)|(1<<ISC01)|(1<<ISC00);
	GICR  = (1<<INT1)|(1<<INT0);  // разрешить INT0 и INT1
	
	init_garlands(); //начальные смещения
	sei(); //разрешаем все прерывания глобально
	
	while(1) {
		//режим настройки
		if (mode == 1) {
			if (adc_ready) { //проверяем готова или нет
				adc_ready = 0; //сбрасываем
				
				// усреднение
				static unsigned int adc_sum = 0; // складываем
				static uint8_t adc_count = 0; // сколько замеров, копим 16
				
				adc_sum += adc_value;
				adc_count++;
				
				if (adc_count >= 16) {
					unsigned int adc_avg = adc_sum / 16; //усредняем
					adc_sum = 0;
					adc_count = 0;
					
					uint8_t brightness = (uint8_t)(adc_avg >> 2); //тк АЦП дает число от 0 до 1023 (10бит)
					// ОCR принимает число от 0 до 255, нужно преобразовать
					// >> 2 = сдвиг вправо на 2 = деление на 4
					OCR2  = brightness; // PD7
					OCR1B = brightness; // PD4
					
					pb = adc_avg * 8 / 1024 + 1;
					if (pb > 8) pb = 8;
					ind[0]=13;
					ind[1]=11;
					ind[2]=18;
					ind[3]=pb;
					
				}
			}
		}
	}
}