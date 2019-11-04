#include "Client.hpp"
#include "Objects.hpp"
#include <iostream>
#include <fstream>
#include <type_traits>

#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/serialization.hpp>

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
			std::cout << "Read operation is pending " << std::endl;
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
	std::cout << "Server response " << res << std::endl;

	std::vector<Command> cmds;
	deserialize(res, cmds);
	for (auto& c : cmds) {
		std::cout << cmdTypeToString(c.cmd) << std::endl;
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
				std::cout << "Write operation is pending " << std::endl;
				break;
			}
			default:
				break;
		}
	}
	else {
		d->complete = true;
		std::cout << "Write operation completed" << std::endl;
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
			std::cout << "Operation is still pending " << std::endl;
			break;
		}
		default:
			break;
		}
	}
	else {
		std::cout << "Operation completed " << std::endl;
		if (d->type == PendingIOType::READ) {
			std::string str(d->_readBuffer.begin(), d->_readBuffer.begin() + d->numBytes);
			processServerResponse(str);
		}
		//ResetEvent(d->ov.hEvent);
		d->complete = true;
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

int _tmain(int argc, TCHAR* argv[])
{
	Client client;
	if (!client.open("\\\\.\\pipe\\mynamedpipe")) {
		std::cout << "Couldnt open named pipe" << std::endl;
		return 0;
	}
	
	bool run = true;
	while (run) {

		std::cout << "Choose option" << std::endl;
		std::cout << "q - quite" << std::endl;
		std::cout << "s - send message to server " << std::endl;
		std::cout << "r - read message from server " << std::endl;
		std::cout << "c - create object in server " << std::endl;
		std::cout << "p - check pending io " << std::endl;

		int c = _getch();
		switch (c) {
		case 'q': run = false;
			break;
		case 's': {
			client.send_async("Hello world");	
			client.send_async_(30.0);
			break;
		}
		case 'r': {
			client.read_async();
			break;
		}
		case 'c': {
			struct Command cmd = {CREATE_OBJECT, CUSTOM_TYPE_1, 0};
			std::string buffer;
			serializeCommand(cmd, buffer);
			client.send_async(buffer);
			break;
		} case 'p': {
			client.checkPendingOperaions();
			break;
		}
		default:
			std::cout << "Unknown operation" << std::endl;
			break;
		}
	}
	return 0;
}

