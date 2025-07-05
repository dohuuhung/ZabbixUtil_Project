#include "logger.h"
#include <fstream>
#include <string>
#include <stdexcept>
#include <ctime>
using namespace std;

void writeLog(const string& filePath, const string& content) {
	std::time_t now = std::time(nullptr);
	std::tm* local_time = std::localtime(&now);
	char buffer[100];
	std::strftime(buffer, sizeof(buffer), "[%Y-%m-%d %H:%M:%S]", local_time);
	string cur_time = std::string(buffer);
	string msg = cur_time + " " + content;
	
    ofstream outFile(filePath, std::ios::app);
    if (!outFile) {
        throw std::runtime_error("Can not open file to write: " + filePath);
    }
    outFile << msg;
    outFile.close();
}