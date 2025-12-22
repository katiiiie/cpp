# Language Teacher AI Agent

Aгент для изучения иностранных языков с использованием больших языковых моделей (LLM).
Работает как с локальной моделью через `llama.cpp`, так и с удалённым API (Hurated).
Агент ведёт интерактивный обучающий диалог в роли учителя языка.


## Идея проекта

Цель проекта — создать простого AI-учителя языка, который:

* общается с пользователем в формате диалога на определенную тему
* подбирает новые слова/правила/примеры под конкретный запрос ученика
* адаптируется под уровень ученика
* мягко исправляет ошибки и даёт пояснения
* хранит историю обучения
* может работать полностью офлайн

Проект ориентирован на людей, самостоятельно изуяающих иностранный язык.

## Возможности

* Интерактивный режим обучения (чат)
* Выбор языка обучения
* Выбор уровня владения языком (beginner, intermediate, advanced)
* Хранение истории диалога в рамках одной сессии (SQLite)
* Два бэкенда:
  * Hurated API
  * Локальная модель через `llama-server`
* Конфигурация через `config.json`

## Установка и сборка
### Зависимости

* C++17 или новее
* nlohmann/json
* sqlite3
* OpenSSL
* llama.cpp (для локального режима)


### Установка локальной модели

1. Скачайте модель в формате GGUF
   Пример: `Qwen3-1.7B-Q4_K_M.gguf`
2. Поместите модель в папку `models/`
3. Проверьте путь в `config.json` (`local.model_path`)


### Сборка llama.cpp

```bash
git clone --depth 1 https://github.com/ggml-org/llama.cpp.git
cd llama.cpp
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DGGML_CUDA=OFF
cmake --build build -j"$(nproc)"
```

Убедитесь, что `llama-server` доступен в `PATH`.

---

### Сборка агента

```bash
mkdir build
cd build
cmake ..
make
```


## Настройка `config.json`


В папке build после сборки проекта создайте файл `config.json`:


```json
{
  "mode": "hurated",//или "local"
  "hurated": {
    "host": "ai-api.hurated.com",
    "port": "443",
    "api_key": "<ваш ключ>"
  },
    "local": {
    "backend": "path-to-llama.cpp-server",
    "model_path": "path-to-your-model.gguf",
    "context_length": 2048,
    "port": "8080",
    "extra_args": ""
  }
}
```


## Запуск

```bash
./language_teacher
```

После запуска агент создаёт учебную сессию и начинает диалог с пользователем.

## Примеры запуска

### Режим Hurated

```bash
./language_teacher config.json
Ожидаемый (сокращённый) вывод:

Welcome to Language Learning AI!
Enter language to learn (e.g., English, Spanish, French): English
Enter your level (beginner, intermediate, advanced): beginner
Enter topic you want to practice: traveling

=== Language Learning Session Started ===
Language: English
Level: beginner
Topic: traveling
Type 'quit' to exit

You: i want to compose a story abot my trip to New York. Give me some examples and suitable phrases

Teacher: Great idea! New York is an exciting place to write about.

Small corrections to your sentence:
- "i" → "I" (always capital I)
- "abot" → "about"
A clearer way to say it is: "I want to write a story about my trip to New York. Please give me some examples and suitable phrases."
Note: "write" is simpler and more common than "compose" in everyday English.

How to structure your story (simple and clear):
- Beginning: When did you go? Why did you go?
....
```
### Режим локальной модели
```bash
./language_teacher config.json
допустим вывод системной информации llama-сервера в консоль

Ожидаемый вывод:

Welcome to Language Learning AI!

build: with GNU 13.3.0 for Linux x86_64
system info:
system_info: 
init: using threads for HTTP server
start: binding port with default address family
main: loading model
srv    load_model: loading model './models/Qwen3-1.7B-Q4_K_M.gguf

Enter language to learn (e.g., English, Spanish, French): English
Enter your level (beginner, intermediate, advanced): beginner
Enter topic you want to practice: traveling

=== Language Learning Session Started ===
Language: English
Level: beginner
Topic: traveling
Type 'quit' to exit

You: i want to compose a story abot my trip to New York. Give me some examples and suitable phrases

srv  log_server_r: request: POST /completion 127.0.0.1 200
Teacher: <think>
Okay, the user wants to compose a story about their trip to New York. Let me think about how to help them. First, they're a beginner, so I need to keep the examples simple and useful. They might not be familiar with the structure of a story, so I should ...
</think>

Sure! Here are some examples of phrases and ideas to help you write a story about your trip to New York. Let me break it down for you:

---

### **1. Introduction (Why you went)**
- "I decided to go to New York because I wanted to experience a bustling city."
- "The thought of visiting New York filled me with excitement." ...