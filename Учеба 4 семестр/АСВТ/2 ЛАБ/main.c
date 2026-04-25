#define F_CPU 8000000UL // частота МК = 8 МГц (для delay)

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>

#define TIMER_TICKS_HALF_PERIOD  200 //200 тиков таймера = 0.25сек = 2Гц
#define EEPROM_MODE_ADDR  ((uint8_t *)0x00) //адрес в EEPROM где хранится режим 

volatile uint8_t mode  = 1; //текущий режим гирлянды 1 или 0
volatile uint8_t state = 0; // текущее состояние 0 или 1 для PD4 
volatile uint8_t y     = 0x55; // значение у для режима 3 
volatile uint16_t tick_count = 0; // счетчик тиков таймера до 200 
volatile uint8_t need_update = 0; // флаг: обновить PORTD 
volatile uint8_t need_eeprom_save = 0; // флаг: сохранить режим EEPROM 

//возвращает байт для вывода на PORTA/PORTB в зависимости от режима и состояния 
uint8_t get_value(void)
{
	switch (mode) { //проверяем текущий режим 
		case 1:
		return (state == 0) ? 0xFF : 0x00; //все горят все негорят 
		case 2:
		return (state == 0) ? 0xAA : 0x55; //четные нечетные 
		case 3:
		default:
		/* прямой код: инвертируем только знаковый бит y / -y в прямом коде */
		return (state == 0) ? y : (y ^ 0x80);
	}
}

//обновляем порты А и В - гирлянда и порт D - индикация режима и состояния 
void update_output(void)
{
	uint8_t val = get_value(); //получаем значение для текущего режима 
	PORTA = val; //выводим гирлянду А
	PORTB = val; //В

	uint8_t pd = PORTD; //читаем порт Д 
	pd &= ~((1<<PD0)|(1<<PD1)|(1<<PD4)); // очищаем биты PD0, PD1, PD4
	pd |= (mode & 0x03); // записываем номер режима в PD0, PD1
	if (state) pd |= (1<<PD4); // PD4 = 1 если состояние 1
	PORTD = pd; // выводим обновлённое значение
}

// сохраняем текущий режим в EEPROM 
void eeprom_save_mode(void)
{
	eeprom_update_byte(EEPROM_MODE_ADDR, mode); // пишем только если значение изменилось 
}

// при страрте загружаем режим из EEPROM 
uint8_t eeprom_load_mode(void)
{
	uint8_t m = eeprom_read_byte(EEPROM_MODE_ADDR); //читаем байт из EEPROM 
	if (m < 1 || m > 3) m = 1; // если мусор - > режим 1 
	return m; // возвращаем загруженный режим 
}

//иниц таймера0 в режиме СТС с частотой 806ГЦ
void timer0_init(void)
{
	TCCR0 = (1<<WGM01) | (1<<CS01) | (1<<CS00); //СТС режиим, /64 
	OCR0  = 154; // 8 МГц / 64 *155 = 806 Гц
	TIMSK |= (1<<OCIE0); //разрешить прерывание по совпадению 
}

// прерывание таймера0 - срабатывает 806 раз в секунду 
ISR(TIMER0_COMP_vect)
{
	tick_count++; //считаем тики 
	if (tick_count >= TIMER_TICKS_HALF_PERIOD) { //прошло 200 тиков = 0.25с?
		tick_count = 0; // сбрасываем ткики 
		state ^= 1; // переключаем состояние из 0 в 1

		uint8_t val = get_value(); // получаем значение для гирлянды
		PORTA = val; //обновляем гирлядну
		PORTB = val; //тоже 

		if (state) PORTD |=  (1<<PD4); // PD4 = 1 если состояние 1 
		else       PORTD &= ~(1<<PD4); // PD4 = 0 если состояние 0
	}
}


//прерывание INT0 - нажата кнопка PD2, режим вперед 
ISR(INT0_vect)
{
	mode++; //увеличиваем режим 
	if (mode > 3) mode = 1; // после 3 сразу 1 
	need_eeprom_save = 1; // попросить мейн сохранить  в EEPROM 
	need_update = 1; // попросить обновить PORTD 
}
 
// прерывание IN1 - нажата кнока PD2, режим вперед 
ISR(INT1_vect)
{
	if (mode <= 1) mode = 3; // если режим 1 то преейти в 3 
	else           mode--; // уиеншить на 1
	need_eeprom_save = 1; // попросить мейн сохранить  в EEPROM 
	need_update = 1; //попросить обновить PORTD 
}

// считываем новое значение у с кнопок PORTC пока зажата PD7 
void read_y_from_portc(void)
{
	_delay_ms(20); // ждем окончания дреюезга кноки PD7 

	while ((PINC & 0xFF) == 0) { // ждем нажатия хоть одной кнопки PORTC 
		if (!(PIND & (1<<PD7))) return; // ждем отпускание PD7 - выходим без изменений 
	}

	_delay_ms(120); //ждем пока нажмут все нужные кнопки 
	y = PINC; //считываем комбинацию кнопок как новый у 

	while ((PINC & 0xFF) != 0 && (PIND & (1<<PD7))); // ждем отпускание кнопки PORTC или PD7 

	_delay_ms(20); //ждем окончания дребезга при отпускании 
	need_update = 1; // обновить отображение с новым у 
}

int main(void)
{
	DDRA  = 0xFF; //выход 
	DDRB  = 0xFF; //выход 
	DDRC  = 0x00; //вход 
	PORTC = 0x00; // pull-yp не вкл

	DDRD  = (1<<PD0)|(1<<PD1)|(1<<PD4); // выходы остальные входы 
	PORTD = 0x00; // пул ап не вкл 

	mode = eeprom_load_mode(); // загружаем сохраненый режим из eeprom 
	state = 0; // начальное состояние 0
	y = 0x55; // начальное 0х055 по заданию 

	MCUCR = (1<<ISC11)|(1<<ISC10)|(1<<ISC01)|(1<<ISC00);
	GICR  = (1<<INT1)|(1<<INT0); // разрешить прерывания int0 и int1
	GIFR  = (1<<INTF1)|(1<<INTF0); // сьросить флаги чтобы не сработало сарзу 

	timer0_init(); // запускаем таймер0 
	update_output(); // выводим начальное состояние на порты 
	sei(); // разрешаем прерывания глобально 

	while (1) //беск
	{
		if (PIND & (1<<PD7)) // нажата кнопка 7 
		{
			TIMSK &= ~(1<<OCIE0); // останавливаем таймер 
			PORTA  = 0x00; //гасим 
			PORTB  = 0x00; //гасим 

			read_y_from_portc(); //считываем новое у с portc 

			while (PIND & (1<<PD7)); // ждем отпускание PD7 
			_delay_ms(20); // дребезг перед отпусканием 

			tick_count = 0; // сбрасываем счетчик таймера 
			TIMSK |= (1<<OCIE0); // запускаем таймер снова 
			update_output(); //выводим гирлянду с новым у 
		}

		if (need_update) // нуждно обновить порт д? 
		{
			need_update = 0; // сбрасываем флаг 
			uint8_t pd = PORTD; // читаем текущий порт д 
			pd &= ~((1<<PD0)|(1<<PD1)|(1<<PD4)); // очищаем PD0 PD1 PD4 
			pd |= (mode & 0x03); // щаписываем новый редим в PD0 PD1 
			if (state) pd |= (1<<PD4); // PD4 = текущее состояние
			PORTD = pd; // выводим обновлённое значение
		}

		if (need_eeprom_save) // нужно сохранить режим в EEPROM?
		{
			need_eeprom_save = 0; // сбрасываем флаг

			eeprom_save_mode();// сохраняем режим в EEPROM
		}
	}

	return 0;
}