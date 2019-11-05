#pragma once

#include <windows.h> 
#include <stdio.h> 
#include <tchar.h>
#include <strsafe.h>

#include <string>
#include <array>
#include <memory> 
#include <vector>
#include <map>
#include <queue>

#include "Objects.hpp"

class Server : public std::enable_shared_from_this<Server>{
public:
	bool init();
	void run();
	void stop();

	~Server();

	static VOID WINAPI completeWriteRoutine(DWORD dwErr, DWORD cbWritten,
		LPOVERLAPPED lpOverLap);
	static VOID WINAPI completeReadRoutine(DWORD dwErr, DWORD cbBytesRead,
		LPOVERLAPPED lpOverLap);

private:
	bool createAndConnectInstance();
	DWORD waitForEvent();
	void workLoop();
	void removePipeInst(int id);
	//void processCommand(const Command& in, Command& res);
	OVERLAPPED _connect;
	HANDLE _pipe = INVALID_HANDLE_VALUE;
	HANDLE _connectEvent = INVALID_HANDLE_VALUE;
	std::atomic<bool> _running = false;

	static constexpr int BUFSIZE = 4096;
	static constexpr int PIPE_TIMEOUT = 5000;
	
	using CustomObjPtr = std::unique_ptr<CustomObject>;

	struct PipeInst {
		OVERLAPPED overlap;
		HANDLE pipeInst;
		DWORD cbRead;
		int id; 
		std::shared_ptr<Server> owner;
	};

	struct ClientData {
		std::map<int, CustomObjPtr> _objectsMap;
		std::queue<std::string> _clientRequests;
		std::queue<std::string> _clientResponses;

		std::array<char, BUFSIZE> clientRequestBuffer;
		std::array<char, BUFSIZE> serverResponseBuffer;
		static int _object_id;
	};

	std::map<int, ClientData> _clientsData;
	void processClientRequest(PipeInst&);
	int createNewObject(ClientData& cdata, CustomObjectsType type);
	void processCommand(ClientData& clientData, const Command cmd);
	struct PipeInstDeleter {
		void operator()(PipeInst* p_inst) const;
	};

	using  t_pipeinst_ptr = std::unique_ptr<PipeInst, PipeInstDeleter>;
	std::vector<t_pipeinst_ptr> _pipeinst_list;


	void completeWrite(PipeInst& pipeInst, DWORD dwErr, DWORD cbWritten);
	void completeRead(PipeInst& pipeInst, DWORD dwErr, DWORD cbBytesRead);

	void writeToClient(
		PipeInst& pipeInst,
		const std::string& message);
	void readFromClient(
		PipeInst& pipeInst);

	static int _connection_id;
};