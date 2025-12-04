# AI Agent CLI Helper

## Идея
Универсальный AI-агент с CLI интерфейсом для помощи в повседневных задачах программирования. Агент использует AI API для ответов на команды пользователя в различных режимах работы.

## Возможности
- **Обычный режим** - базовый AI-ассистент
- **CLI режим** - интерактивный помощник с поддержкой команд
- **Мульти-режимы**:
  - `help` - справка по командам
  - `todo` - управление задачами, органайзер
  - `summary` - суммаризация текста из файлов
  - `ideas` - генерация творческих идей
  - `planner` - помощь в планировании

## Настройка
Интерактивный режим
./ai_agent --cli

Одна команда
./ai_agent --cli "помоги настроить git"

Специфический режим
./ai_agent --cli --mode todo "добавь задачу: подготовить отчет"
./ai_agent --cli --mode ideas "идеи для проекта на C++"

Суммаризация файла из папки build (всегда надо добавлять лишнюю точку в начале)
./ai_agent --cli --mode summary --file .../examples/sample.txt

Помощь
./ai_agent --cli --help

## Контекст и история разговоров

Агент поддерживает сохранение контекста разговора в SQLite базе данных:

```bash
# Включить контекст для сессии
./ai_agent --cli --enable-context project1

# Показать историю текущей сессии
./ai_agent --cli --show-context

# Очистить историю
./ai_agent --cli --clear-context

# Выключить контекст
./ai_agent --cli --disable-context


## Сборка и тестирование

```bash
cd build
# Убедитесь, что установлен sqlite3
sudo apt-get install libsqlite3-dev  # для Ubuntu/Debian

cmake ..
make -j

# Тестируем новый функционал
./ai_agent --cli --enable-context test_session
После этого должен включиться интерактивный режим. Если этого вдруг не произошло, то работаем как обычно:
./ai_agent --cli "привет! запомни что я люблю программирование"
./ai_agent --cli "напомни что я люблю?"
./ai_agent --cli --show-context

Если интерактивный режим включился, то просто сообщение по умолчанию будет считать как промпт:
привет! запомни что я люблю программирование
напомни что я люблю?
Чтобы подать ключи, пишем их без -- (например, --show-context пишем как show-context)

## Установка и сборка

```bash
git clone <ваш-репозиторий>
cd ai_agent_example
mkdir build && cd build
cmake ..
make -j

## Создание сервера

Либо запускаем скрипт, либо
```bash
rm -rf build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j$(nproc)
./llama.cpp/build/bin/llama-server   -m llama.cpp/models/tinyllama-1.1b-chat-v1.0.Q2_K.gguf   -c 4096 -ngl 999   --host 127.0.0.1 --port 8080

## Обращение к локальному серверу (из другого терминала)

```bash
curl -s http://127.0.0.1:8080/v1/chat/completions   -H "Content-Type: application/json"   -d '{
    "model": "local-gguf",
    "messages": [
      {"role":"system","content":"You are a helpful coding assistant."},
      {"role":"user","content":"Поясни в двух предложениях, что такое цикл в C++."}
    ],
    "max_tokens": 200,
    "temperature": 0.7,
    "top_p": 0.9
  }' | jq -r '.choices[0].message.content'




build/./ai_agent --cli --local "расскажи про искусственный интеллект в двух предложениях"

# Тестируем удаленный API
build/./ai_agent --cli --remote "расскажи про искусственный интеллект в двух предложениях"

build/./ai_agent --cli --model-info