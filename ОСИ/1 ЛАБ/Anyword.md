volatile unsigned int g_ticks = 0;
void timer_handler()
{
    asm("pusha");

    g_ticks++;

    outb(PIC1_PORT, 0x20);
    asm("popa; leave; iret");
}

unsigned int seed = 137;
int a = 50904585;

int lkg(int count)
{
	seed ^= g_ticks;
	seed = seed*a+ 2781023;
	return (int)(seed%count);
}

int starts_with_anyword(char* first) // start word translate
{
    char second[] = "anyword";
    int i = 0;

    while (second[i] != '\0')
    {
        if (first[i] != second[i])
            return 0;
        i++;
    }

    return 1;
}
void anyword(char* name)
{
	int count_ar = 0;
	int index = -1;
	char symbol;
	int i = 0;
	while (name[i] != ' ' && name[i] != '\0')
    {
        i++;
    }	
    while (name[i] == ' ')
    {
        i++;
    }
    find_turn_symbols();
    if (name[i] == '\0')
    {
    	int x1 = lkg(turn_symbols_count); // random letter
    	char letter = turn_symbols[x1];
    	int q = 0;
    	while (en_words[q][0] != letter) q++;
    	int count1 = 0;
    	while (en_words[q][0] == letter)
    	{
    		count1++;
    		q++;
    	}
    	int x2 = lkg(count1);
    	str_str(en_words[q-count1+x2]);
   	put_point(':');
	put_point(' ');
    	str_str(es_words[q-count1+x2]);
    	
        back_n();
        return;
    }
    else
    {
    	if (name[i+1] != '\0')
    	{
    		int k = i;
    		while (name[k] == ' ')
    		{
   	 	    k++;
    		}
    		if (name[k] != '\0')
    		{
    			str_str (">1 symbols");
    			return;
    		}
    	}
    	symbol = name[i];

	if (!letter_is_allowed(symbol))
	{
		str_str("Error: no words '");
		back_n();
		return;
	}
    	int j = 0; 
    	while (en_words[j][0] != symbol && j<array_size) j++;
    	if (j>=array_size)
    	{
    		return;
    	}
    	else
    	{
    		index = j;
    		int count = 0;
    		while (en_words[j][0] == symbol)
    		{
    			count++;
    			j++;
    		}
    		count_ar = count;
    	}
    }
    int x1 = lkg(count_ar);
    str_str(en_words[index+x1]);
    put_point(':');
	put_point(' ');
    str_str(es_words[index+x1]);
    back_n();
}