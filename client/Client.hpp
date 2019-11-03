#pragma once

#include <windows.h> 
#include <stdio.h>
#include <conio.h>
#include <tchar.h>

#include <string>
#include <array>
#include <vector>
#include <type_traits>

class Client {
public:
	bool open(const std::string& pipe_name);
	bool send_sync(const std::string& data);
	bool read_sync();

	~Client();

	void send_async(const std::string& data);
	void read_async();

	void checkPendingOperaions();

	template <class T>
	void send_async_(T data) {
		send_async(std::to_string(data));
	}

private:
	enum class PendingIOType {
		READ,
		WRITE
	};
	static const int BUFFER_SIZE = 4096;
	struct PendingIOData {
		OVERLAPPED ov = {0};
		PendingIOType type;
		DWORD numBytes{0};
		bool complete{ false };
		std::array<char, BUFFER_SIZE> _readBuffer;
	};
	using PendingIODataPtr = std::unique_ptr<PendingIOData>;
	std::vector<PendingIODataPtr> _pendingOPList; 
	void checkPendingIO(PendingIODataPtr& data);
	HANDLE _pipe = INVALID_HANDLE_VALUE;
	HANDLE _connectEvent = INVALID_HANDLE_VALUE;
};