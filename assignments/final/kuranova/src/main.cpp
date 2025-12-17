
#include "AiAgent.h"
#include <iostream>
#include <fstream>
#include <queue>   
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sqlite3.h>
#include <chrono>
#include <thread>

using nlohmann::json;

using std::string;
using std::endl;

int open_file(string path, string * err, string *data) {
	std::ifstream f(path, std::ios::binary);
    //std::cout << path;
    if (!f) { if (err) *err = "Cannot open file: " + path; return false; }
    std::ostringstream ss; ss << f.rdbuf();
    *data = std::move(ss).str();
    f.close();
    return true;
	
	}
	
	
class BD {
	sqlite3* db;
	char* err;
	public:
	BD() { //string bd_path) {

		int result = sqlite3_open("my_db.db", &db);  // Поменять потом !!!!
		    // Создание таблицы
		const char* createTableSQL = 
			"CREATE TABLE IF NOT EXISTS message ("
			"id INTEGER PRIMARY KEY AUTOINCREMENT,"
			"message_text TEXT NOT NULL,"
			"date DATETIME DEFAULT CURRENT_TIMESTAMP,"
			"user INTEGER DEFAULT 0,"
			"type INTEGER CHECK (type IN (0, 1)) DEFAULT 0"
			");";
		
		result = sqlite3_exec(db, createTableSQL, nullptr, nullptr, &err);
		
		if (result != SQLITE_OK) {
			std::cerr << "Ошибка создания таблицы: " << err << std::endl;
			sqlite3_free(err);
		} else {
			//std::cout << "Таблица 'message' создана успешно!" << std::endl;
		}
	}
    
		/*if (result != SQLITE_OK) {
			*err =  "Не удалось открыть базу данных";
		}*/
		
	~BD() {
		sqlite3_close(db);
		}
		
	int insert_text(string data, int tp) {
		string q = 
        "INSERT INTO message (message_text, type) VALUES ('" + data + "', " + std::to_string(tp) + ");";
		
		int result = sqlite3_exec(db, q.c_str(), nullptr, nullptr, &err);
		if (result != SQLITE_OK) {
			std::cerr << "Ошибка вставки сообщения" << err << std::endl;
			sqlite3_free(err);
        return -1;
		} 
		//else {
			//std::cout << "Сообщние записано" << std::endl;
		//}
		return 0;
	}
	static int callback(void* data, int argc, char ** argv, char ** colName) {
		string* result = static_cast<string*>(data);
		string type = (string(argv[4]) == "1") ? "user" : "system";
		string message = argv[1];
		
		if (!result->empty()) *result += ", ";
		*result += type + ": " + message;
		
		return 0;
	}
	string get_lastN(int count) {
		string result;
		string sql = "SELECT * FROM ("
					" SELECT * FROM message ORDER BY id DESC LIMIT " + std::to_string(count) +
					" ) AS last_m" 
					" ORDER BY id ASC;";
		sqlite3_exec(db, sql.c_str(), callback, &result, NULL);
		
		// std::cout << "===========================================================================================================================================" +  result << std::endl;

		return result;
	}
};
	
	

pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t db_mtx = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t cv_in  = PTHREAD_COND_INITIALIZER;
pthread_cond_t cv_out  = PTHREAD_COND_INITIALIZER;
string text_path = "../text.json", prompt_path = "../prompt.json", conf_path = "../config.json";
string err, data, tp;
int mcount;


struct Param {
	int * is_exit;
	bool * is_input;
	std::chrono::time_point<std::chrono::steady_clock> * last_time;
	int * time;
	int type;
    AiAgent * agent;
    BD * db;
    pthread_mutex_t * mtx;
	};


/*
void in_func(void * par){
	struct Param * param = (struct Param *)par;
	while (!param->is_exit){
		pthread_mutex_lock(param->mtx);
		while(!param->is_input){
			pthread_cond_signal(&cv_in);
			pthread_cond_wait(&cv_out, param->mtx);
			
			}
		
		pthread_cond_signal(&cv_in);
		pthread_mutex_unlock(param->mtx);
	}
}
	*/
	

void remember(string tp1, int time, string last_str) {
	json new_j;
	auto j = json::parse(data);
    string work1 = j.at(tp + "remember").get<std::string>();
    
    new_j["prompt"] = work1 + last_str + "| последнее сообщение было " + std::to_string(time) + tp1 + " назад.";
    std::ofstream f(prompt_path);
    f << new_j.dump(4);
    f.close();
	
	}

void * out_f(void * par){
	struct Param * param = (struct Param *)par;
    
	while (*(param->is_exit) != 1){
        auto currentTime = std::chrono::steady_clock::now();
        auto lastTime = *(param->last_time);
        auto elapsed_h = std::chrono::duration_cast<std::chrono::hours>(currentTime - lastTime);
		auto elapsed_m = std::chrono::duration_cast<std::chrono::minutes>(currentTime - lastTime);
        auto elapsed_s = std::chrono::duration_cast<std::chrono::seconds>(currentTime - lastTime);

		if ((param->type == 0) and (elapsed_h.count() >= *(param->time))) {
			pthread_mutex_lock(param->mtx);
			//std::cout << "№№№№№№№№№№№№№№DEADLOCK" << endl;
			string last_message = (*(param->db)).get_lastN(mcount);
			remember("часов", elapsed_h.count(), last_message);
			if (!(*(param->agent)).loadPrompt(prompt_path, &err)) {
				std::cerr << "Prompt error: " << err << "\n";
				pthread_mutex_unlock(param->mtx);
				return 0;
			}
			auto resp = (*(param->agent)).ask(&err);
			if (!resp) {
				std::cerr << "Request failed: " << err << "\n";
				pthread_mutex_unlock(param->mtx);
				return 0;
			}
			std::cout << endl << "<system>" << *resp << endl;
			
			*(param ->last_time) = currentTime;
			//pthread_mutex_lock(&db_mtx);
			(*(param->db)).insert_text(*resp, 1);
			//pthread_mutex_unlock(&db_mtx);
			pthread_mutex_unlock(param->mtx);
				
			fflush(stdout);
		} else if ((param->type == 1) and (elapsed_m.count() >= *(param->time))) {
			pthread_mutex_lock(param->mtx);
			string last_message = (*(param->db)).get_lastN(mcount);
			remember("минут", elapsed_m.count(), last_message);
			if (!(*(param->agent)).loadPrompt(prompt_path, &err)) {
				std::cerr << "Prompt error: " << err << "\n";
				return 0;
			}
			auto resp = (*(param->agent)).ask(&err);
			if (!resp) {
				std::cerr << "Request failed: " << err << "\n";
				pthread_mutex_unlock(param->mtx);
				return 0;
			}
			std::cout << endl << "<system>" << *resp << endl;
			
			*(param ->last_time) = currentTime;
			//std::cout << "+++++++++++++++++++++++++++++++DEADLOCK" << endl;
			pthread_mutex_lock(&db_mtx);
			//std::cout << "______________________________+++++++++++++++++++++++++++++++DEADLOCK" << endl;
			(*(param->db)).insert_text(*resp, 1);
			pthread_mutex_unlock(&db_mtx);
			pthread_mutex_unlock(param->mtx);
			//std::cout << "end!!!" << endl;
			fflush(stdout);
		} else if ((param->type > 1) and (elapsed_s.count() >= *(param->time))) {
			pthread_mutex_lock(param->mtx);
			//std::cout << ";;;;;;;;;;;;;;;;;;;;DEADLOCK" << endl;
			string last_message = (*(param->db)).get_lastN(mcount);

			remember("секунд", elapsed_h.count(), last_message);
			if (!(*(param->agent)).loadPrompt(prompt_path, &err)) {
				std::cerr << "Prompt error: " << err << "\n";
				pthread_mutex_unlock(param->mtx);
				return 0;
			}
			auto resp = (*(param->agent)).ask(&err);
			if (!resp) {
				std::cerr << "Request failed: " << err << "\n";
				pthread_mutex_unlock(param->mtx);
				return 0;
			}
			std::cout << endl << "<system>" << *resp << endl;
			std::cout << "<user> ";
			
			*(param ->last_time) = currentTime;
			pthread_mutex_lock(&db_mtx);
			(*(param->db)).insert_text(*resp, 1);
			pthread_mutex_unlock(&db_mtx);
			pthread_mutex_unlock(param->mtx);
				
			fflush(stdout);
		}
	
        std::this_thread::sleep_for(std::chrono::seconds(15));
        //std::cout << "type: " << (param->type == 1) << " exit " << *(param->is_exit) << " " << (elapsed_m.count() >= *(param->time)) << " " << elapsed_m.count() << (*(param->time)) <<endl;
		fflush(stdout);
    }
    
    return 0;
}

void write_prompt(string cur_str, string last = ""){
	json new_j;
	auto j = json::parse(data);
    string work1 = j.at(tp+ "work1").get<std::string>();
    string work2 = j.at(tp + "work2").get<std::string>();
    string is_e = j.at(tp + "is_end").get<std::string>();
    
    new_j["prompt"] = work1 + last + work2 + cur_str + is_e;
    std::ofstream f(prompt_path);
    f << new_j.dump(4);
    f.close();
	}
	
	
void is_end(string cur_str) {
	json new_j;
	auto j = json::parse(data);
    string work1 = j.at(tp + "is_end").get<std::string>();
    new_j["prompt"] = cur_str + "|" + work1;
    std::ofstream f(prompt_path);
    f << new_j.dump(4);
    f.close();
	}



void * func(void * par){
	struct Param * param = (struct Param *)par;
	auto currentTime = std::chrono::steady_clock::now();
    AiAgent * agent = param->agent;
    int is_exit = 0;
    
	pthread_mutex_lock(param->mtx);
	*(param->is_exit) = is_exit;
	*(param->last_time) = currentTime;
	pthread_mutex_unlock(param->mtx);
    
    string user_answer;
    string last_message;
    
    //std::cout << "<user> ";
    //std::getline(std::cin, user_answer);
       
    
    while (is_exit != 1) {
		std::cout << "<user> ";
		std::getline(std::cin, user_answer);
		
		pthread_mutex_lock(param->mtx);
		pthread_mutex_lock(&db_mtx);
		last_message = (*(param->db)).get_lastN(mcount);
		(*(param->db)).insert_text(user_answer, 1);
		pthread_mutex_unlock(&db_mtx);
		write_prompt(user_answer, last_message);
		if (!(*agent).loadPrompt(prompt_path, &err)) {
			std::cerr << "Prompt error: " << err << "\n";
			return 0;
		}
		auto resp = (*agent).ask(&err);
		if (!resp) {
			std::cerr << "Request failed: " << err << "\n";
			return 0;
		}
		string check = *resp;
		if (tp == "") {
			// std::cout << "<system> " << check << endl;
			
			auto j = json::parse(check);
			string txt_answr = j.at("text").get<std::string>();
			std::cout << "<system> " << txt_answr << endl;

			is_exit = j.at("flag").get<int>();
			*(param->is_exit) = is_exit;
			if (!is_exit){
				(*(param->db)).insert_text(txt_answr, 0);
				} 
		}
		else {
			std::cout << "<system> " << check << endl;
			(*(param->db)).insert_text(check, 0);
			
			}
		fflush(stdout);
		pthread_mutex_unlock(param->mtx);
		
		
		//std::cout << "!!!!!!!!!!!!!!!!!!!!!!!!!!!test" << endl << endl << endl;
		/*if (tp == "") {
			pthread_mutex_lock(param->mtx);
			currentTime = std::chrono::steady_clock::now();
			*(param->last_time) = currentTime;
			is_end(user_answer);
			if (!(*agent).loadPrompt(prompt_path, &err)) {
				std::cerr << "Prompt error: " << err << "\n";
				return 0;
			}
			resp = (*agent).ask(&err);
			if (!resp) {
				std::cerr << "Request failed: " << err << "\n";
				return 0;
			}
			//std::cout << "!!!!!!!!!!!!!!!!!!!!!!!!!!!test" << endl << endl << endl;
			string check = *resp;
			// std::cout << check << endl;
			auto j = json::parse(check);
			is_exit = j.at("flag").get<int>();
			if (is_exit)
				std::cout << j.at("text").get<std::string>() << endl;
			*(param->is_exit) = is_exit;
			pthread_mutex_unlock(param->mtx);
		}*/
	}

	*(param->is_exit) = 1;
	fflush(stdout);
	return 0;
}



int main(int argc, char **argv)
{
    AiAgent agent;
	string cur_str;
	BD db;
	
	bool is_input = false;
	tp = "";
	if (argc != 1) {
		if (strcmp("--local", argv[1]) == 0) {
			tp = "local_";
			std::cout << "Local mode:" << endl << "Чтобы закончить, используйте клавиши ctrl + c" << endl;
			}
		else if (strcmp("--online", argv[1]) == 0) {
			std::cout << "Online mode:" << endl;
			}
		else {
			std::cout << "Справка:" << endl << "Для онлайн запуска ии-агента введите ./ai_agent" << endl << "Для Офлайн запуска введите ./ai_agent --local";
			return 0;
			}
		}
	else {
		//std::cout << "Online mode" << endl;
		string conf;
		if (!open_file(conf_path, &err, &conf)) {
			std::cout << err;
			return 0;
			}

		auto j = json::parse(conf);
		auto cur_str = j.at("mode").get<std::string>().c_str();
		if (strcmp("local", cur_str) == 0) {
			tp = "local_";
			std::cout << "Local mode:" << endl << "Чтобы закончить, используйте клавиши ctrl + c" << endl;
			}
		else if (strcmp("online", cur_str) == 0) {
			std::cout << "Online mode:" << endl;
			}
		else {
			std::cout << "Справка:" << endl << "Для онлайн запуска ии-агента введите ./ai_agent" << endl << "Для Офлайн запуска введите ./ai_agent --local";
			return 0;
			}
		}
	
    if ((mcount = agent.loadConfig(conf_path, &err, tp)) == 0) {
        std::cerr << "Config error: " << err << "\n";
        return 1;
    }

	
	if (!open_file(text_path, &err, &data)) {
		std::cout << err;
		return 0;
		}

	auto j = json::parse(data);
    cur_str = j.at(tp + "start").get<std::string>();
    json new_j;
    new_j["prompt"] = cur_str;
    std::ofstream f(prompt_path);
    f << new_j.dump(4);
    f.close();
    
   // std::cout << "!!!" << endl;
    
    
	if (!agent.loadPrompt(prompt_path, &err)) {
		std::cerr << "Prompt error: " << err << "\n";
		return 1;
    }
    auto resp = agent.ask(&err);
    if (!resp) {
        std::cerr << "Request failed: " << err << "\n";
        return 2;
    }
	
	db.insert_text(*resp, 0);
    std::cout << "<system>" << *resp << "\n";
    
    pthread_t in_th, out_th;
    
    int ex = 0;
    auto currentTime = std::chrono::steady_clock::now();
    int tim = 5;
    struct Param m;
    m.is_exit = &ex;
    m.type = 1;
    m.last_time = &currentTime;
    m.time = &tim;
    m.agent = &agent;
    m.db = &db;
    m.mtx = &mtx;
    
    
	pthread_create(&in_th, NULL, func, &m);
	if (tp == "") {
		pthread_create(&out_th, NULL, out_f, &m);
	}
    
	pthread_join(in_th, NULL);	
	if (tp == "") {
		pthread_join(out_th, NULL);	
    }
    
	
	return 0;
}

