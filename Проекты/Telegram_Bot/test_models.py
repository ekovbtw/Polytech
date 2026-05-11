
import requests
import time

import os
from dotenv import load_dotenv
load_dotenv()
API_KEY = os.getenv("OPENROUTER_API_KEY", "")

# Список моделей для теста (из вашего списка, только не Google — они под лагом)
MODELS_TO_TRY = [
    "meta-llama/llama-3.3-70b-instruct:free",
    "qwen/qwen3-next-80b-a3b-instruct:free",
    "meta-llama/llama-3.2-3b-instruct:free",
    "nvidia/nemotron-3-super-120b-a12b:free",
    "cognitivecomputations/dolphin-mistral-24b-venice-edition:free",
    "nousresearch/hermes-3-llama-3.1-405b:free",
    "liquid/lfm-2.5-1.2b-instruct:free",
    "nvidia/nemotron-nano-9b-v2:free",
]

SYSTEM_PROMPT = (
    "Ты — ассистент тайм-менеджмента для русскоязычного студента. "
    "Твоя задача — оценить, сколько минут займёт выполнение задачи. "
    "Учитывай, что пользователь — средний студент, не отличник и не двоечник. "
    "Отвечай ТОЛЬКО целым числом минут. Без пояснений, без слов, только число."
)

TEST_TASKS = [
    "решить 10 задач по математике",
    "прочитать параграф по истории и сделать конспект",
    "убраться в квартире: пропылесосить, помыть полы, протереть пыль",
    "приготовить ужин: паста с соусом и салат",
    "помыться и собраться на учебу",
]

def test_model(model_name):
    print(f"\n{'='*50}")
    print(f"ТЕСТ: {model_name}")
    print(f"{'='*50}")
    
    for task in TEST_TASKS:
        response = requests.post(
            "https://openrouter.ai/api/v1/chat/completions",
            headers={
                "Authorization": f"Bearer {API_KEY}",
                "Content-Type": "application/json"
            },
            json={
                "model": model_name,
                "messages": [
                    {"role": "system", "content": SYSTEM_PROMPT},
                    {"role": "user", "content": f"Оцени в минутах: {task}"}
                ],
                "max_tokens": 10,
                "temperature": 0.1
            }
        )
        
        if response.status_code == 200:
            answer = response.json()["choices"][0]["message"]["content"].strip()
            print(f"  ✅ '{task}' -> {answer} мин.")
        else:
            error_code = response.status_code
            error_msg = response.json().get("error", {}).get("message", "?")
            print(f"  ❌ Ошибка {error_code}: {error_msg[:80]}")
            return False  # модель не работает, переходим к следующей
        time.sleep(0.5)  # чтобы не заспамить
    
    return True  # все 5 задач прошли

# Запуск
for model in MODELS_TO_TRY:
    if test_model(model):
        print(f"\n\n✅✅✅ РАБОЧАЯ МОДЕЛЬ: {model}")
        print("Используй её в проекте!")
        break
    else:
        print(f"   >>> {model} не подходит, пробую следующую...")
        time.sleep(1)
else:
    print("\n❌ Ни одна модель не сработала. Попробуй позже или используй DeepSeek.")
