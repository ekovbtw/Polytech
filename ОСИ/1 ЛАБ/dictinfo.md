![[Pasted image 20260402145325.png]]
void dictinfo()
{
	int number_of_words = 0;
	int number_of_loaded_words = 0;
	str_str("Dictionary: en -> es");
	back_n();
	str_str("Number of words: ");
	back_n();
	str_str("Number of loaded words:");
	back_n();
}

теперь надо добавить словарь:
const char* en_words[3] = {"apple", "book", "cat"}; // ENG words
const char* es_words[3] = {"manzana", "libro", "gato"}; // SPAIN words 
пока так.

Number of words = длине словаря это похуй 
а вот number of loaded word  просто найду по первому символу. 

char turn_symbols[26] = {0}; // array turn_symbols 
int turn_symbols_count = 0;

void index_to_symbol(int index, int k) // switch index to symbols 
{
    turn_symbols[k] = 'a' + index;
    turn_symbols_count++;
}

void find_turn_symbols() // scan boot_letters and if letter has true ==> turn to array turn_symbols 
{
	unsigned char* boot_letters = (unsigned char*)0x9000;
	int k = 0;
	turn_symbols_count = 0;

    	for (int i = 0; i < 26; i++)
    	{
    	    turn_symbols[i] = 0;
    	}
	for (int i = 0; i<26; i++)
	{
		if (boot_letters[i] == 1) {index_to_symbol(i, k); k++;}
	}
	if (k < 26)
    	{
        	turn_symbols[k] = 0;
    	}
	
}

int letter_is_allowed(char c) // proverka true first symbol or not develop
{
    for (int i = 0; i < turn_symbols_count; i++)
    {
        if (turn_symbols[i] == c)
        {
            return 1;
        }
    }

    return 0;
}

int number_of_loaded_words() // FIDN COUNT turn words
{
	int count = 0;
	for (int i = 0; i<turn_symbols_count; i++)
	{
		for (int j =0; j<3; j++)
		{
			if (turn_symbols[i] == en_words[j][0])
			{
				count++;
			}
		}
	}
	return count;
}

теперь надо придумать как вывести число count тк %d не работает.

print_count(count_of_loaded_words); сделаем вызов фунции в dictinfo.

void print_count(int count) // 
{
	if (count<0)
	{
		put_point('-');
		count = - count;
	}
	else if (count == 0)
	{
		put_point('0');
	}
	else if(count>999)
	{
		str_str("Error find count loaded words(>=1000)");
	}
	else 
	{
		char chislo[4];
		int i = 0;
		while (count>0)
		{
			int last = count%10; 
			chislo[i] = '0' + last;
			i++;
			count = count/10;
		}
		while (i>0)
		{
			i--;
			put_point(chislo[i]);
		}
		
	}
	
}