#pragma once
#include <string>
#include <optional>
#include <nlohmann/json.hpp>
#include <sqlite3.h>

struct AiConfig {
    std::string host;
    std::string port = "443";
    std::string api_key;

};

struct LearningSession {
    int id;
    std::string language;
    std::string level;
    std::string topic;
    std::string created_at;
};

class AiAgent {
public:
    AiAgent();
    ~AiAgent();

    // Загрузить конфиг (host, port, api_key) из JSON-файла
    bool loadConfig(const std::string& path, std::string* err = nullptr);
    
    // Загрузить промпт из JSON-файла (принимает либо строку, либо объект с ключом "prompt")
    bool loadPrompt(const std::string& path, std::string* err = nullptr);

    // Выполнить запрос и вернуть распарсенный "text" из ответа
    // Возвращает std::nullopt при ошибке (описание в outErr, если передан)
    std::optional<std::string> ask(std::string* outErr = nullptr) const;

    // Явно задать промпт программно (не из файла)
    void setPrompt(std::string p) { prompt_ = std::move(p); }

    // Инициализировать базу данных
    bool initDatabase(const std::string& dbPath, std::string* err = nullptr);

    // Создать новую сессию обучения
    bool createSession(const std::string& language, const std::string& level, 
                      const std::string& topic, std::string* err = nullptr);

    // Получить историю диалога для текущей сессии
    std::string getConversationHistory(std::string* err = nullptr);

    // Отправить сообщение ученика и получить ответ от ИИ
    std::optional<std::string> sendMessage(const std::string& message, std::string* outErr = nullptr);

    // Получить текущую активную сессию
    const LearningSession& getCurrentSession() const { return currentSession_; }

private:
    // Низкоуровневый HTTPS POST запрос
    static std::optional<std::string> httpsPostGenerate(
        const AiConfig& cfg, const std::string& jsonBody, std::string* err);

    // Извлечь текст из JSON ответа
    static std::string extractTextFromJsonBody(const std::string& body);

    // Прочитать файл целиком
    static bool readWholeFile(const std::string& path, std::string& out, std::string* err);

    // Добавить сообщение в историю диалога
    bool addMessageToHistory(const std::string& role, const std::string& content, std::string* err = nullptr);

    // Сгенерировать системный промпт для обучения языку
    std::string generateSystemPrompt() const;

private:
    AiConfig cfg_;
    sqlite3* db_ = nullptr;
    LearningSession currentSession_;
    std::string prompt_;
};
