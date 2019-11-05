#include "Client.hpp"
#include <iostream>
#include <fstream>
#include <type_traits>
#include "../common/logging.hpp"


static const LPTSTR PIPE_NAME = TEXT("\\\\.\\pipe\\mynamedpipe");
#define BUFSIZE 512

Client::~Client() {
	if (_pipe != INVALID_HANDLE_VALUE) {
		CloseHandle(_pipe);
	}

	if (_connectEvent != INVALID_HANDLE_VALUE) {
		CloseHandle(_pipe);
	}
} 

bool Client::open(const std::string& pipe_name) {
	LPSTR p_name = const_cast<char*>(pipe_name.c_str());

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

	while (true)
	{
		_pipe = CreateFile(
			p_name, 
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,             
			NULL,           // default security attributes
			OPEN_EXISTING,  // opens existing pipe 
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
			NULL);          // no template file 
 

		if (_pipe != INVALID_HANDLE_VALUE)
			break;

		if (GetLastError() != ERROR_PIPE_BUSY)
		{
			_tprintf(TEXT("Could not open pipe. GLE=%d\n"), GetLastError());
			return false;
		} 

		if (!WaitNamedPipe(p_name, 20000))
		{
			printf("Could not open pipe: 20 second wait timed out.");
			return false;
		}
	}

	return true;
}

bool Client::send_sync(const std::string& data) {
	LPTSTR lpvMessage = const_cast<char*>(data.c_str());
	DWORD cbToWrite = (lstrlen(lpvMessage) + 1) * sizeof(TCHAR);
	DWORD cbWritten;
	BOOL fSuccess = WriteFile(
		_pipe,                  
		lpvMessage,            
		cbToWrite,              
		&cbWritten,             
		NULL);                 

	if (!fSuccess)
	{
		_tprintf(TEXT("WriteFile to pipe failed. GLE=%d\n"), GetLastError());
		return false;
	}
	return true;
}

bool Client::read_sync() {
	BOOL fSuccess = FALSE;
	TCHAR  chBuf[BUFSIZE];
	DWORD  cbRead;
	do
	{
		fSuccess = ReadFile(
			_pipe,    
			chBuf,   
			BUFSIZE * sizeof(TCHAR),  // size of buffer 
			&cbRead,  // number of bytes read 
			NULL);    // not overlapped 

		if (!fSuccess && GetLastError() != ERROR_MORE_DATA)
			break;

		_tprintf(TEXT("\"%s\"\n"), chBuf);
	} while (!fSuccess);  

	if (!fSuccess)
	{
		_tprintf(TEXT("ReadFile from pipe failed. GLE=%d\n"), GetLastError());
		return false;
	}
	return true;
}

void Client::read_async() {
	_pendingOPList.emplace_back(std::make_unique<PendingIOData>());
	auto& d = _pendingOPList.back();
	d->type = PendingIOType::READ;
	d->ov.hEvent = _connectEvent;
	BOOL bResult = ReadFile(_pipe,
		d->_readBuffer.data(),
		d->_readBuffer.size(),
		&d->numBytes,
		&d->ov);

	if (!bResult) {
		DWORD dwError = GetLastError();
		switch (dwError) {
		case ERROR_IO_PENDING:
		{
			LOG_INFO << "Read operation is pending ";
			break;
		}
		default:
			break;
		}
	}
	else {
		std::string str(d->_readBuffer.begin(), d->_readBuffer.begin() + d->numBytes);
		processServerResponse(str);
		d->complete = true;
	}
}

void Client::processServerResponse(const std::string& res) {
	LOG_INFO << "Server response " << res;

	std::vector<Command> cmds;
	deserialize(res, cmds);
	for (auto& c : cmds) {
		LOG_INFO << cmdTypeToString(c.cmd) << ":" << cmdResToString(c.res);
		if (c.res == ACK_OK) {
			switch (c.cmd) {
			case CREATE_OBJECT: 
				_objectsOnServer[c.objId] = c.objType;
				break;
			case GET_OBJECT: {
				LOG_INFO << "GET_OBJECT";
				auto res = deserializeObject(c);
				if (res)
					LOG_INFO << "Successfully got object from server";
				break;
			}
			case GET_OBJECT_MEMBER:
				LOG_INFO << "GET_OBJECT_MEMBER res " << c.info;
				break;
			default:
				LOG_INFO << "Unknown command";
				break;
			}
		}
		
	}
}

void Client::send_async(const std::string& data) {
	
	LPTSTR lpvMessage = const_cast<char*>(data.c_str());
	DWORD cbToWrite = (lstrlen(lpvMessage) + 1) * sizeof(TCHAR);
	
	_pendingOPList.emplace_back(std::make_unique<PendingIOData>());
	auto& d = _pendingOPList.back();
	d->type = PendingIOType::WRITE;
	d->ov.hEvent = _connectEvent;
	BOOL fSuccess = WriteFile(
		_pipe,
		lpvMessage,
		cbToWrite,
		&d->numBytes,
		&d->ov);

	
	bool pending = true;
	if (!fSuccess)
	{
		DWORD dwError = GetLastError();
		switch (dwError) {
			case ERROR_IO_PENDING:
			{
				LOG_INFO << "Write operation is pending ";
				break;
			}
			default:
				break;
		}
	}
	else {
		d->complete = true;
		LOG_INFO << "Write operation completed";
	}
}

void Client::checkPendingIO(PendingIODataPtr& d) {
	BOOL bResult = GetOverlappedResult(_pipe,
		&d->ov,
		&d->numBytes,
		FALSE);

	DWORD dwError;
	if (!bResult)
	{
		switch (dwError = GetLastError())
		{
		case ERROR_IO_INCOMPLETE:
		{
			LOG_INFO << "Operation is still pending ";
			break;
		}
		default:
			break;
		}
	}
	else {
		LOG_INFO << "Operation completed ";
		if (d->type == PendingIOType::READ) {
			std::string str(d->_readBuffer.begin(), d->_readBuffer.begin() + d->numBytes);
			processServerResponse(str);
		}
		//ResetEvent(d->ov.hEvent);
		d->complete = true;
	}

}

void Client::printObjectsIds() {
	LOG_INFO << "Object ids";
	for (const auto& obj : _objectsOnServer) {
		LOG_INFO << obj.first;
	}
}

void Client::checkPendingOperaions() {
	for (auto& op : _pendingOPList) {
		checkPendingIO(op);
	}

	_pendingOPList.erase(std::remove_if(_pendingOPList.begin(), _pendingOPList.end(), [](const PendingIODataPtr& d) {
		return d->complete;
		}), _pendingOPList.end());
}

CustomObjectsType Client::getObjectInfo(int id) {
	auto it = _objectsOnServer.find(id);
	if (it != _objectsOnServer.end()) {
		return it->second;
	}
	else {
		return UNKNOWN_OBJ;
	}
}

void printMenu() {
	static const std::string menu = "\n"
		"1 quite \n"
		"2 send simple data to server(string, int) \n"
		"3 read (async) message from server \n"
		"4 create object in server \n"
		"5 print available objects ids \n"
		"6 get object from server \n"
		"7 call object member \n"
		"8 check pending operations \n";
	LOG_INFO << menu;
}

int _tmain(int argc, TCHAR* argv[])
{
	Client client;
	if (!client.open("\\\\.\\pipe\\mynamedpipe")) {
		LOG_ERROR << "Couldnt open named pipe";
		return 0;
	}
	
	bool run = true;
	while (run) {
		printMenu();

		int c = _getch();
		switch (c) {
		case '1': run = false;
			break;
		case '2': {
			client.send_async("Hello world");	
			client.send_async_(30.0);
			break;
		}
		case '3': {
			client.read_async();
			break;
		}
		case '4': {
			struct Command cmd = {CREATE_OBJECT, CUSTOM_TYPE_1, 0};
			std::string buffer;
			serializeCommand(cmd, buffer);
			client.send_async(buffer);
			client.read_async();
			break;
		} case '5': {
			client.printObjectsIds();
			break;
		}
		case '6': {
			LOG_INFO << "Enter obj id";
			int id = -1;
			std::cin >> id;
			auto type = client.getObjectInfo(id);
			if (type != UNKNOWN_OBJ) {
				struct Command cmd = { GET_OBJECT, type, id };
				std::string buffer;
				serializeCommand(cmd, buffer);
				client.send_async(buffer);
				client.read_async();
			}
			else {
				LOG_ERROR << "Unknown object id";
			}
			break;
		}
		case '7': {
			LOG_INFO << "Enter obj id";
			int id = -1;
			std::cin >> id;
			auto type = client.getObjectInfo(id);
			if (type != UNKNOWN_OBJ) {
				struct Command cmd = { GET_OBJECT_MEMBER, type, id,  UNKNOWN_RES, "getM1" };
				std::string buffer;
				serializeCommand(cmd, buffer);
				client.send_async(buffer);
				client.read_async();
			}
			else {
				LOG_ERROR << "Unknown object id";
			}
			break;
		}
		case '8': {
			client.checkPendingOperaions();
			break;
		}
		default:
			LOG_ERROR << "Unknown operation";
			break;
		}
	}
	return 0;
}

