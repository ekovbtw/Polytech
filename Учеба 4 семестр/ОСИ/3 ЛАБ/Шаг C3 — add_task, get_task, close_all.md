#### Копируешь из QSort без изменений

- add_task — логика та же, только вызывается из main
- get_task — логика та же
- update_memory — логика та же

---

#### close_all — меняется полностью

Вместо CloseHandle на каждый объект синхронизации — pthread аналоги:

- pthread_mutex_destroy для mutex
- pthread_cond_destroy для каждого cond
- free для global_array и task_array
- для потоков — не CloseHandle, а pthread_join (но это в main, не в close_all)

Сигнатура close_all тоже меняется — вместо HANDLE* threads передаёшь pthread_t* threads.