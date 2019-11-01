#include "Server.hpp"
#include <iostream>

int Server::_connection_id = 0;

void Server::PipeInstDeleter::operator()(PipeInst* p_inst) const {
	if (!DisconnectNamedPipe(p_inst->pipeInst))
	{
		printf("DisconnectNamedPipe failed with %d.\n", GetLastError());
	}

	CloseHandle(p_inst->pipeInst);
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

	std::cout << "Server::createAndConnectInstance " << std::endl;
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

    std::cout << "Start an overlapped connection for this pipe instance." << std::endl;
	fConnected = ConnectNamedPipe(_pipe, &_connect);
	std::cout << "Connected to client" << std::endl;
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
}

void Server::stop() {
	_running = false;
}

void Server::workLoop() {
	bool pendingIO = createAndConnectInstance();
	while (_running) {
		
		std::cout << "Wait for event " << std::endl;
		DWORD dwWait = waitForEvent();
		std::cout << "Wait finished" << std::endl;
		switch (dwWait)
		{
		case WAIT_OBJECT_0:
		{
			if (pendingIO)
			{
				std::cout << "We have pending IO" << std::endl;
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
				std::cout << "Bytes readen " << cbRet << std::endl;
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
			++_connection_id;

			last->cbToWrite = 0;
			std::cout << "Before write/read routine starts" << std::endl;
			readFromClient(0, 0, *last);
			std::cout << "Read done" << std::endl;
			writeToClient(0, 0, *last);

			std::cout << "Create new pipe instance for the next client." << std::endl;
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

void Server::writeToClient(DWORD dwErr, DWORD cbWritten,
	PipeInst& pipeInst) {
	std::cout << "WriteFileEx" << std::endl;

	std::string default_response = "Default server response";
	std::copy(default_response.begin(), default_response.end(), pipeInst.serverResponseBuffer.data());
	pipeInst.cbToWrite = default_response.length();
	BOOL fWrite = WriteFileEx(
		pipeInst.pipeInst,
		pipeInst.serverResponseBuffer.data(),
		pipeInst.cbToWrite,
		&pipeInst.overlap,
		(LPOVERLAPPED_COMPLETION_ROUTINE)&Server::completeWriteRoutine);
	if (!fWrite) {
		removePipeInst(pipeInst.id);
	}
}
void Server::readFromClient(DWORD dwErr, DWORD cbBytesRead,
	PipeInst& pipeInst) {
	BOOL fRead = ReadFileEx(
		pipeInst.pipeInst,
		pipeInst.clientRequestBuffer.data(),
		BUFSIZE,
		&pipeInst.overlap,
		(LPOVERLAPPED_COMPLETION_ROUTINE)&Server::completeReadRoutine);

	if (!fRead) {
		removePipeInst(pipeInst.id);
	}
	else {
		std::cout << "Succseful call read from client " << std::endl;
	}
}

void Server::completeWrite(PipeInst& pipeInst, DWORD dwErr, DWORD cbWritten) {
	if (dwErr != 0) {
		removePipeInst(pipeInst.id);
	}
}

void Server::completeRead(PipeInst& pipeInst, DWORD dwErr, DWORD cbBytesRead) {
	if (dwErr != 0) {
		removePipeInst(pipeInst.id);
	}
}

VOID WINAPI Server::completeReadRoutine(DWORD dwErr, DWORD cbRead,
	LPOVERLAPPED lpOverLap)
{
	std::cout << "Complete read routine, static " << std::endl;
	PipeInst* pipeInst = (PipeInst*)lpOverLap;
	pipeInst->owner->completeRead(*pipeInst, dwErr, cbRead);
}

VOID WINAPI Server::completeWriteRoutine(DWORD dwErr, DWORD cbRead,
	LPOVERLAPPED lpOverLap)
{
	PipeInst* pipeInst = (PipeInst*)lpOverLap;
	pipeInst->owner->completeWrite(*pipeInst, dwErr, cbRead);
}

#define PIPE_TIMEOUT 5000
#define BUFSIZE 4096

typedef struct
{
	OVERLAPPED oOverlap;
	HANDLE hPipeInst;
	TCHAR chRequest[BUFSIZE];
	DWORD cbRead;
	TCHAR chReply[BUFSIZE];
	DWORD cbToWrite;
} PIPEINST, * LPPIPEINST;

VOID DisconnectAndClose(LPPIPEINST);
BOOL CreateAndConnectInstance(LPOVERLAPPED);
BOOL ConnectToNewClient(HANDLE, LPOVERLAPPED);
VOID GetAnswerToRequest(LPPIPEINST);

VOID WINAPI CompletedWriteRoutine(DWORD, DWORD, LPOVERLAPPED);
VOID WINAPI CompletedReadRoutine(DWORD, DWORD, LPOVERLAPPED);

HANDLE hPipe;

void test_func() {
	std::shared_ptr<Server> server = std::make_shared<Server>();
	server->init();
	server->run();
	server->workLoop();
	
}

int _tmain(VOID)
{
	//test_func();
	//return 0;

	HANDLE hConnectEvent;
	OVERLAPPED oConnect;
	LPPIPEINST lpPipeInst;
	DWORD dwWait, cbRet;
	BOOL fSuccess, fPendingIO;

	// Create one event object for the connect operation. 

	hConnectEvent = CreateEvent(
		NULL,    // default security attribute
		TRUE,    // manual reset event 
		TRUE,    // initial state = signaled 
		NULL);   // unnamed event object 

	if (hConnectEvent == NULL)
	{
		printf("CreateEvent failed with %d.\n", GetLastError());
		return 0;
	}

	oConnect.hEvent = hConnectEvent;

	// Call a subroutine to create one instance, and wait for 
	// the client to connect. 

	fPendingIO = CreateAndConnectInstance(&oConnect);

	while (1)
	{
		// Wait for a client to connect, or for a read or write 
		// operation to be completed, which causes a completion 
		// routine to be queued for execution. 

		std::cout << "WaitForSingleObjectEx" << std::endl;
		dwWait = WaitForSingleObjectEx(
			hConnectEvent,  // event object to wait for 
			INFINITE,       // waits indefinitely 
			TRUE);          // alertable wait enabled 

		
		switch (dwWait)
		{
			// The wait conditions are satisfied by a completed connect 
			// operation. 
		case WAIT_OBJECT_0:
			// If an operation is pending, get the result of the 
			// connect operation. 

			if (fPendingIO)
			{
				fSuccess = GetOverlappedResult(
					hPipe,     // pipe handle 
					&oConnect, // OVERLAPPED structure 
					&cbRet,    // bytes transferred 
					FALSE);    // does not wait 
				if (!fSuccess)
				{
					printf("ConnectNamedPipe (%d)\n", GetLastError());
					return 0;
				}

				// Allocate storage for this instance. 

				lpPipeInst = (LPPIPEINST)GlobalAlloc(
					GPTR, sizeof(PIPEINST));
				if (lpPipeInst == NULL)
				{
					printf("GlobalAlloc failed (%d)\n", GetLastError());
					return 0;
				}

				lpPipeInst->hPipeInst = hPipe;

				// Start the read operation for this client. 
				// Note that this same routine is later used as a 
				// completion routine after a write operation. 

				lpPipeInst->cbToWrite = 0;
				std::cout << "Before write/read routine starts" << std::endl;
				CompletedWriteRoutine(0, 0, (LPOVERLAPPED)lpPipeInst);

				// Create new pipe instance for the next client. 

				std::cout << "Create new pipe instance for the next client." << std::endl;
				fPendingIO = CreateAndConnectInstance(
					&oConnect);
			}

			break;

			// The wait is satisfied by a completed read or write 
			// operation. This allows the system to execute the 
			// completion routine. 

		case WAIT_IO_COMPLETION:
			std::cout << "WAIT_IO_COMPLETION" << std::endl;
			break;

			// An error occurred in the wait function. 

		default:
		{
			printf("WaitForSingleObjectEx (%d)\n", GetLastError());
			return 0;
		}
		}
	}
	return 0;
}

// CompletedWriteRoutine(DWORD, DWORD, LPOVERLAPPED) 
// This routine is called as a completion routine after writing to 
// the pipe, or when a new client has connected to a pipe instance.
// It starts another read operation. 

VOID WINAPI CompletedWriteRoutine(DWORD dwErr, DWORD cbWritten,
	LPOVERLAPPED lpOverLap)
{
	LPPIPEINST lpPipeInst;
	BOOL fRead = FALSE;

	// lpOverlap points to storage for this instance. 

	lpPipeInst = (LPPIPEINST)lpOverLap;

	// The write operation has finished, so read the next request (if 
	// there is no error). 

	std::cout << "ReadFileEx" << std::endl;
	if ((dwErr == 0) && (cbWritten == lpPipeInst->cbToWrite)) {
		std::cout << "Read from client" << std::endl;
		fRead = ReadFileEx(
			lpPipeInst->hPipeInst,
			lpPipeInst->chRequest,
			BUFSIZE * sizeof(TCHAR),
			(LPOVERLAPPED)lpPipeInst,
			(LPOVERLAPPED_COMPLETION_ROUTINE)CompletedReadRoutine);
	}

	// Disconnect if an error occurred. 

	if (!fRead)
		DisconnectAndClose(lpPipeInst);
}

// CompletedReadRoutine(DWORD, DWORD, LPOVERLAPPED) 
// This routine is called as an I/O completion routine after reading 
// a request from the client. It gets data and writes it to the pipe. 

VOID WINAPI CompletedReadRoutine(DWORD dwErr, DWORD cbBytesRead,
	LPOVERLAPPED lpOverLap)
{
	LPPIPEINST lpPipeInst;
	BOOL fWrite = FALSE;

	// lpOverlap points to storage for this instance. 

	lpPipeInst = (LPPIPEINST)lpOverLap;

	// The read operation has finished, so write a response (if no 
	// error occurred). 

	if ((dwErr == 0) && (cbBytesRead != 0))
	{
		GetAnswerToRequest(lpPipeInst);

		std::cout << "WriteFileEx" << std::endl;
		fWrite = WriteFileEx(
			lpPipeInst->hPipeInst,
			lpPipeInst->chReply,
			lpPipeInst->cbToWrite,
			(LPOVERLAPPED)lpPipeInst,
			(LPOVERLAPPED_COMPLETION_ROUTINE)CompletedWriteRoutine);
	}

	// Disconnect if an error occurred. 

	if (!fWrite)
		DisconnectAndClose(lpPipeInst);
}

// DisconnectAndClose(LPPIPEINST) 
// This routine is called when an error occurs or the client closes 
// its handle to the pipe. 

VOID DisconnectAndClose(LPPIPEINST lpPipeInst)
{
	std::cout << "Disconnect the pipe instance " << std::endl;

	if (!DisconnectNamedPipe(lpPipeInst->hPipeInst))
	{
		printf("DisconnectNamedPipe failed with %d.\n", GetLastError());
	}

	// Close the handle to the pipe instance. 

	CloseHandle(lpPipeInst->hPipeInst);

	// Release the storage for the pipe instance. 

	if (lpPipeInst != NULL)
		GlobalFree(lpPipeInst);
}

// CreateAndConnectInstance(LPOVERLAPPED) 
// This function creates a pipe instance and connects to the client. 
// It returns TRUE if the connect operation is pending, and FALSE if 
// the connection has been completed. 

BOOL CreateAndConnectInstance(LPOVERLAPPED lpoOverlap)
{
	LPTSTR lpszPipename = TEXT("\\\\.\\pipe\\mynamedpipe");

	hPipe = CreateNamedPipe(
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
	if (hPipe == INVALID_HANDLE_VALUE)
	{
		printf("CreateNamedPipe failed with %d.\n", GetLastError());
		return 0;
	}

	// Call a subroutine to connect to the new client. 

	return ConnectToNewClient(hPipe, lpoOverlap);
}

BOOL ConnectToNewClient(HANDLE hPipe, LPOVERLAPPED lpo)
{
	BOOL fConnected, fPendingIO = FALSE;

	// Start an overlapped connection for this pipe instance. 
	fConnected = ConnectNamedPipe(hPipe, lpo);

	// Overlapped ConnectNamedPipe should return zero. 
	if (fConnected)
	{
		printf("ConnectNamedPipe failed with %d.\n", GetLastError());
		return 0;
	}

	switch (GetLastError())
	{
		// The overlapped connection in progress. 
	case ERROR_IO_PENDING:
		fPendingIO = TRUE;
		break;

		// Client is already connected, so signal an event. 

	case ERROR_PIPE_CONNECTED:
		if (SetEvent(lpo->hEvent))
			break;

		// If an error occurs during the connect operation... 
	default:
	{
		printf("ConnectNamedPipe failed with %d.\n", GetLastError());
		return 0;
	}
	}
	return fPendingIO;
}

VOID GetAnswerToRequest(LPPIPEINST pipe)
{
	_tprintf(TEXT("[%d] %s\n"), pipe->hPipeInst, pipe->chRequest);
	StringCchCopy(pipe->chReply, BUFSIZE, TEXT("Default answer from server"));
	pipe->cbToWrite = (lstrlen(pipe->chReply) + 1) * sizeof(TCHAR);
}
