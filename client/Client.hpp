#pragma once

#include <windows.h> 
#include <stdio.h>
#include <conio.h>
#include <tchar.h>

#include <string>

class Client {
public:
	bool open(const std::string& pipe_name);
	bool sent_data(const std::string& data);
	bool read_sync();
	~Client();
private:
	HANDLE _pipe = INVALID_HANDLE_VALUE;
};