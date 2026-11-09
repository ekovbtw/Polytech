// нужно доделать шифрование всех строк 


#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <winternl.h>
#include <iphlpapi.h>
#include <locale.h>
#pragma comment(lib, "iphlpapi.lib")
#include <intrin.h>

unsigned char enc_password[] = { 0x29, 0x3F, 0x39, 0x28, 0x3F, 0x2E, 0x6B, 0x68, 0x69, 0x5A };
unsigned char enc_success[] = { 0x0A, 0x3B, 0x29, 0x29, 0x2D, 0x35, 0x28, 0x3E, 0x7A, 0x33, 0x29, 0x7A, 0x39, 0x35, 0x28, 0x28, 0x3F, 0x39, 0x2E, 0x74, 0x5A };
unsigned char enc_welcome[] = { 0x0D, 0x3F, 0x36, 0x39, 0x35, 0x37, 0x3F, 0x7A, 0x2E, 0x35, 0x7A, 0x2E, 0x32, 0x3F, 0x7A, 0x2A, 0x28, 0x35, 0x3D, 0x28, 0x3B, 0x37, 0x7B, 0x5A };
unsigned char enc_fail[] = { 0x0A, 0x3B, 0x29, 0x29, 0x2D, 0x35, 0x28, 0x3E, 0x7A, 0x33, 0x29, 0x7A, 0x33, 0x34, 0x39, 0x35, 0x28, 0x28, 0x3F, 0x39, 0x2E, 0x74, 0x5A };
unsigned char enc_err_openpwd[] = { 0x1F, 0x28, 0x28, 0x35, 0x28, 0x7A, 0x35, 0x2A, 0x3F, 0x34, 0x33, 0x34, 0x3D, 0x7A, 0x2A, 0x3B, 0x29, 0x29, 0x2D, 0x35, 0x28, 0x3E, 0x7A, 0x3C, 0x33, 0x36, 0x3F, 0x74, 0x5A };  // "Error opening password file."
unsigned char enc_err_readpwd[] = { 0x1F, 0x28, 0x28, 0x35, 0x28, 0x7A, 0x28, 0x3F, 0x3B, 0x3E, 0x33, 0x34, 0x3D, 0x7A, 0x2A, 0x3B, 0x29, 0x29, 0x2D, 0x35, 0x28, 0x3E, 0x7A, 0x3C, 0x33, 0x36, 0x3F, 0x74, 0x5A };  // "Error reading password file."
unsigned char enc_pwd_read[] = { 0x0A, 0x3B, 0x29, 0x29, 0x2D, 0x35, 0x28, 0x3E, 0x7A, 0x28, 0x3F, 0x3B, 0x3E, 0x7A, 0x3C, 0x28, 0x35, 0x37, 0x7A, 0x3C, 0x33, 0x36, 0x3F, 0x60, 0x7A, 0x7F, 0x29, 0x50, 0x5A };  
unsigned char enc_dbg1[] = { 0x1E, 0x3F, 0x38, 0x2F, 0x3D, 0x3D, 0x3F, 0x28, 0x7A, 0x3E, 0x3F, 0x2E, 0x3F, 0x39, 0x2E, 0x3F, 0x3E, 0x74, 0x7A, 0x1F, 0x22, 0x33, 0x2E, 0x33, 0x34, 0x3D, 0x74, 0x5A };  // "Debugger detected. Exiting."
unsigned char enc_fake[] = { 0x09, 0x3F, 0x39, 0x2F, 0x28, 0x33, 0x2E, 0x23, 0x7A, 0x2C, 0x33, 0x35, 0x36, 0x3B, 0x2E, 0x33, 0x35, 0x34, 0x7A, 0x39, 0x35, 0x3E, 0x3F, 0x7A, 0x6A, 0x22, 0x6B, 0x1C, 0x74, 0x5A };  // "Security violation code 0x1F."
unsigned char enc_access_viol[] = { 0x1B, 0x39, 0x39, 0x3F, 0x29, 0x29, 0x7A, 0x2C, 0x33, 0x35, 0x36, 0x3B, 0x2E, 0x33, 0x35, 0x34, 0x74, 0x5A };  // "Access violation."
unsigned char enc_err_output[] = { 0x1F, 0x28, 0x28, 0x35, 0x28, 0x7A, 0x35, 0x2A, 0x3F, 0x34, 0x33, 0x34, 0x3D, 0x7A, 0x35, 0x2F, 0x2E, 0x2A, 0x2F, 0x2E, 0x7A, 0x3C, 0x33, 0x36, 0x3F, 0x74, 0x5A };  // "Error opening output file."
unsigned char enc_correct[] = { 0x0A, 0x3B, 0x29, 0x29, 0x2D, 0x35, 0x28, 0x3E, 0x7A, 0x33, 0x29, 0x7A, 0x39, 0x35, 0x28, 0x28, 0x3F, 0x39, 0x2E, 0x74, 0x5A };  // "Password is correct."
unsigned char enc_incorrect[] = { 0x0A, 0x3B, 0x29, 0x29, 0x2D, 0x35, 0x28, 0x3E, 0x7A, 0x33, 0x29, 0x7A, 0x33, 0x34, 0x39, 0x35, 0x28, 0x28, 0x3F, 0x39, 0x2E, 0x74, 0x5A };  // "Password is incorrect."
unsigned char enc_decoy[] = { 0x1B, 0x39, 0x39, 0x3F, 0x29, 0x29, 0x7A, 0x3E, 0x3F, 0x34, 0x33, 0x3F, 0x3E, 0x7A, 0x72, 0x3E, 0x3F, 0x39, 0x35, 0x23, 0x73, 0x74, 0x5A };  // "Access denied (decoy)."
unsigned char enc_integrity[] = { 0x13, 0x34, 0x2E, 0x3F, 0x3D, 0x28, 0x33, 0x2E, 0x23, 0x7A, 0x39, 0x32, 0x3F, 0x39, 0x31, 0x7A, 0x3C, 0x3B, 0x33, 0x36, 0x3F, 0x3E, 0x74, 0x7A, 0x19, 0x35, 0x3E, 0x3F, 0x7A, 0x2E, 0x3B, 0x37, 0x2A, 0x3F, 0x28, 0x3F, 0x3E, 0x74, 0x5A };  // "Integrity check failed. Code tampered."
unsigned char enc_finished[] = { 0x0A, 0x28, 0x35, 0x3D, 0x28, 0x3B, 0x37, 0x7A, 0x3C, 0x33, 0x34, 0x33, 0x29, 0x32, 0x3F, 0x3E, 0x74, 0x5A };  // "Program finished."
unsigned char enc_err_file[] = { 0x1F, 0x28, 0x28, 0x35, 0x28, 0x7A, 0x35, 0x2A, 0x3F, 0x34, 0x33, 0x34, 0x3D, 0x7A, 0x3C, 0x33, 0x36, 0x3F, 0x7B, 0x5A };  // "Error opening file!"


// global seed for key generation
volatile unsigned int g_seed = 0xDEADBEEF;

static unsigned char str_key(void)
{
	return (unsigned char)((g_seed & 0xFF) ^ 0xB5);
}

static void corrupt_seed(void)
{
	g_seed = g_seed * 1664525u + 1013904223u;
}
// 


__declspec(noinline) int self_modify_target()
{
	int x = 50;
	x = x + 50;
	return x;
}


void self_modify_demo()
{
	unsigned char* func = (unsigned char*)self_modify_target;
	unsigned char* target_byte = func + 1;

	int (* volatile fp)() = self_modify_target;

	DWORD oldProtect;
	VirtualProtect(func, 16, PAGE_EXECUTE_READWRITE, &oldProtect);

	unsigned char key = 0xAA;
	*target_byte ^= key;
	FlushInstructionCache(GetCurrentProcess(), func, 16);

	

	*target_byte ^= key;
	FlushInstructionCache(GetCurrentProcess(), func, 16);

	VirtualProtect(func, 16, oldProtect, &oldProtect);

	       
}



void anti_debug_4();
__declspec(noinline) int verify_password_again(const char* input);
extern "C" int antidis_add1(int x);
extern "C" int antidis_jmid(int x);
void decrypt(const unsigned char* enc, int len, unsigned char key, char* out);

void print_enc(const unsigned char* enc, int len)
{
	char buf[256];
	decrypt(enc, len, str_key(), buf);
	printf("%s", buf);
}

//#include <intrin.h>
#define SUM_MAX (2147483647LL + 99999LL)
#define SUM_MIN (-2147483648LL - 99999LL)
#define LL long long
#define DEBUG_PRINT 0
#define SIZE 512
CRITICAL_SECTION crit_sec;
int count_threads = 0;
int* start_array = NULL;
int N = 0;
HANDLE start_sem;
LL result = 0;
DWORD WINAPI worker_thread(LPVOID param);
LL count_solved = 0;
LL next_task = 0;
DWORD start_time, end_time;
LL total = 0;
LL total_combinations();
int number_of_digits(LL task_index); // функция для номера знака (для кода Грэя)

FILE* open_file_and_scan()
{
	FILE* file = fopen("input.txt", "r");
	if (file == NULL)
	{
		print_enc(enc_err_file, sizeof(enc_err_file) - 1); printf("\n");
		exit(1);
	}
	return file;
}

void time_to_file()
{
	FILE* file = fopen("time.txt", "w");
	if (file == NULL)
	{
		print_enc(enc_err_file, sizeof(enc_err_file) - 1); printf("\n");
		exit(1);
	}
	DWORD elapsed = end_time - start_time;
	fprintf(file, "%lu", elapsed);
	fclose(file);
}

void result_to_file()
{
	FILE* file = fopen("output.txt", "w");
	if (file == NULL)
	{
		print_enc(enc_err_file, sizeof(enc_err_file) - 1); printf("\n");
		exit(1);
	}
	fprintf(file, "%d\n", count_threads);
	fprintf(file, "%d\n", N);
	fprintf(file, "%lld\n", count_solved);
	fclose(file);
}

int run_payload()
{
	setlocale(LC_ALL, "Russian");
	FILE* file = open_file_and_scan();
	fscanf(file, "%d", &count_threads);
	fscanf(file, "%d", &N);
	start_array = (int*)malloc(sizeof(int) * N);
	InitializeCriticalSection(&crit_sec);

	for (int i = 0; i < N; i++)
	{
		fscanf(file, "%d", &start_array[i]);
	}

	fscanf(file, "%lld", &result);
	fclose(file);


	total = total_combinations();


	start_sem = CreateSemaphore(NULL, 0, count_threads, NULL);
	HANDLE* threads = (HANDLE*)malloc(count_threads * sizeof(HANDLE));

	for (int i = 0; i < count_threads; i++)
	{
		threads[i] = CreateThread(NULL, 0, worker_thread, (LPVOID)(size_t)i, 0, NULL);
	}

	start_time = GetTickCount();


	ReleaseSemaphore(start_sem, count_threads, NULL);

	for (int i = 0; i < count_threads; i++)
	{
		WaitForSingleObject(threads[i], INFINITE);
	}

	end_time = GetTickCount();

	time_to_file();
	result_to_file();

	for (int i = 0; i < count_threads; i++)
		CloseHandle(threads[i]);

	free(threads);
	CloseHandle(start_sem);
	DeleteCriticalSection(&crit_sec);
	free(start_array);
	return 0;
}

LL total_combinations()
{
	return 1LL << (N - 1);
}

bool check_combination(LL task_index)
{
	int count_signs = N - 1;
	LL sum = (LL)start_array[0];

	for (int i = 0; i < count_signs; i++)
	{
		int digit = (task_index >> (count_signs - 1 - i)) & 1;
		if (digit == 1)
			sum += (LL)start_array[i + 1];
		else
			sum -= (LL)start_array[i + 1];

		if (sum > SUM_MAX || sum < SUM_MIN)
			return false;
	}

	if (sum == result)
	{
#if DEBUG_PRINT
		EnterCriticalSection(&crit_sec);
		printf("Решение: %d", start_array[0]);
		for (int i = 0; i < count_signs; i++)
		{
			int digit = (task_index >> (count_signs - 1 - i)) & 1;
			printf(" %c %d", digit ? '+' : '-', start_array[i + 1]);
		}
		printf(" = %lld\n", result);
		LeaveCriticalSection(&crit_sec);
#endif
		return true;
	}
	return false;
}

DWORD WINAPI worker_thread(LPVOID param)
{
	int idx = (int)(size_t)param;

	// Поток блокируется здесь, пока main не вызовет ReleaseSemaphore
	WaitForSingleObject(start_sem, INFINITE);


	LL poisoned_result = result ^ (LL)(g_seed ^ 0xDEADBEEF); // ЗАЩИТА

	while (true)
	{
		int index;
		int end_index;

		EnterCriticalSection(&crit_sec);
		if (next_task >= total)
		{
			LeaveCriticalSection(&crit_sec);
			break;
		}
		index = next_task;
		end_index = next_task + SIZE;
		if (end_index > total)
			end_index = (int)total;
		next_task = end_index;
		LeaveCriticalSection(&crit_sec);

		int local_solved = 0;

		int count_signs = N - 1; // количество знаков
		int current_digit_array[64] = { 0 }; // массив для хранения текущего знака каждого числа (0 - минус, 1 - плюс)
		LL summ = start_array[0]; // сумма текущей комбинации, начинаем с первого числа (тк оно всегда положительное)
		int Grey_code = index ^ (index >> 1); // код Грея для текущего индекса (xor - ^)
		for (int k = 0; k < count_signs; k++)
		{
			int Grey_code_new = Grey_code >> k;
			Grey_code_new = Grey_code_new & 1; // получение знака
			current_digit_array[k] = Grey_code_new; // запись текущего знака (для текущего числа) в массив
			if (Grey_code_new == 1) // обработка знака
			{
				summ += start_array[k + 1];
			}
			else
			{
				summ -= start_array[k + 1];
			}
		}

		if (summ == poisoned_result) // ЗАЩИТА
		{
			local_solved++;
		}
		for (int i = index + 1; i < end_index;i++)
		{
			int k = number_of_digits(i); // получили позицию знака, который изменился
			current_digit_array[k] = 1 - current_digit_array[k]; // меняем знак на противоположный
			if (current_digit_array[k] == 1) // обработка знака
			{
				summ += 2 * start_array[k + 1]; // прибавляем дважды, тк мы меняем знак с минуса на плюс
			}
			else
			{
				summ -= 2 * start_array[k + 1]; // вычитаем дважды, тк мы меняем знак с плюса на минус
			}
			if (summ == poisoned_result)
			{
				local_solved++;
			}
		}
		if (local_solved > 0)
		{
			EnterCriticalSection(&crit_sec);
			count_solved += local_solved;
			LeaveCriticalSection(&crit_sec);
		}
	}

	return 0;
}

int number_of_digits(LL task_index) // суть в том чтобы найти позицию первой еденицы справа 
{
	int c = 0;
	while ((task_index & 1) == 0)
	{
		task_index >>= 1;
		c++;
	}
	return c;
}





void programm(const char* password)
{
	anti_debug_4();
	if (!verify_password_again(password))
	{
		print_enc(enc_access_viol, sizeof(enc_access_viol) - 1); printf("\n");
		exit(11);
	}

	char buf[64];
	decrypt(enc_welcome, sizeof(enc_welcome) - 1, str_key(), buf);
	printf("%s\n", buf);  
	run_payload();


	return;
}

void decrypt(const unsigned char* enc, int len, unsigned char key, char* out)
{
	for (int i = 0; i < len; i++)
		out[i] = enc[i] ^ key;
	out[len] = '\0';
}

void anti_debug_1()
{
	if (IsDebuggerPresent())
	{
		print_enc(enc_dbg1, sizeof(enc_dbg1) - 1); printf("\n");
		exit(1);
	}
}


void anti_debug_2()
{
	PPEB peb = (PPEB)__readgsqword(0x60);
	if (peb->BeingDebugged)
	{
		corrupt_seed();   
	}
}


void anti_debug_3()
{
	BOOL present = FALSE;
	CheckRemoteDebuggerPresent(GetCurrentProcess(), &present);
	if (present)
	{
		corrupt_seed();  
	}
}


void anti_debug_4()
{
	DWORD start = GetTickCount();
	for (volatile int i = 0; i < 1000; i++) {}
	DWORD elapsed = GetTickCount() - start;
	if (elapsed > 100)
	{
		corrupt_seed();  
	}
}


void anti_debug5()
{
	CONTEXT ctx;
	memset(&ctx, 0, sizeof(ctx));
	ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
	if (GetThreadContext(GetCurrentThread(), &ctx))
	{
		if (ctx.Dr0 != 0 || ctx.Dr1 != 0 || ctx.Dr2 != 0 || ctx.Dr3 != 0)
			corrupt_seed();
	}
}


int check_registry_vm()
{
	HKEY hKey;
	char buffer[256];
	DWORD size = sizeof(buffer);


	if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
		"HARDWARE\\DESCRIPTION\\System\\BIOS", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
	{
		if (RegQueryValueExA(hKey, "SystemManufacturer", NULL, NULL,
			(LPBYTE)buffer, &size) == ERROR_SUCCESS)
		{
			RegCloseKey(hKey);
			if (strstr(buffer, "VMware") || strstr(buffer, "VirtualBox") ||
				strstr(buffer, "VBOX") || strstr(buffer, "innotek"))
				return 1;
		}
		else RegCloseKey(hKey);
	}
	return 0;
}


int check_mac_vm()
{
	IP_ADAPTER_INFO adapters[16];
	DWORD bufLen = sizeof(adapters);
	if (GetAdaptersInfo(adapters, &bufLen) != ERROR_SUCCESS) return 0;

	IP_ADAPTER_INFO* a = adapters;
	while (a)
	{
		// VirtualBox: 08-00-27
		if (a->Address[0] == 0x08 && a->Address[1] == 0x00 && a->Address[2] == 0x27)
			return 1;
		// VMware: 00-0C-29
		if (a->Address[0] == 0x00 && a->Address[1] == 0x0C && a->Address[2] == 0x29)
			return 1;
		a = a->Next;
	}
	return 0;
}


int check_cpuid_vm()
{
	int cpuInfo[4] = { 0 };
	__cpuid(cpuInfo, 1);
	if ((cpuInfo[2] >> 31) & 1)
		return 1;
	return 0;
}

int check_files_vm()
{
	const char* vm_files[] = {
		"C:\\Windows\\System32\\drivers\\VBoxMouse.sys",
		"C:\\Windows\\System32\\drivers\\VBoxGuest.sys",
		"C:\\Windows\\System32\\drivers\\VBoxSF.sys",
		"C:\\Windows\\System32\\drivers\\vmhgfs.sys",
		"C:\\Windows\\System32\\drivers\\vmmouse.sys"
	};
	int count = sizeof(vm_files) / sizeof(vm_files[0]);
	for (int i = 0; i < count; i++)
	{
		if (GetFileAttributesA(vm_files[i]) != INVALID_FILE_ATTRIBUTES)
			return 1;
	}
	return 0;
}

void anti_vm()
{
	if (check_registry_vm()) { corrupt_seed(); }   
	if (check_mac_vm()) { corrupt_seed(); }   
	//if (check_cpuid_vm()) { printf("VM: cpuid\n"); exit(7); }
	if (check_files_vm()) { corrupt_seed(); }  
}



unsigned int crc32(unsigned char* data, int len)
{
	unsigned int crc = 0xFFFFFFFF; // стартовое значение
	for (int i = 0; i < len; i++)
	{
		crc ^= data[i];
		for (int j = 0; j < 8; j++)
			crc = (crc >> 1) ^ (0xEDB88320 & (-(int)(crc & 1))); // полином
	}
	return ~crc;
}

volatile int opaque_var = 5;




__declspec(noinline) int check_password_crc(const char* input, const char* real)
{
	int i = 0;
	
	while (real[i] != '\0')
	{
		if (input[i] != real[i])
			return 0;
		i++;
	}
	if (input[i] != '\0')   
		return 0;
	return 1;
}


__declspec(noinline) void marker_crc()
{
	volatile int x = 0;
	(void)x;
}

void verify_integrity()
{
	unsigned char* start = (unsigned char*)check_password_crc;
	unsigned char* end = (unsigned char*)marker_crc;
	int len = (int)(end - start);
	if (len <= 0 || len > 4096) return;
	unsigned int current = crc32(start, len);
	if (current != 0xF5D709B8)
		corrupt_seed();
}


__declspec(noinline) int verify_password_again(const char* input)
{
	char real[64];
	decrypt(enc_password, sizeof(enc_password) - 1, str_key(), real);


	int i = 0;
	while (real[i] != '\0')
	{
		if (input[i] != real[i])
			return 0;
		i++;
	}
	if (input[i] != '\0') return 0;
	return 1;
}


__declspec(noinline) void protected_action(const char* password)
{
	int zero = 0;
	zero = (opaque_var - opaque_var);

	__try
	{
		int crash = 1 / zero;
		printf("unreachable %d\n", crash);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		programm(password);
	}
}


int main()
{
	anti_debug_1();
	
	self_modify_demo();
	   
	verify_integrity();
	FILE* file = fopen("password.txt", "r");
	if (file == NULL)
	{
		print_enc(enc_err_openpwd, sizeof(enc_err_openpwd) - 1); printf("\n");
		exit(1);
	}

	char password[100];
	if (fgets(password, sizeof(password), file) == NULL)
	{
		print_enc(enc_err_readpwd, sizeof(enc_err_readpwd) - 1); printf("\n");
		fclose(file);
		exit(1);
	}

	int password_length = strlen(password);
	if (password[password_length - 1] == '\n')
	{
		password[password_length - 1] = '\0';
		password_length--;
	}

	{ char fb[256]; decrypt(enc_pwd_read, sizeof(enc_pwd_read) - 1, str_key(), fb); printf(fb, password); }


	if (strlen(password) == 999)   // fake
	{
		print_enc(enc_fake, sizeof(enc_fake) - 1); printf("\n");
		exit(42);
	}

	anti_debug_2();

	anti_vm();

	anti_debug5();

	char real_password[64];
	decrypt(enc_password, sizeof(enc_password) - 1, str_key(), real_password);



	if (check_password_crc(password, real_password) == 1)
	{

		anti_debug_3();


		FILE* out = fopen("serial.txt", "w");
		if (out == NULL)
		{
			print_enc(enc_err_output, sizeof(enc_err_output) - 1); printf("\n");
			fclose(file);
			exit(1);
		}
		else
		{
			
			unsigned int base = (g_seed ^ 0x973BBC3D) - 4;
			unsigned int serial_num = base;
			serial_num = antidis_add1(serial_num);   
			serial_num = antidis_jmid(serial_num);  
	
			fprintf(out, "KEY$%010u$\n", serial_num);
			fclose(out);
		}

		if ((opaque_var | 1) != 0) // непрозрачный предикат
		{
			print_enc(enc_correct, sizeof(enc_correct) - 1); printf("\n");
			protected_action(password);
		}
		else
		{
			print_enc(enc_decoy, sizeof(enc_decoy) - 1); printf("\n");
			exit(99);
		}
	}
	else
	{
		print_enc(enc_incorrect, sizeof(enc_incorrect) - 1); printf("\n");
	}
	print_enc(enc_finished, sizeof(enc_finished) - 1); printf("\n");
	fclose(file);
	return 0;
}