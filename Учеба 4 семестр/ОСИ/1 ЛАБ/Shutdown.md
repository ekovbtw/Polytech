Добавим в varity:
else if (str_check(name, "shutdown"))
    {
    	shutdown();
    }
сама функция 
void shutdown()
{
    str_str("Powering off...");
    back_n();

    outw(0x604, 0x2000);

    while (1)
    {
        asm("hlt");
    }
}
обработчик прерываний(в начало):
static inline void outw(unsigned short port, unsigned short data)
{
    asm volatile ("outw %0, %1" : : "a"(data), "Nd"(port));
}
