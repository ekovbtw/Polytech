#### Что копируешь из QSort без изменений

- чтение файла, fscanf для global_count_threads, N и элементов массива
- malloc для global_array и task_array
- malloc для массива потоков
- output_to_file и запись time.txt
- close_all в конце

---

#### Что меняется в инициализации объектов синхронизации

Вместо CreateMutex, CreateEvent:

- pthread_mutex_init(&mutex, NULL)
- pthread_cond_init(&cond_task, NULL)
- pthread_cond_init(&done_cond, NULL)

---

#### Что меняется в создании потоков

Вместо CreateThread — pthread_create. Сигнатура:

pthread_create(&threads[i], NULL, worker_thread, NULL)

---

#### Главное новое — цикл раундов

После создания потоков запускаешь цикл пока current_size_buffer_array <= N:

- сбрасываешь head, tail, count_add_task в 0
- добавляешь задачи через add_task — каждая задача это кусок размером current_size_buffer_array
- захватываешь mutex, будишь потоки через pthread_cond_broadcast(&cond_task)
- ждёшь завершения раунда через pthread_cond_wait(&done_cond, &mutex)
- отпускаешь mutex
- удваиваешь current_size_buffer_array

После цикла выставляешь merge_done = true и будишь все потоки через pthread_cond_broadcast чтобы они вышли из своих циклов.

---