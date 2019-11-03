#include "Server.hpp"
#include <cstdlib>
#include <signal.h>

#include "Objects.hpp"

int Server::_connection_id = 0;
int Server::PipeInst::_object_id = 0;

void Server::PipeInstDeleter::operator()(PipeInst* p_inst) const {
	if (!DisconnectNamedPipe(p_inst->pipeInst))
	{
		printf("DisconnectNamedPipe failed with %d.\n", GetLastError());
	}

	CloseHandle(p_inst->pipeInst);
	p_inst->owner.reset();
	if (p_inst != NULL)
		GlobalFree(p_inst);
}

bool Server::init() {
	_connectEvent = CreateEvent(
		NULL,    // default security attribute
		TRUE,    // manual reset event 
		TRUE,    // initial state = signaled 
		NULL);   // unnamed event object 

	if (_connectEvent == NULL)
	{
		printf("CreateEvent failed with %d.\n", GetLastError());
		return false;
	}
	_connect.hEvent = _connectEvent;

	return true;
}

bool Server::createAndConnectInstance() {

	LPTSTR lpszPipename = TEXT("\\\\.\\pipe\\mynamedpipe");

	_pipe = CreateNamedPipe(
		lpszPipename,             // pipe name 
		PIPE_ACCESS_DUPLEX |      // read/write access 
		FILE_FLAG_OVERLAPPED,     // overlapped mode 
		PIPE_TYPE_MESSAGE |       // message-type pipe 
		PIPE_READMODE_MESSAGE |   // message read mode 
		PIPE_WAIT,                // blocking mode 
		PIPE_UNLIMITED_INSTANCES, // unlimited instances 
		BUFSIZE * sizeof(TCHAR),    // output buffer size 
		BUFSIZE * sizeof(TCHAR),    // input buffer size 
		PIPE_TIMEOUT,             // client time-out 
		NULL);                    // default security attributes
	if (_pipe == INVALID_HANDLE_VALUE)
	{
		printf("CreateNamedPipe failed with %d.\n", GetLastError());
		return 0;
	}


	bool pendingIO = false;
	BOOL fConnected = FALSE;

	fConnected = ConnectNamedPipe(_pipe, &_connect);
	if (fConnected)
	{
		printf("ConnectNamedPipe failed with %d.\n", GetLastError());
		return false;
	}

	switch (GetLastError())
	{
		// The overlapped connection in progress. 
		case ERROR_IO_PENDING:
			pendingIO = true;
			break;
		// Client is already connected, so signal an event. 
		case ERROR_PIPE_CONNECTED:
			if (SetEvent(_connect.hEvent))
				break;
			// If an error occurs during the connect operation... 
		default:
		{
			printf("ConnectNamedPipe failed with %d.\n", GetLastError());
			return 0;
		}
	}
	return pendingIO;
}

DWORD Server::waitForEvent() {
	// Wait for a client to connect, or for a read or write 
	// operation to be completed, which causes a completion 
	// routine to be queued for execution. 

	DWORD dwWait = WaitForSingleObjectEx(
		_connectEvent,  // event object to wait for 
		INFINITE,       // waits indefinitely 
		TRUE);          // alertable wait enabled 
	return dwWait;
}

void Server::run() {
	_running = true;
	workLoop();
}

void Server::stop() {
	_running = false;
}

void Server::workLoop() {
	bool pendingIO = createAndConnectInstance();
	while (_running) {
		
		//try get something from client
		for (auto& p : _pipeinst_list) {
			readFromClient(*p);
		}

		//check client request
		for (auto& p : _pipeinst_list) {
			processClientRequest(*p);
		}

		//send responses 
		for (auto& p : _clientsData) {
			while(!p.second._clientResponses.empty()) {
				Command cmd = p.second._clientResponses.front();
				std::string out;
				serializeCommand(cmd, out);
				//writeToClient(*p, out);
				p.second._clientResponses.pop();
			}
		}

		std::cout << "Wait for event " << std::endl;
		DWORD dwWait = waitForEvent();
		switch (dwWait)
		{
		case WAIT_OBJECT_0:
		{
			if (pendingIO)
			{
				DWORD cbRet = 0;
				BOOL fSuccess = GetOverlappedResult(
					_pipe,     // pipe handle 
					&_connect, // OVERLAPPED structure 
					&cbRet,    // bytes transferred 
					FALSE);    // does not wait 
				if (!fSuccess)
				{
					printf("ConnectNamedPipe (%d)\n", GetLastError());
					return;
				}
			}

			PipeInst* v = (PipeInst*)GlobalAlloc(GPTR, sizeof(PipeInst));
			if (v == NULL) {
				std::cout << "Failed to allocate memory";
				return;
			}
			_pipeinst_list.emplace_back(t_pipeinst_ptr(v));

			auto& last = _pipeinst_list.back();
			last->pipeInst = _pipe;
			last->id = _connection_id;
			last->owner = shared_from_this();
			_clientsData[last->id] = ClientData();
			++_connection_id;

			readFromClient(*last);

			pendingIO = createAndConnectInstance();
		}
			break;
		case WAIT_IO_COMPLETION:
			break;
		default:
		{
			printf("WaitForSingleObjectEx (%d)\n", GetLastError());
			return;
		}
		}
	}
}

void Server::removePipeInst(int id) {
	std::cout << "Remove pipe inst with id " << id << std::endl;
	_pipeinst_list.erase(std::remove_if(_pipeinst_list.begin(), _pipeinst_list.end(), [id](const auto& inst) {
		return inst->id == id;
		}),
		_pipeinst_list.end());
}

void Server::writeToClient(
	PipeInst& pipeInst, 
	const std::string& message) {
	auto it = _clientsData.find(pipeInst.id);
	if (it == _clientsData.end())
		return;
	std::fill(std::begin(it->second.serverResponseBuffer),
		std::end(it->second.serverResponseBuffer), 0);
    std::copy(message.begin(), message.end(), it->second.serverResponseBuffer.data());
	BOOL fWrite = WriteFileEx(
		pipeInst.pipeInst,
		it->second.serverResponseBuffer.data(),
		message.length(),
		&pipeInst.overlap,
		(LPOVERLAPPED_COMPLETION_ROUTINE)&Server::completeWriteRoutine);
	if (!fWrite) {
		removePipeInst(pipeInst.id);
	}
}
void Server::readFromClient(
	PipeInst& pipeInst) {
	auto it = _clientsData.find(pipeInst.id);
	if (it == _clientsData.end())
		return;
	std::fill(std::begin(it->second.clientRequestBuffer),
		std::end(it->second.clientRequestBuffer), 0);
	BOOL fRead = ReadFileEx(
		pipeInst.pipeInst,
		it->second.clientRequestBuffer.data(),
		BUFSIZE,
		&pipeInst.overlap,
		(LPOVERLAPPED_COMPLETION_ROUTINE)&Server::completeReadRoutine);

	if (!fRead) {
		removePipeInst(pipeInst.id);
	}
}

void Server::completeWrite(PipeInst& pipeInst, DWORD dwErr, DWORD cbWritten) {
	if (dwErr != 0) {
		removePipeInst(pipeInst.id);
	}
}

void Server::completeRead(PipeInst& pipeInst, DWORD dwErr, DWORD cbBytesRead) {
	//if (dwErr != 0) {
	//	std::cout << "Remove pipeInst" << std::endl;
	//	removePipeInst(pipeInst.id);
	//	return;
	//}
	auto it = _clientsData.find(pipeInst.id);
	if (it != _clientsData.end()) {
		std::string str(it->second.clientRequestBuffer.begin(),
			it->second.clientRequestBuffer.begin() + cbBytesRead);
		it->second._clientRequests.push(str);
	}
}

int Server::createNewObject(PipeInst& pipeInst, CustomObjectsType type) {
	CustomObjPtr obj{nullptr};
	switch (type) {
	case CUSTOM_TYPE_1:
		obj.reset(new CustomObject1());
		break;
	case CUSTOM_TYPE_2:
		obj.reset(new CustomObject2());
	default:
		std::cout << "Unknown object type" << std::endl;
		break;
	}
	auto it = _clientsData.find(pipeInst.id);
	if (obj != nullptr && it != _clientsData.end()) {
		it->second._objectsMap[pipeInst._object_id++] = std::move(obj);
		return pipeInst._object_id - 1;
	}
	return -1;
}

void Server::processClientRequest(PipeInst& p) {
	auto it = _clientsData.find(p.id);
	if (it == _clientsData.end()) {
		std::cout << "Unknown id" << std::endl;
		return;
	}
	while (!it->second._clientRequests.empty()) {
		std::string req = it->second._clientRequests.front();
		it->second._clientRequests.pop();
		std::cout << "Client request " << req << std::endl;
		try {
			Command cmd = deserializeCommand(req);
			Command response;
			switch (cmd.cmd) {
			case CREATE_OBJECT: {
				int id = createNewObject(p, cmd.objType);
				if (id >= 0) {
					std::cout << "New object created, id " << id << std::endl;
					response.cmd = ACK_OK;
					response.objId = id;
					response.objType = cmd.objType;
				}
				else {
					response.cmd = ACK_FAIL;
					std::cout << "Couldn't create new object" << std::endl;
				}
				it->second._clientResponses.push(response);
				break;
			}
			case GET_OBJECT: {
				auto itObj = it->second._objectsMap.find(cmd.objId);
				if (itObj != it->second._objectsMap.end()) {
					std::string out;
					response.objId = cmd.objId;
					
					if (serializeObject(itObj->second.get(), out)) {
						response.cmd = ACK_OK;
						response.info = out;
					}
					else {
						response.cmd = ACK_FAIL;
					}
				}
				it->second._clientResponses.push(response);
				break;
			}
			case GET_OBJECT_MEMBER: {
				auto itObj = it->second._objectsMap.find(cmd.objId);
				if (itObj != it->second._objectsMap.end()) {
					std::string out;
					if (itObj->second->exec(cmd.info, out)) {
						response.cmd = ACK_OK;
						response.info = out;
					}
					else {
						response.cmd = ACK_FAIL;
					}
				}
				it->second._clientResponses.push(response);
				break;
			}
			default:
				std::cout << "Unknown command" << std::endl;
				break;
			}
		}
		catch (const std::exception & e) {
			std::cout << "Exeption " << e.what() << std::endl;
		}
	}
}

VOID WINAPI Server::completeReadRoutine(DWORD dwErr, DWORD cbRead, LPOVERLAPPED lpOverLap)  { 
	PipeInst * pipeInst = (PipeInst*)lpOverLap;
	pipeInst->owner->completeRead(*pipeInst, dwErr, cbRead);
}

VOID WINAPI Server::completeWriteRoutine(DWORD dwErr, DWORD cbRead, LPOVERLAPPED lpOverLap)
{
	PipeInst * pipeInst = (PipeInst*)lpOverLap;
	pipeInst->owner->completeWrite(*pipeInst, dwErr, cbRead);
}

std::shared_ptr<Server> server = std::make_shared<Server>();

//void signal_callback_handler(int signum) {
//	std::cout << "Caught signal " << signum << std::endl;
//	
//	server->stop();
//	exit(signum);
//}

int _tmain(VOID)
{
	//signal(SIGINT, signal_callback_handler);
	
	server->init();
	server->run();
	return 0;

}