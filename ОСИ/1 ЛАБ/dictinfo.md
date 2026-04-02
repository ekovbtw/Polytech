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

