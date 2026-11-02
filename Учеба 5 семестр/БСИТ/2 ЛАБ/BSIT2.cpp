#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <shldisp.h>      // IShellDispatch, Folder, CopyHere
#include <comdef.h>       // _bstr_t, _variant_t
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#include <aclapi.h>


void  WINAPI service_main(DWORD argc, LPSTR* argv);
void  WINAPI control_handler(DWORD request);
int   install_service();
int   remove_service();
int   do_start_service();     // Do..., чтобы не путать с WinAPI StartService
int   do_stop_service();
void  write_log(const char* msg);
int   read_config(const char* path);
char* trim(char* s);
void  scan_folder(const char* dir, char files[][MAX_PATH], int* count);
void  make_dirs(const char* path);
void  remove_dir(const char* dir);
int   copy_to_staging(const char* staging, char files[][MAX_PATH], int count);
int   create_empty_zip(const char* zipPath);
int   zip_add_files(const char* zipPath, const char* staging);
void  do_backup();
void grant_everyone_access(const char* path);

static const char* SERVICE_NAME = "BackupService";


SERVICE_STATUS        service_status = { 0 };
SERVICE_STATUS_HANDLE status_handle = NULL;
HANDLE                stop_event = INVALID_HANDLE_VALUE;   // сигнал "пора останавливаться"


#define MAX_MASKS 16

typedef struct BACKUP_CONFIG
{
	char source[MAX_PATH];        // откуда бэкапим
	char dest[MAX_PATH];          // куда класть архив
	char archive[MAX_PATH];       // имя zip
	char masks[MAX_MASKS][64];    // список масок
	int  mask_count;              // сколько масок реально прочитали
} BACKUP_CONFIG;

BACKUP_CONFIG config;


char service_path[MAX_PATH] = { 0 };   // путь к .exe (нужен при установке)


int main(int argc, char* argv[])
{
	read_config("C:\\Users\\user\\Documents\\Bsit2\\config.txt");   // реальный путь к config.txt

	char dbg[1024];
	sprintf_s(dbg, sizeof(dbg), "src=%s | dest=%s | zip=%s | masks=%d",
		config.source, config.dest, config.archive, config.mask_count);
	write_log(dbg);

	GetModuleFileNameA(NULL, service_path, MAX_PATH);

	if (argc > 1)
	{
		// запустил ЧЕЛОВЕК с аргументом -> режим управления (SCP)
		if (strcmp(argv[1], "install") == 0) return install_service();
		else if (strcmp(argv[1], "remove") == 0) return remove_service();
		else if (strcmp(argv[1], "start") == 0) return do_start_service();
		else if (strcmp(argv[1], "stop") == 0) return do_stop_service();
		else if (strcmp(argv[1], "backup") == 0) { do_backup(); return 0; }   // прогон вручную
		else { printf("Unknown command\n"); return 1; }
	}

	// аргументов нет -> нас запустил SCM -> режим СЕРВИСА
	SERVICE_TABLE_ENTRYA serviceTable[] = {
		{ (LPSTR)SERVICE_NAME, (LPSERVICE_MAIN_FUNCTIONA)service_main },
		{ NULL, NULL }
	};

	if (!StartServiceCtrlDispatcherA(serviceTable))
	{
		// частый код 1063 — запустили вручную, а не через SCM
		write_log("StartServiceCtrlDispatcher failed");
		return 1;
	}
	return 0;
}


void WINAPI service_main(DWORD argc, LPSTR* argv)
{
	status_handle = RegisterServiceCtrlHandlerA(SERVICE_NAME, control_handler);
	if (status_handle == NULL) return;

	// сообщаем: стартуем
	memset(&service_status, 0, sizeof(service_status));
	service_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	service_status.dwCurrentState = SERVICE_START_PENDING;
	service_status.dwControlsAccepted = 0;   // пока ничего не принимаем
	SetServiceStatus(status_handle, &service_status);

	// событие остановки (ручной сброс, изначально не сигнально)
	stop_event = CreateEventA(NULL, TRUE, FALSE, NULL);
	if (stop_event == NULL)
	{
		service_status.dwCurrentState = SERVICE_STOPPED;
		service_status.dwWin32ExitCode = GetLastError();
		SetServiceStatus(status_handle, &service_status);
		return;
	}

	// готовы работать: RUNNING + принимаем STOP/SHUTDOWN
	service_status.dwCurrentState = SERVICE_RUNNING;
	service_status.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
	SetServiceStatus(status_handle, &service_status);
	write_log("Service RUNNING");

	// ждём 5 сек или событие остановки; на каждом тике — архивация
	while (WaitForSingleObject(stop_event, 30000) != WAIT_OBJECT_0)
	{
		do_backup();
	}

	// пришёл сигнал остановки -> выходим
	CloseHandle(stop_event);
	service_status.dwCurrentState = SERVICE_STOPPED;
	service_status.dwWin32ExitCode = 0;
	SetServiceStatus(status_handle, &service_status);
	write_log("Service STOPPED");
}


void WINAPI control_handler(DWORD request)   // обработчик команд от SCM
{
	switch (request)
	{
	case SERVICE_CONTROL_STOP:
	case SERVICE_CONTROL_SHUTDOWN:
		write_log("STOP/SHUTDOWN received");
		// сообщаем, что начали останавливаться
		service_status.dwCurrentState = SERVICE_STOP_PENDING;
		SetServiceStatus(status_handle, &service_status);
		// будим рабочий цикл
		SetEvent(stop_event);
		break;
	default:
		break;
	}
}


void write_log(const char* msg)   // сервис не видит консоль — пишем в файл
{
	FILE* f = NULL;
	// абсолютный путь! рабочая папка сервиса = System32, относительный потеряется
	fopen_s(&f, "C:\\BackupService.log", "a+");
	if (f == NULL) return;
	fprintf(f, "%s\n", msg);
	fclose(f);
}


int install_service()
{
	SC_HANDLE hSCM = OpenSCManagerA(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
	if (hSCM == NULL)
	{
		printf("OpenSCManager failed: %lu\n", GetLastError());
		return 1;
	}

	SC_HANDLE hSvc = CreateServiceA(
		hSCM,
		SERVICE_NAME,              // внутреннее имя
		SERVICE_NAME,              // отображаемое имя
		SERVICE_ALL_ACCESS,
		SERVICE_WIN32_OWN_PROCESS,
		SERVICE_DEMAND_START,      // запуск вручную
		SERVICE_ERROR_NORMAL,
		service_path,              // путь к нашему .exe
		NULL, NULL, NULL, NULL, NULL);

	if (hSvc == NULL)
	{
		printf("CreateService failed: %lu\n", GetLastError());
		CloseServiceHandle(hSCM);
		return 1;
	}
	printf("Service installed.\n");
	CloseServiceHandle(hSvc);
	CloseServiceHandle(hSCM);
	return 0;
}


int remove_service()
{
	SC_HANDLE hSCM = OpenSCManagerA(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (hSCM == NULL)
	{
		printf("OpenSCManager failed: %lu\n", GetLastError());
		return 1;
	}

	SC_HANDLE hSvc = OpenServiceA(hSCM, SERVICE_NAME, DELETE);
	if (hSvc == NULL)
	{
		printf("OpenService failed: %lu\n", GetLastError());
		CloseServiceHandle(hSCM);
		return 1;
	}

	if (!DeleteService(hSvc))
		printf("DeleteService failed: %lu\n", GetLastError());
	else
		printf("Service removed.\n");

	CloseServiceHandle(hSvc);
	CloseServiceHandle(hSCM);
	return 0;
}


int do_start_service()
{
	SC_HANDLE hSCM = OpenSCManagerA(NULL, NULL, SC_MANAGER_CONNECT);
	if (hSCM == NULL)
	{
		printf("OpenSCManager failed: %lu\n", GetLastError());
		return 1;
	}

	SC_HANDLE hSvc = OpenServiceA(hSCM, SERVICE_NAME, SERVICE_START);
	if (hSvc == NULL)
	{
		printf("OpenService failed: %lu\n", GetLastError());
		CloseServiceHandle(hSCM);
		return 1;
	}

	if (!StartServiceA(hSvc, 0, NULL))
		printf("StartService failed: %lu\n", GetLastError());
	else
		printf("Service started.\n");

	CloseServiceHandle(hSvc);
	CloseServiceHandle(hSCM);
	return 0;
}


int do_stop_service()
{
	SC_HANDLE hSCM = OpenSCManagerA(NULL, NULL, SC_MANAGER_CONNECT);
	if (hSCM == NULL)
	{
		printf("OpenSCManager failed: %lu\n", GetLastError());
		return 1;
	}

	SC_HANDLE hSvc = OpenServiceA(hSCM, SERVICE_NAME, SERVICE_STOP);
	if (hSvc == NULL)
	{
		printf("OpenService failed: %lu\n", GetLastError());
		CloseServiceHandle(hSCM);
		return 1;
	}

	SERVICE_STATUS status;
	if (!ControlService(hSvc, SERVICE_CONTROL_STOP, &status))
		printf("ControlService(STOP) failed: %lu\n", GetLastError());
	else
		printf("Stop signal sent.\n");

	CloseServiceHandle(hSvc);
	CloseServiceHandle(hSCM);
	return 0;
}


char* trim(char* s)
{
	while (*s == ' ' || *s == '\t') s++;                             // слева
	char* end = s + strlen(s) - 1;
	while (end > s && (*end == ' ' || *end == '\t')) *end-- = '\0';  // справа
	return s;
}


int read_config(const char* path)
{
	FILE* f = fopen(path, "r");
	if (f == NULL)
	{
		write_log("Error: can't open config");
		return -1;
	}

	config.mask_count = 0;
	char line[512];

	while (fgets(line, sizeof(line), f))
	{
		// убираем перевод строки в конце
		line[strcspn(line, "\r\n")] = '\0';

		// пропускаем пустые строки
		if (line[0] == '\0') continue;

		// ищем '=' и делим на ключ и значение
		char* eq = strchr(line, '=');
		if (eq == NULL) continue;    // строка без '=' — игнор
		*eq = '\0';                  // разрубаем строку на две
		char* key = line;
		char* val = eq + 1;

		// обрезаем пробелы вокруг ключа и значения
		key = trim(key);
		val = trim(val);

		// раскладываем по полям
		if (strcmp(key, "source") == 0) strcpy_s(config.source, MAX_PATH, val);
		else if (strcmp(key, "dest") == 0) strcpy_s(config.dest, MAX_PATH, val);
		else if (strcmp(key, "archive") == 0) strcpy_s(config.archive, MAX_PATH, val);
		else if (strcmp(key, "mask") == 0)
		{
			if (config.mask_count < MAX_MASKS)
			{
				strcpy_s(config.masks[config.mask_count], 64, val);
				config.mask_count++;
			}
		}
	}

	fclose(f);
	return 0;
}


void scan_folder(const char* dir, char files[][MAX_PATH], int* count)
{
	char searchPath[MAX_PATH];
	WIN32_FIND_DATAA fd;
	HANDLE hFind;

	// ПРОХОД 1: подпапки (рекурсия)
	sprintf_s(searchPath, MAX_PATH, "%s\\*", dir);
	hFind = FindFirstFileA(searchPath, &fd);
	if (hFind != INVALID_HANDLE_VALUE)
	{
		do {
			if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
				continue;

			if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)   // это папка
			{
				char subDir[MAX_PATH];
				sprintf_s(subDir, MAX_PATH, "%s\\%s", dir, fd.cFileName);
				scan_folder(subDir, files, count);                // спускаемся внутрь
			}
		} while (FindNextFileA(hFind, &fd));
		FindClose(hFind);
	}

	// ПРОХОД 2: файлы по маскам
	for (int m = 0; m < config.mask_count; m++)
	{
		sprintf_s(searchPath, MAX_PATH, "%s\\%s", dir, config.masks[m]);
		hFind = FindFirstFileA(searchPath, &fd);
		if (hFind == INVALID_HANDLE_VALUE) continue;

		do {
			if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;

			if (*count < 256)                                     // сохраняем файл
			{
				sprintf_s(files[*count], MAX_PATH, "%s\\%s", dir, fd.cFileName);
				(*count)++;
			}
		} while (FindNextFileA(hFind, &fd));
		FindClose(hFind);
	}
}


void make_dirs(const char* path)
{
	char tmp[MAX_PATH];
	strcpy_s(tmp, MAX_PATH, path);

	// идём по пути и на каждом '\' создаём промежуточную папку
	for (char* p = tmp + 3; *p; p++)        // +3 чтобы пропустить "C:\"
	{
		if (*p == '\\')
		{
			*p = '\0';                      // временно обрубаем путь здесь
			CreateDirectoryA(tmp, NULL);    // создаём этот уровень (если есть — не беда)
			*p = '\\';                      // возвращаем разделитель
		}
	}
	CreateDirectoryA(tmp, NULL);            // последний уровень
}


void remove_dir(const char* dir)            // рекурсивно удаляет папку со всем содержимым
{
	char searchPath[MAX_PATH];
	WIN32_FIND_DATAA fd;

	sprintf_s(searchPath, MAX_PATH, "%s\\*", dir);
	HANDLE hFind = FindFirstFileA(searchPath, &fd);
	if (hFind == INVALID_HANDLE_VALUE) return;

	do {
		if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
			continue;

		char full[MAX_PATH];
		sprintf_s(full, MAX_PATH, "%s\\%s", dir, fd.cFileName);

		if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			remove_dir(full);               // внутрь
		else
			DeleteFileA(full);              // файл
	} while (FindNextFileA(hFind, &fd));
	FindClose(hFind);

	RemoveDirectoryA(dir);                  // теперь пустую папку
}


int copy_to_staging(const char* staging, char files[][MAX_PATH], int count)
{
	for (int i = 0; i < count; i++)
	{
		// files[i] = "C:\backup_src\p1\p2\lalala.txt"
		// берём относительную часть "p1\p2\lalala.txt"
		const char* rel = files[i] + strlen(config.source);
		if (*rel == '\\') rel++;            // убираем ведущий слэш

		// полный путь назначения: "C:\...\_staging\p1\p2\lalala.txt"
		char dest[MAX_PATH];
		sprintf_s(dest, MAX_PATH, "%s\\%s", staging, rel);

		// создаём папки под файл (всё, кроме имени самого файла)
		char destDir[MAX_PATH];
		strcpy_s(destDir, MAX_PATH, dest);
		char* lastSlash = strrchr(destDir, '\\');
		if (lastSlash)
		{
			*lastSlash = '\0';             // отрезаем имя файла — остаётся путь папки
			make_dirs(destDir);           // создаём эту папку
		}

		// копируем файл
		if (!CopyFileA(files[i], dest, FALSE))
		{
			char buf[600];
			sprintf_s(buf, sizeof(buf), "CopyFile failed: %s (err %lu)", files[i], GetLastError());
			write_log(buf);
		}
	}
	return 0;
}


int create_empty_zip(const char* zipPath)
{
	FILE* f = fopen(zipPath, "wb");
	if (!f)
	{
		write_log("Error: can't create zip");
		return -1;
	}
	// 22 байта пустого ZIP: сигнатура EOCD "PK\x05\x06" + нули
	unsigned char emptyZip[22] = {
		0x50, 0x4B, 0x05, 0x06, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0
	};
	fwrite(emptyZip, 1, sizeof(emptyZip), f);
	fclose(f);
	return 0;
}


int zip_add_files(const char* zipPath, const char* staging)
{
	// 1. создаём пустой валидный zip
	if (create_empty_zip(zipPath) != 0) return -1;

	// 2. включаем COM
	HRESULT hr = CoInitialize(NULL);
	if (FAILED(hr)) { write_log("CoInitialize failed"); return -1; }

	IShellDispatch* pShell = NULL;
	hr = CoCreateInstance(CLSID_Shell, NULL, CLSCTX_INPROC_SERVER,
		IID_IShellDispatch, (void**)&pShell);
	if (FAILED(hr)) { write_log("CoCreateInstance failed"); CoUninitialize(); return -1; }

	// 3. получаем Folder нашего zip (NameSpace принимает VARIANT со строкой-путём)
	VARIANT vZip;
	VariantInit(&vZip);
	WCHAR wzip[MAX_PATH];
	MultiByteToWideChar(CP_ACP, 0, zipPath, -1, wzip, MAX_PATH);   // путь к zip нужен в BSTR
	vZip.vt = VT_BSTR;
	vZip.bstrVal = SysAllocString(wzip);

	Folder* pZipFolder = NULL;
	hr = pShell->NameSpace(vZip, &pZipFolder);
	if (FAILED(hr) || pZipFolder == NULL)
	{
		write_log("NameSpace(zip) failed");
		VariantClear(&vZip);
		pShell->Release();
		CoUninitialize();
		return -1;
	}

	// 4. кладём в zip верхний уровень staging (папки уедут целиком, с деревом)
	char topSearch[MAX_PATH];
	sprintf_s(topSearch, MAX_PATH, "%s\\*", staging);
	WIN32_FIND_DATAA fd;
	HANDLE h = FindFirstFileA(topSearch, &fd);
	if (h != INVALID_HANDLE_VALUE)
	{
		do {
			if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
				continue;

			char item[MAX_PATH];
			sprintf_s(item, MAX_PATH, "%s\\%s", staging, fd.cFileName);

			VARIANT vFile, vOpt;
			VariantInit(&vFile);
			VariantInit(&vOpt);
			WCHAR witem[MAX_PATH];
			MultiByteToWideChar(CP_ACP, 0, item, -1, witem, MAX_PATH);
			vFile.vt = VT_BSTR;
			vFile.bstrVal = SysAllocString(witem);
			vOpt.vt = VT_I4;
			vOpt.lVal = (4 | 16 | 1024);   // без диалогов, "да для всех", без UI ошибок

			pZipFolder->CopyHere(vFile, vOpt);
			Sleep(1000);                   // CopyHere асинхронный — даём время дозаписать

			VariantClear(&vFile);
			VariantClear(&vOpt);
		} while (FindNextFileA(h, &fd));
		FindClose(h);
	}

	// 5. уборка
	pZipFolder->Release();
	VariantClear(&vZip);
	pShell->Release();
	CoUninitialize();
	return 0;
}


void do_backup()   // полный цикл: собрать -> зеркалить в staging -> упаковать -> убрать staging
{
	static char foundFiles[256][MAX_PATH];
	int foundCount = 0;

	scan_folder(config.source, foundFiles, &foundCount);

	// промежуточная папка, в которую зеркалим дерево источника
	char staging[MAX_PATH];
	sprintf_s(staging, MAX_PATH, "%s\\_staging", config.dest);

	remove_dir(staging);        // чистим остатки прошлого прогона
	make_dirs(staging);         // создаём заново (заодно и config.dest)

	copy_to_staging(staging, foundFiles, foundCount);

	char zipFull[MAX_PATH];
	sprintf_s(zipFull, MAX_PATH, "%s\\%s", config.dest, config.archive);   // C:\...\backup.zip

	zip_add_files(zipFull, staging);
	grant_everyone_access(zipFull);

	remove_dir(staging);        // убираем промежуточную папку

	char dbg[256];
	sprintf_s(dbg, sizeof(dbg), "Backup done: %d files -> %s", foundCount, zipFull);
	write_log(dbg);
}

void grant_everyone_access(const char* path)
{
	PSID pEveryone = NULL;
	SID_IDENTIFIER_AUTHORITY worldAuth = SECURITY_WORLD_SID_AUTHORITY;

	// создаём SID группы "Все" (Everyone / World)
	if (!AllocateAndInitializeSid(&worldAuth, 1, SECURITY_WORLD_RID,
		0, 0, 0, 0, 0, 0, 0, &pEveryone))
		return;

	// описываем запись доступа: Everyone -> полный доступ, с наследованием
	EXPLICIT_ACCESSA ea;
	memset(&ea, 0, sizeof(ea));
	ea.grfAccessPermissions = GENERIC_ALL;
	ea.grfAccessMode = SET_ACCESS;
	ea.grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;  // на подпапки и файлы
	ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
	ea.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
	ea.Trustee.ptstrName = (LPSTR)pEveryone;

	// строим новый DACL с этой записью
	PACL pNewDacl = NULL;
	if (SetEntriesInAclA(1, &ea, NULL, &pNewDacl) == ERROR_SUCCESS)
	{
		// применяем DACL к папке
		SetNamedSecurityInfoA((LPSTR)path, SE_FILE_OBJECT,
			DACL_SECURITY_INFORMATION,
			NULL, NULL, pNewDacl, NULL);
		LocalFree(pNewDacl);
	}

	FreeSid(pEveryone);
}