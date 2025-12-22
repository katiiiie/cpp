#include "AiAgent.h"
#include <iostream>
#include <string>

int main(int argc, char **argv) {
    AiAgent teacher;
    std::string err;
    
    if (!teacher.loadConfig("config.json", &err)) {
        std::cerr << err << "\n";
        return 1;
    }

    if (!teacher.initDatabase("language_learning.db", &err)) {
        std::cerr << "Database error: " << err << "\n";
        return 1;
    }
       
    std::string language, level, topic;
    
    std::cout << "Welcome to Language Learning AI!\n";
    std::cout << "Enter language to learn (e.g., English, Spanish, French): ";
    std::getline(std::cin, language);
    
    std::cout << "Enter your level (beginner, intermediate, advanced): ";
    std::getline(std::cin, level);
    
    std::cout << "Enter topic you want to practice: ";
    std::getline(std::cin, topic);

    if (!teacher.createSession(language, level, topic, &err)) {
        std::cerr << "Session creation error: " << err << "\n";
        return 1;
    }

    std::cout << "\n=== Language Learning Session Started ===\n";
    std::cout << "Language: " << language << "\n";
    std::cout << "Level: " << level << "\n";
    std::cout << "Topic: " << topic << "\n";
    std::cout << "Type 'quit' to exit\n\n";

    std::string userInput = "";
    while (true) {
        std::cout << "You: ";
        std::getline(std::cin, userInput);
        
        if (userInput == "quit" || userInput == "exit") {
            //FIX
            teacher.stopLocalServer();
            break;
        }
    
        if (!userInput.empty()) {
            auto response = teacher.sendMessage(userInput, &err);
            
            if (!response) {
                std::cerr << "Error: " << err << "\n";
                continue;
            }

            std::cout << "Teacher: " << *response << "\n\n";
        }
    }

    std::cout << "Thank you for learning!\n";
    return 0;
}
