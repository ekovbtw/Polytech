.include "m32def.inc"

;векторы прерываний 
.org $000
    JMP RESET
.org INT0addr
    JMP EXT_INT0
.org INT1addr
    JMP EXT_INT1
.org OVF0addr
    JMP TIMER0_OVF
.org $012
    JMP TIM1_OVF
.org 0x0040

numbers:  .db 192, 249, 164, 176, 153, 146, 130, 248, 128, 144
;numbers: .db 63, 6, 91, 79, 102, 109, 125, 7, 127, 111 ; 0-9

limit: .db 9, 5, 9, 5

.DSEG            ;след переменные разместить в SRAM 
time: .BYTE 4    ; время [S1, S10, M1, M10]

.CSEG                  ;след размещать в памяти как машинные команды
.def temp = R21        ;временный регистр
.def NULL = R22        ;для хранения 0
.def out_counter = R23 ; номер текущего выводимого разряда
.def max = R27         ;макс допустимое значение текущ разряда 5/9

.def FLAGS = R25       ;регистр флагов состояния программы
;каждый бит отдельное состояние: 0 - режим 0.1с, 1 - режим 0.2с
;2 - удерживание кнопки UP, 3 - удерживание кнопки DOWN
;4 - режим настройки, 5 - достигнут 00.00, 6 - режим пусто (мигание)

.def counter = R18     ;счетчик для удержания кнопки 
.def disp_cnt = R26    ; текущ номер индикатора 0-3

RESET:
	;настройка стека
    LDI R20, HIGH(RAMEND) ; старшая часть адреса конца памяти 
    OUT SPH, R20          ;записываем в верхнюю часть указателя стека
    LDI R20, LOW(RAMEND)  ;младшая часть адреса конца памяти 
    OUT SPL, R20          ;записываем в нижнюю часть указателя стека


	;настройка портов А и С на выход 
    SER R16            ;заполнить регистр единицами
    OUT DDRA, R16      ; для включения нужного индикатора
    OUT DDRC, R16      ; для управления сегментами индикатора 

    CLR R16 ;обнуляем
    OUT DDRD, R16      ; вход, кнопки

    ; настройка внешних прерываний INT0 и INT1
    LDI R16, 0x0F                  ; режим срабатывания INT0/INT1
    OUT MCUCR, R16                 ; запись в MCUCR - регистр управлени мк 
    LDI R16, 0xC0                  ; разрешить INT0 и INT1
    OUT GICR, R16                  ; запись в регистр разрешения прерываний 

    ; Timer1 - логика времени
    LDI R16, 0b00000100
    OUT TCCR1B, R16 ;ргеситр управления, предделитель 256

    ; Timer0 - динамическая индикация, предделитель 8
    LDI R16, 0b00000010
    OUT TCCR0, R16

    ; Разрешаем прерывания Timer0 
    LDI R16, 0b00000001 ;TOIE0 = 1 
    OUT TIMSK, R16

	;начальные значения 
    CLR NULL        ; для использования регистра как 0
    LDI FLAGS, 0x00 ;стартуем в чистом состоянии, сбрасываем все флаги
    CLR disp_cnt    ;0 - индикация начинается с 1 разряда

    ; Инициализация времени 
    LDI ZH, HIGH(time)  ;указатель Z ставим на массив time
    LDI ZL, LOW(time)
    LDI temp, 1
    ST Z+, temp ; 0 сек
    LDI temp, 1
    ST Z+, temp ; 30 сек
    LDI temp, 1
    ST Z+, temp ; 1 мин
    LDI temp, 1
    ST Z, temp  ; 00 мин

    SEI ;глобально разрешить прерывания

LOOP:
    ; Режим настройки обрабатывается здесь
    SBRC FLAGS, 4 ;если режим настройки включен 
    CALL CHECK_PIND ;опросить кнокпи PD0,PD1
    RJMP LOOP ;бесконечный цикл

TIMER0_OVF:
	PUSH R16                       ; сохранить R16
    IN R16, SREG                   ; читаем регистр состяония 
    PUSH R16                       ; сохранить его
    PUSH R17                       ; сохранить R17
    PUSH R19                       ; сохранить R19
    PUSH ZL                        ; сохранить ZL
    PUSH ZH                        ; сохранить ZH

    ; выключаем все индикаторы перед переключением
    OUT PORTA, NULL

    ; берем номер текущего индикатора, 0-3
    MOV out_counter, disp_cnt
    
    ;выбор разряда, какой индикатор чейчас включать 
    CPI out_counter, 0
    BREQ DIGIT0 ; если 0 то первый индикатор 
    CPI out_counter, 1 
    BREQ DIGIT1 ; если 1 то второй индикатор
    CPI out_counter, 2
    BREQ DIGIT2 ; есои 2 то третий индикатор
    LDI R19, 0b00001000            ; иначе это четвертый
    RJMP PREP_OUTPUT

DIGIT0: LDI R19, 0b00000001          ; маска первого инд
    RJMP PREP_OUTPUT
DIGIT1: LDI R19, 0b00000010          ; маска второго инд
    RJMP PREP_OUTPUT
DIGIT2: LDI R19, 0b00000100          ; маска тертьяег

PREP_OUTPUT:
    SBRC FLAGS, 6                  ; если режим "пусто"
    RJMP SHOW_BLANK                   ; показать пусто

    ; взять цифру из time
    LDI ZH, HIGH(time)
    LDI ZL, LOW(time)
    ADD ZL, out_counter            ; смещение на нужный разряд
    ADC ZH, NULL
    LD R17, Z                      ; R17 = цифра

    ; перевести цифру в сегменты
    LDI ZH, HIGH(numbers*2)
    LDI ZL, LOW(numbers*2)
    ADD ZL, R17                    ; индекс в таблице numbers
    ADC ZH, NULL
    LPM R17, Z                     ; R17 = код сегментов
    RJMP ADD_DOT

SHOW_BLANK:
    CLR R17                        ; ничего не показывать
    RJMP SHOW_DIGIT

;точка
ADD_DOT:
    CPI out_counter, 2             ; если это нужный разряд
    BRNE SHOW_DIGIT
    ORI R17, 0x80                  ; добавить точку

SHOW_DIGIT:
    OUT PORTC, R17                 ; вывести сегменты
    OUT PORTA, R19                 ; включить нужный разряд

T0_END:
    INC disp_cnt                   ; следующий разряд
    ANDI disp_cnt, 0x03            ; цикл 0..3
    POP ZH                         ; восстановить ZH
    POP ZL                         ; восстановить ZL
    POP R19                        ; восстановить R19
    POP R17                        ; восстановить R17
    POP R16                        ; вернуть старый SREG
    OUT SREG, R16                  ; возвращаем его
    POP R16                        ; восстановить R16
    RETI                           ; выход из прерывания

;TIMER1 - ЛОГИКА ТАЙМЕРА
TIM1_OVF:
    PUSH R16
    IN R16, SREG
    PUSH R16
    PUSH R17
    PUSH R24
    PUSH R27
    PUSH ZL
    PUSH ZH

    SBRC FLAGS, 4 ; если режим настройки вклбчен 
    RJMP T1_SPEED ; идем в обработку удержания кнопок
    SBRC FLAGS, 5 ; если режим нуля включен
    RJMP T1_FLASH ; идем в мигание

    ; обычный режим: уменьшаем время
    LDI R29, 0
    CALL DECREASE_TIME ;--
    CALL CHECK_ZERO_ALL ;проверка не стало ли 00.00

    SBRC FLAGS, 5 ;если теперь уже ноль
    RJMP T1_2HZ ;перейти на мигание 
    RJMP T1_1SEC ; иначе снова ждать 1 сек


TIM1_OVF_SUB_2HG: ;2 Гц
   LDI R16, 0xC2  ;значение для 2 Гц
   OUT TCNT1H, R16 ;старший байт 1тайм
   LDI R16, 0xF6
   OUT TCNT1L, R16 ;младший
   RJMP T1_EXIT    


T1_FLASH:
   SBRC FLAGS, 6      ; если сейчас уже пусто
   RJMP FLASHING_SUB  ; вернуть цифры обратно

   SBR FLAGS, 0b01000000 ; иначе включить режим пусто
   CLR R19
   OUT PORTA, R19     ; погасить разряды
   RJMP TIM1_OVF_SUB_2HG


FLASHING_SUB:
   CBR FLAGS, 0b01000000 ;убрать режим пусто
   RJMP TIM1_OVF_SUB_2HG    

T1_SPEED:
    INC counter ;увеличить счетчик удержания

    SBRS FLAGS, 1 ;если еще нет режима 0.2с
    RJMP T1_SPEED_02

    CALL TIME_CHANGE ;менять время
    SBRS FLAGS, 0    ;если еще нет режима 0.1с
    RJMP T1_SPEED_01
    RJMP T1_01SEC ; уже режим 0.1с

T1_SPEED_02:
    CPI counter, 2 ;счетчик дошел до порога?
    BREQ T1_02SEC  ; перейти на 0.2 с
    RJMP T1_1SEC ;иначе пока езе 1с

T1_SPEED_01:
    CPI counter, 10 ;дошли до следующего порога:
    BREQ T1_01SEC ; перейти на 0.1с
    RJMP T1_02SEC ; иначе пока 0.2с

T1_1SEC:
    LDI R16, 0x85 ;значение для 1 секунды
    OUT TCNT1H, R16
    LDI R16, 0xED
    OUT TCNT1L, R16
    RJMP T1_EXIT

T1_02SEC:
    SBR FLAGS, 0b00000010 ;запомнить: режим 0.2с
    LDI R16, 0xE7 ;значение для 0.2с
    OUT TCNT1H, R16
    LDI R16, 0x95
    OUT TCNT1L, R16
    RJMP T1_EXIT

T1_01SEC:
    SBR FLAGS, 0b00000001 ;запомнить режим 0.1с
    LDI R16, 0xF3 ;значение для 0.1с
    OUT TCNT1H, R16
    LDI R16, 0xCA
    OUT TCNT1L, R16
    RJMP T1_EXIT

T1_2HZ:
    LDI R16, 0xC2 ;значение для мигания 2 Гц
    OUT TCNT1H, R16
    LDI R16, 0xF6
    OUT TCNT1L, R16
    RJMP T1_EXIT

T1_EXIT:
    POP ZH
    POP ZL
    POP R27
    POP R24
    POP R17
    POP R16
    OUT SREG, R16 ;вернуть срег
    POP R16
    RETI  ;выйти из прерывания


CHECK_PIND:
    IN temp, PIND ;прочитать кнопки из портД

    SBRC temp, 0 ;если нажата кнопка PD0
    RJMP UP_SET ;идти на увеличение

    SBRC temp, 1 ; если нажата кнопка PD1
    RJMP DOWN_SET ;идти на уменьшение 

    CBR FLAGS, 0b00001111 ; Кнопки отпущены, убрать служебные флаги 
    CALL TIMER_STOP ;остановить таймер
    RET

UP_SET:
    SBRC FLAGS, 2 ;если удержание уже отмечено
    RET ;не делать повторно

    LDI R29, 0 ; устанавливаем регистр в 0, для увел/умен с секунд
    CALL INCREASE_TIME ;сразу увеличить время
    CLR counter ;обнулить счетчик удержания 
    SBR FLAGS, 0b00000100 ;отметить удержание кнопки вверх
    CALL TIMER_START ;включить таймер1 для автопвтора
    RET

DOWN_SET:
    SBRC FLAGS, 3 ;если удержание уже отмечено
    RET ;не делать повторно

    LDI R29, 0 ; устанавливаем регистр в 0, для увел/умен с секунд
    CALL DECREASE_TIME ;сразу уменьшить время 
    CLR counter  ;обнулить счетчик удержания
    SBR FLAGS, 0b00001000 ; отметить удержание кнопки вниз
    CALL TIMER_START ; включить Timer1 для автоповтора
    RET

TIME_CHANGE:
    SBRC FLAGS, 2 ;если удерживается кнопка вверх
    RJMP INCREASE_TIME ;увеличить время

    SBRC FLAGS, 3 ;если удерживается кнопка вниз
    RJMP DECREASE_TIME ;уменьшить время
    RET

DECREASE_TIME: ;если вкл режим нуля, ничего не уменьшать
SBRC FLAGS,5;
RET;

PUSH R16 ;сохранить 
    LDI ZL, LOW(time) ; адресс массива 
    LDI ZH, HIGH(time)
    LD R16, Z+ ; взять 1 байт
    LD R17, Z+ ;взять 2 байт
    OR R16, R17 ;об\единить
    LD R17, Z+ ; взять 3 байт
    OR R16, R17 ;обьединить
    LD R17, Z ;взять 4 байт
    OR R16, R17 ;если все нули, получится 0
    POP R16 ;вернуть 
    BREQ DT_RET        ; Если уже нули, выходим 

    LDI R29, 0         ; Начинаем вычитание с младшего разряда


DT_LOOP:
    CPI R29, 4 ;если все 4 разряда просмотрены
    BREQ DT_RET ;выйти

    LDI ZH, HIGH(time)
    LDI ZL, LOW(time)
    ADD ZL, R29 ;перейти к нужному разряду
    LD R24, Z; взять его значение 
    
    TST R24            ; он ноль?
    BRNE DT_DEC        ; если не ноль просто уменьшить
    

    PUSH ZL
    PUSH ZH
    LDI ZH, HIGH(limit*2) ;таблица максимуммов 
    LDI ZL, LOW(limit*2) 
    ADD ZL, R29 ;взять максимум жля этого разряда
    LPM max, Z
    POP ZH
    POP ZL
    ST Z, max ;записать максимум
    INC R29 ; перейти к следующему страшему разряду
    RJMP DT_LOOP

DT_DEC:
    DEC R24 ;уменьшить значение разряда
    ST Z, R24 ;записать обратно

DT_RET: RET

INCREASE_TIME:
    SBRC FLAGS, 5      ; Если режим нуля включен, не даем увеличивать
    RET
    
    LDI R29, 0         ; Начинаем с младшего разряда

IT_LOOP:
    CPI R29, 4
    BREQ IT_RET        ; Если прошли все 4 разряда, выходим
    
    ; Загружаем максимум для текущего разряда
    LDI ZH, HIGH(limit*2)
    LDI ZL, LOW(limit*2)
    ADD ZL, R29 ;выбрать максимум для текущего разряда
    LPM max, Z ;прочитать его
    
    ; Загружаем текущее значение разряда
    LDI ZH, HIGH(time)
    LDI ZL, LOW(time)
    ADD ZL, R29 ;перейти к нужному разряду
    LD R24, Z ;прочитать текущее значение
    
    CP R24, max        ; Сравниваем с максимумом
    BRNE IT_INC        ; Если меньше максимума — просто прибавляем 1
    
    ; Если равен максимуму
    ST Z, NULL         ; Обнуляем текущий разряд
    INC R29            ; Переходим к следующему разрядк
    RJMP IT_LOOP       ; повторить 

IT_INC:
    INC R24 ;увеличить значение разряда
    ST Z, R24 ; записать обратно

IT_RET: RET

CHECK_ZERO_ALL:
    LDI ZL, LOW(time)
    LDI ZH, HIGH(time)
    LD R16, Z+ ; 1 разряд
    OR R16, R16
    LD R17, Z+ ; 2 разряд
    OR R16, R17
    LD R17, Z+ ; 3
    OR R16, R17
    LD R17, Z ; 4
    OR R16, R17
    BRNE CZ_RET ; если не ноль, выйти
    SBR FLAGS, 0b00100000 ; Установить режим нуля
    
	LDI R16, 0xC2
    OUT TCNT1H, R16 ;загрузить атймер1 для мигания
    LDI R16, 0xF6
    OUT TCNT1L, R16
    
    ; Убеждаемся, что прерывания Timer1 включены
    IN R16, TIMSK ;прочитаем 
    ORI R16, (1<<TOIE1) ;разрешить 
    OUT TIMSK, R16
	
	
	CALL TIMER_START      ; на всякий вкл таймер 1

CZ_RET: RET

TIMER_START:
    IN R16, TIMSK ;прочитать
    ORI R16, (1<<TOIE1) ; включить бит TOIE1
    OUT TIMSK, R16 ; таймер1 разрешен 
    RET

TIMER_STOP:
    IN R16, TIMSK ;прочитать 
    ANDI R16, ~(1<<TOIE1) ;убрать бит TOIE1
    OUT TIMSK, R16 ;запрещен таймер1
    RET

; ВНЕШНИЕ ПРЕРЫВАНИЯ
;обработчик кнопки: старт, пауза, возобновление
EXT_INT0:
    PUSH R16 ; сохраняем регистр в стек
    IN R16, SREG ;взять срег
    PUSH R16 ;сохраняем 

    SBRC FLAGS, 4 ;если сейчас режим настройки 
    RJMP E0_EXIT ; не реагировать на кнопку
	 
    IN R16, TIMSK ; прочитать
    SBRC R16, TOIE1 ; если таймер 1 уже работает
    RJMP E0_STOP ; остановить егл

    CALL TIMER_START ; иначе запустить таймер1
    RJMP E0_EXIT ;выход из обработчика

;если таймер уже работал ставит его на паузу
E0_STOP:
    CALL TIMER_STOP ;остановить таймер1

E0_EXIT:
    POP R16 ;достать старый срег
    OUT SREG, R16 ;возвращаем SREG в исходное состояние 
    POP R16 ;восстанавливаем 
    RETI ;выход из прерывания 

;обработчик прерывания, кнопки: вход выход из режима настройки 
EXT_INT1:
    PUSH R16 ;сохранить р16 в стек
    IN R16, SREG ;взять срег (регистр состояния процессора)
    PUSH R16 ; сохранить срег

    CBR FLAGS, 0b01100000 ; сброс мигания и режим нуля, 5бит - режим нуля, 6 - мигание или пустое отобр
	;нужно для того, чтобы когда таймер дошел до 0 и экран мигает
	; а кнока перехода в настройку нажата, то программа должна выйти из состояния мигания

    LDI R16, 0b00010000 ; маска бита режима настройки, если 4 бит = 0 - обычный режим, если 1 - настройки
    EOR FLAGS, R16        ; переключить режим настройки
	;когда нажимаем кнопку смены режима, нужно остановить текущ активность Т1
    CALL TIMER_STOP ; остановить таймер 1

    POP R16 ;ждостать старый стрег
    OUT SREG, R16 ;восстановить его
    POP R16 ;восстановить р16
	
    RETI ;выходл из прерыания 