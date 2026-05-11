import requests

import os
from dotenv import load_dotenv
load_dotenv()
API_KEY = os.getenv("OPENROUTER_API_KEY", "")

# 1. Получаем список ВСЕХ моделей
print("Получаю список моделей...")
r = requests.get(
    "https://openrouter.ai/api/v1/models",
    headers={"Authorization": f"Bearer {API_KEY}"}
)

if r.status_code != 200:
    print(f"Ошибка получения списка: {r.status_code}")
    print(r.json())
    exit()

models = r.json()["data"]

# 2. Фильтруем бесплатные
free_models = []
for m in models:
    prompt_price = float(m["pricing"]["prompt"])
    completion_price = float(m["pricing"]["completion"])
    if prompt_price == 0 and completion_price == 0:
        free_models.append(m["id"])

print(f"\nНайдено {len(free_models)} бесплатных моделей:\n")
for fm in free_models:
    print(f"  • {fm}")

# 3. Пробуем первую из списка
if not free_models:
    print("Нет бесплатных моделей.")
    exit()

test_model = free_models[0]
print(f"\nТестирую модель: {test_model}")

response = requests.post(
    "https://openrouter.ai/api/v1/chat/completions",
    headers={
        "Authorization": f"Bearer {API_KEY}",
        "Content-Type": "application/json"
    },
    json={
        "model": test_model,
        "messages": [
            {"role": "user", "content": "Скажи: ТЕСТ ПРОЙДЕН"}
        ],
        "max_tokens": 20
    }
)

print(f"Статус: {response.status_code}")

if response.status_code == 200:
    answer = response.json()["choices"][0]["message"]["content"]
    print(f"Ответ модели: {answer}")
    print("\n✅ Модель работает! Используй её для проекта.")
else:
    print(f"Ошибка: {response.json()}")