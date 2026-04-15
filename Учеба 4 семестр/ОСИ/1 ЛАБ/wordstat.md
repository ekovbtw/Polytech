![[Pasted image 20260405194446.png]]
Добавим в varity: 
else if (starts_with_wordstat(name))
    {
    	wordstat(name);
    } 
    
Вот сами функции:
int starts_with_wordstat(char* first) // start word translate
{
    char second[] = "wordstat";
    int i = 0;

    while (second[i] != '\0')
    {
        if (first[i] != second[i])
            return 0;
        i++;
    }

    return 1;
}



void wordstat(char* name)
{
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
    if (name[i] == '\0')
    {
        str_str("Error: no symbol");
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
    	find_turn_symbols();

	if (!letter_is_allowed(symbol))
	{
		str_str("Letter '");
		put_point(symbol);
		str_str("': 0 words loaded.");
		back_n();
		return;
	}
    	int j = 0; 
    	while (en_words[j][0] != symbol && j<array_size) j++;
    	if (j>=array_size)
    	{
    		str_str("Not found");
    		return;
    	}
    	else
    	{
    		int count = 0;
    		while (en_words[j][0] == symbol)
    		{
    			count++;
    			j++;
    		}
    		str_str("Letter '");
	    	put_point(symbol);
	    	str_str("': ");
	    	print_count(count);
	    	str_str(" words loaded.");
    		back_n();
    	}
    }
}