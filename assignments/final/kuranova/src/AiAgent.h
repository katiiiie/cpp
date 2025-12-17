#pragma once
#include <string>
#include <optional>
#include <nlohmann/json.hpp>

struct AiConfig {
    std::string host;
    std::string port = "443";
    std::string api_key;
    int mcount;
    bool is_loc;
};

class AiAgent {
public:
    // Загрузить конфиг (host, port, api_key) из JSON-файла
    int loadConfig(const std::string& path, std::string* err = nullptr, std::string tp="");

    // Загрузить промпт из JSON-файла (принимает либо строку, либо объект с ключом "prompt")
    bool loadPrompt(const std::string& path, std::string* err = nullptr);

    // Выполнить запрос и вернуть распарсенный "text" из ответа
    // Возвращает std::nullopt при ошибке (описание в outErr, если передан)
    std::optional<std::string> ask(std::string* outErr = nullptr) const;

    // Явно задать промпт программно (не из файла)
    void setPrompt(std::string p) { prompt_ = std::move(p); }

private:
    // ---- низкоуровневые помощники ----
    static std::optional<std::string> httpsPostGenerate(
        const AiConfig& cfg, const std::string& jsonBody, std::string* err);

    // Простой разбор JSON: ожидаем { "text": "<строка>" }
    static std::string extractTextFromJsonBody(const std::string& body);

    static bool readWholeFile(const std::string& path, std::string& out, std::string* err);

private:
    AiConfig cfg_;
    std::string prompt_;
};
