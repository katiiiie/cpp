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
    
    /*if (!addMessageToHistory("system", generateSystemPrompt(), err)) {
        return false;
    }*/

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

    const char* sql = R"(
        SELECT role, content FROM conversation_messages 
        WHERE session_id = ?
        ORDER BY timestamp ASC
        LIMIT 5
    )";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (err) *err = sqlite3_errmsg(db_);
        return "";
    }

    sqlite3_bind_int(stmt, 1, currentSession_.id);
    
    std::string history = generateSystemPrompt();
    //prompt_ = generateSystemPrompt();
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::string role = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        std::string content = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        
        history += role + ": " + content + "\n";
        //prompt_ += role + ": " + content + "\n";
        
    }

    sqlite3_finalize(stmt);
    return history;
}

std::string AiAgent::generateSystemPrompt() const {
    return "You are a friendly " + currentSession_.language + " language teacher. " +
           "The student is at " + currentSession_.level + " level and wants to practice: " + 
           currentSession_.topic + ". " +
           "Please:\n"
           "1. Respond naturally in " + currentSession_.language + "\n"
           "2. Correct mistakes gently and provide explanations, don't pay attention to the register in sentences.\n"
           "3. Use appropriate vocabulary for their level\n"
           "4. Encourage conversation and ask follow-up questions\n"
           "5. Provide examples and practice exercises when helpful\n";
}

std::optional<std::string> AiAgent::sendMessage(const std::string& message, std::string* outErr) {
    if (currentSession_.id == -1) {
        if (outErr) *outErr = "No active learning session";
        return std::nullopt;
    }

    if (!addMessageToHistory("user", message, outErr)) {
        return std::nullopt;
    }

    //std::string fullPrompt = getConversationHistory(outErr);
    prompt_ = getConversationHistory(outErr);
    if (prompt_.empty()) {
        if (outErr) *outErr = "Failed to get conversation history";
        return std::nullopt;
    }

    prompt_ += "assistant: ";

    json payload = { {"prompt", prompt_} };
    const std::string body = payload.dump();

    auto response = httpsPostGenerate(cfg_, body, outErr);
    
    if (response) {
        if (!addMessageToHistory("assistant", *response, outErr)) {
            return std::nullopt;
        }
    }

    return response;
}