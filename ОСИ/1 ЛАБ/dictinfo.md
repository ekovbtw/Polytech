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
а вот number of loaded words надо как-то найти. 

причем в доп задании надо это сделать бинарным поиском. 

char turn_symbols[26] = {0}; //  массив с  символами (boot_letters был двоичным массивом)
int turn_symbols_count = 0;

void index_to_symbol(int index, int k) // перевод индекса в символ
{
    turn_symbols[k] = 'a' + index;
}

void find_turn_symbols()
{
	unsigned char* boot_letters = (unsigned char*)0x9000;
	int k =0;
	turn_symbols_count = 0;  
  
	for (int i = 0; i < 26; i++)  // отчистка на всякий случай
	{  
		turn_symbols[i] = 0;  
	}
	for (int i = 0; i<26; i++)
	{
		if (boot_letters[i] == 1) {index_to_symbol(i, k); k++;}
	}
	if (k < 26) // на всякий
    	{
        	turn_symbols[k] = 0;
    	}
}
