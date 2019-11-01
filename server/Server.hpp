#pragma once

#include <windows.h> 
#include <stdio.h> 
#include <tchar.h>
#include <strsafe.h>

#include <string>
#include <array>
#include <memory> 
#include <vector>
#include <atomic>

class Server : public std::enable_shared_from_this<Server> {
public:
	bool init();
	bool createAndConnectInstance();
	DWORD waitForEvent();

	void run();
	void stop();

	void workLoop();

	static VOID WINAPI completeWriteRoutine(DWORD dwErr, DWORD cbWritten,
		LPOVERLAPPED lpOverLap);
	static VOID WINAPI completeReadRoutine(DWORD dwErr, DWORD cbBytesRead,
		LPOVERLAPPED lpOverLap);

private:

	void removePipeInst(int id);
	OVERLAPPED _connect;
	HANDLE _pipe = INVALID_HANDLE_VALUE;
	HANDLE _connectEvent = INVALID_HANDLE_VALUE;
	std::atomic<bool> _running = false;

	static constexpr int BUFSIZE = 4096;
	static constexpr int PIPE_TIMEOUT = 5000;
	
	struct PipeInst {
		OVERLAPPED overlap;
		HANDLE pipeInst;
		std::array<char, BUFSIZE> clientRequestBuffer;
		DWORD cbRead;
		std::array<char, BUFSIZE> serverResponseBuffer;
		DWORD cbToWrite;

		int id; 
		std::shared_ptr<Server> owner;
	};

	struct PipeInstDeleter {
		void operator()(PipeInst* p_inst) const;
	};

	using  t_pipeinst_ptr = std::unique_ptr<PipeInst, PipeInstDeleter>;
	std::vector<t_pipeinst_ptr> _pipeinst_list;


	void completeWrite(PipeInst& pipeInst, DWORD dwErr, DWORD cbWritten);
	void completeRead(PipeInst& pipeInst, DWORD dwErr, DWORD cbBytesRead);

	void writeToClient(DWORD dwErr, DWORD cbWritten,
		PipeInst& pipeInst);
	void readFromClient(DWORD dwErr, DWORD cbBytesRead,
		PipeInst& pipeInst);

	static int _connection_id;
};