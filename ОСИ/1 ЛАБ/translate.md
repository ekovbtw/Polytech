Для начала:
![[Pasted image 20260405133907.png]]
То есть сначала нужно написать проверку на то, что мы ввели translate, затем отделить по пробелу слово, которое хотим перевести, затем выполнить бинарный поиск и написать перевод.
Сначала функция, что ввод начинается с translate:

int starts_with_translate(char* first) // start word translate
{
    char second[] = "translate";
    int i = 0;

    while (second[i] != '\0')
    {
        if (first[i] != second[i])
            return 0;
        i++;
    }

    return 1;
}

Затем объявим эту функцию в varity 
 else if (tarts_with_translate(name))
    {
    	translate()					
    }
    
Напишем функция translate(): начнем с отделения слова, которое нужно перевести.
void translate(char* name) // func for translate
{
    char word[41] = {0}; // find word in fist array
    int i = 0;
    int j = 0;

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
		str_str("Error: no word");  
		back_n();  
		return;  
	}

    while (name[i] != '\0' && j < 40)
    {
        word[j] = name[i];
        i++;
        j++;
    }

    word[j] = '\0'; // после этого в массиве word у нас нужное слово
    
    
}
Чтобы сделать бинарный поиск нужно написать свою str_cmp 

int str_cmp(const char* a, const char* b) // str_cmp 
{
    int i = 0;

    while (a[i] != '\0' && b[i] != '\0')
    {
        if (a[i] < b[i]) return -1;
        if (a[i] > b[i]) return 1;
        i++;
    }

    if (a[i] == '\0' && b[i] == '\0') return 0;
    if (a[i] == '\0') return -1;
    return 1;
}

Далее сам бинарный поиск:

int binary_search(char* word)
{
	int left = 0; 
	int right = array_size - 1;
	int mid = (left + right)/2;
	if (str_cmp(word, en_words[mid]) == 0)
	{
		return mid;
	}
	while ((str_cmp(word, en_words[mid]) == -1 || str_cmp(word, en_words[mid]) == 1) && left <= right)
	{
		mid = (left + right)/2;
		if (str_cmp(word, en_words[mid]) == -1)
		{
			right = mid-1;
		}
		else if (str_cmp(word, en_words[mid]) == 1) left = mid+1;
		else 
		{
			return mid;
		}
	}
	return -1;
}

допишем translate: 
void translate(char* name) // func for translate
{
    char word[41] = {0}; // find word in fist array
    int i = 0;
    int j = 0;

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
        str_str("Error: no word");
        back_n();
        return;
    }

    while (name[i] != '\0' && j < 40)
    {
        word[j] = name[i];
        i++;
        j++;
    }

    word[j] = '\0';
    
    int index_word = binary_search(word);
    if (index_word == -1)
    {
    	str_str("Error a word ");
    	str_str(word);
    	str_str("is unknown");
    	back_n();
    	return; 
    }
    
    str_str(es_words[index_word]);
    
}
