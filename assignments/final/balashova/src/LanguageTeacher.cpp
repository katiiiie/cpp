#include "AiAgent.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <cstring>
#include <chrono>
#include <iomanip>

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <iostream>

using nlohmann::json;

AiAgent::AiAgent() {
    currentSession_.id = -1;
}

AiAgent::~AiAgent() {
    if (db_) {
        sqlite3_close(db_);
    }
}

bool AiAgent::initDatabase(const std::string& dbPath, std::string* err) {
    if (sqlite3_open(dbPath.c_str(), &db_) != SQLITE_OK) {
        if (err) *err = sqlite3_errmsg(db_);
        return false;
    }

    const char* createSessionsTable = R"(
        CREATE TABLE IF NOT EXISTS learning_sessions (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            language TEXT NOT NULL,
            level TEXT NOT NULL,
            topic TEXT NOT NULL,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    )";
    
    const char* createMessagesTable = R"(
        CREATE TABLE IF NOT EXISTS conversation_messages (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            session_id INTEGER NOT NULL,
            role TEXT NOT NULL,
            content TEXT NOT NULL,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (session_id) REFERENCES learning_sessions (id)
        )
    )";

    char* errorMsg = nullptr;
    if (sqlite3_exec(db_, createSessionsTable, nullptr, nullptr, &errorMsg) != SQLITE_OK) {
        if (err) *err = errorMsg;
        sqlite3_free(errorMsg);
        return false;
    }

    if (sqlite3_exec(db_, createMessagesTable, nullptr, nullptr, &errorMsg) != SQLITE_OK) {
        if (err) *err = errorMsg;
        sqlite3_free(errorMsg);
        return false;
    }

    return true;
}

bool AiAgent::createSession(const std::string& language, const std::string& level, 
                                   const std::string& topic, std::string* err) {
    const char* sql = "INSERT INTO learning_sessions (language, level, topic) VALUES (?, ?, ?)";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (err) *err = sqlite3_errmsg(db_);
        return false;
    }

    sqlite3_bind_text(stmt, 1, language.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, level.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, topic.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        if (err) *err = sqlite3_errmsg(db_);
        sqlite3_finalize(stmt);
        return false;
    }

    currentSession_.id = sqlite3_last_insert_rowid(db_);
    currentSession_.language = language;
    currentSession_.level = level;
    currentSession_.topic = topic;

    std::string welcomeMsg = "Hello! I'm your " + language + " teacher. We'll be practicing " + 
                           topic + " at " + level + " level. How can I help you today?";

    if (!addMessageToHistory("assistant", welcomeMsg, err)) {
        return false;
    }

    sqlite3_finalize(stmt);
    return true;
}

bool AiAgent::addMessageToHistory(const std::string& role, const std::string& content, std::string* err) {
    if (currentSession_.id == -1) {
        if (err) *err = "No active session";
        return false;
    }

    const char* sql = "INSERT INTO conversation_messages (session_id, role, content) VALUES (?, ?, ?)";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (err) *err = sqlite3_errmsg(db_);
        return false;
    }

    sqlite3_bind_int(stmt, 1, currentSession_.id);
    sqlite3_bind_text(stmt, 2, role.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, content.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        if (err) *err = sqlite3_errmsg(db_);
        sqlite3_finalize(stmt);
        return false;
    }

    sqlite3_finalize(stmt);
    return true;
}

std::string AiAgent::getConversationHistory(std::string* err) {
    if (currentSession_.id == -1) {
        if (err) *err = "No active session";
        return "";
    }
    
    const char* sql;
    if (cfg_.mode == "hurated") {
        sql = R"(
            SELECT role, content
                FROM (
                    SELECT *
                    FROM conversation_messages
                    ORDER BY timestamp DESC
                    LIMIT 3
                ) AS t
                WHERE session_id = (
                    SELECT id 
                    FROM learning_sessions 
                    ORDER BY created_at DESC 
                    LIMIT 1
                )
                ORDER BY timestamp ASC
            )";
    } else {
        sql = R"(
            SELECT role, content
                FROM (
                    SELECT *
                    FROM conversation_messages
                    ORDER BY timestamp DESC
                    LIMIT 1
                ) AS t
                WHERE session_id = (
                    SELECT id 
                    FROM learning_sessions 
                    ORDER BY created_at DESC 
                    LIMIT 1
                )
                ORDER BY timestamp ASC
        )";
    }
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (err) *err = sqlite3_errmsg(db_);
        return "";
    }

    sqlite3_bind_int(stmt, 1, currentSession_.id);
    
    std::string history;
    if (cfg_.mode == "local") {
        history += "<|im_start|>system\n" + generateSystemPrompt() + "<|im_end|>\n";
    } else {
        history = "system: " + generateSystemPrompt();
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::string role = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        std::string content = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        
        //history += role + ": " + content + "\n";
        
        if (cfg_.mode == "local") {
            history += "<|im_start|>" + role + "\n" + content + "<|im_end|>\n";
        } else {
            history += role + ": " + content + "\n";
        }
        
    }

    sqlite3_finalize(stmt);
    if (cfg_.mode == "local") {
        history += "<|im_start|>assistant\n";
    } else {
        history += "assistant: ";
    }
    return history;
}

std::string AiAgent::generateSystemPrompt() const {
    std::string sys_prompt = "You are a friendly " + currentSession_.language + " language teacher. " +
           "The student is at " + currentSession_.level + " level and wants to practice: " + 
           currentSession_.topic + ". ";
    if (cfg_.mode == "hurated") {
        sys_prompt += "Please:\n"
           "1. Respond naturally in " + currentSession_.language + "\n"
           "2. Correct mistakes gently and provide explanations, don't pay attention to the register in sentences.\n"
           "3. Use appropriate vocabulary for their level\n"
           "4. Encourage conversation and ask follow-up questions\n"
           "5. Provide examples and practice exercises when helpful\n";
    }
    return sys_prompt;
}

bool AiAgent::startLocalServer(std::string* err) {
    
    std::string cmd = cfg_.local.backend + 
                     " -m \"" + cfg_.local.model_path + "\"" +
                     " -c " + std::to_string(cfg_.local.context_length) +
                     " --port " + cfg_.local.port +
                     " --host 127.0.0.1" +
                     " -np 4";
    
    if (!cfg_.local.extra_args.empty()) cmd += " " + cfg_.local.extra_args;
    
    int pipefd[2];
    if (pipe(pipefd) != 0) {
        if (err) *err = "pipe failed";
        return false;
    }

    cfg_.local.server_pid = fork();
    if (cfg_.local.server_pid == -1) {
        if (err) *err = "fork failed";
        return false;
    }

    if (cfg_.local.server_pid == 0) {
        // child process
        close(pipefd[0]);
        execl("/bin/sh", "sh", "-c", cmd.c_str(), (char*)NULL);
        _exit(127);
    } else {
        // parent process
        close(pipefd[1]);
        if (waitpid(cfg_.local.server_pid, NULL, WNOHANG) == cfg_.local.server_pid) {
            if (err) *err = "server failed to start";
            cfg_.local.server_pid = -1;
            return false;
        }
        
        cfg_.local.server_running = true;
        return true;
    }
}
    
void AiAgent::stopLocalServer() {
    if (cfg_.local.server_running) {
        kill(cfg_.local.server_pid, SIGTERM);

        int status;
        waitpid(cfg_.local.server_pid, &status, 0);

        cfg_.local.server_running = false;
    }
}

std::optional<std::string> AiAgent::localPostGenerate(
        const LocalCfg& cfg, const std::string& prompt, std::string* err) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        if (err) *err = "socket failed";
        return std::nullopt;
    }

    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo("127.0.0.1", cfg.port.c_str(), &hints, &res) != 0) {
        if (err) *err = "getaddrinfo failed";
        close(sock);
        return std::nullopt;
    }

    if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
        if (err) *err = "connect failed (local server not running?)";
        freeaddrinfo(res);
        close(sock);
        return std::nullopt;
    }
    freeaddrinfo(res);
    
    json payload = {
        {"prompt", prompt}//,
        //{"stop", json::array({"<|im_end|>"})}
    };

    
    std::string body = payload.dump();

    std::ostringstream req;
    req << "POST /completion HTTP/1.1\r\n"
        << "Host: 127.0.0.1\r\n"
        << "Content-Type: application/json\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Connection: close\r\n\r\n"
        << body;

    std::string request = req.str();
    //std::cout << req.str() << "\n";
    send(sock, request.c_str(), request.size(), 0);
    
    std::string response;
    char buf[4096];
    ssize_t n;
    while ((n = recv(sock, buf, sizeof(buf) - 1, 0)) > 0) {
        buf[n] = 0;
        response += buf;
    }
    close(sock);

    auto pos = response.find("\r\n\r\n");
    if (pos == std::string::npos) {
        if (err) *err = "Invalid HTTP response";
        return std::nullopt;
    }

    std::string json_part = response.substr(pos + 4);

    try {
        auto j = json::parse(json_part);
        return j.at("content").get<std::string>();
    } catch (...) {
        if (err) *err = "Failed to parse llama.cpp response";
        return std::nullopt;
    }
}


std::optional<std::string> AiAgent::sendMessage(const std::string& message, std::string* outErr) {
    if (currentSession_.id == -1) {
        if (outErr) *outErr = "No active learning session";
        return std::nullopt;
    }

    if (!addMessageToHistory("user", message, outErr)) {
        return std::nullopt;
    }

    prompt_ = getConversationHistory(outErr);
    
    if (prompt_.empty()) {
        if (outErr) *outErr = "Failed to get conversation history";
        return std::nullopt;
    }
    
    std::optional<std::string> response;
    
    if (cfg_.mode == "local") {
        response = localPostGenerate(cfg_.local, prompt_, outErr);    
    } else {
        json payload = { {"prompt", prompt_} };
        const std::string body = payload.dump();
        response = httpsPostGenerate(cfg_.hurated, body, outErr);
    }

    
    if (response) {
        if (!addMessageToHistory("assistant", *response, outErr)) {
            return std::nullopt;
        }
    }

    return response;
}