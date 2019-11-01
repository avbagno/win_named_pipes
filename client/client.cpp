#include "Client.hpp"

#include <iostream>

static const LPTSTR PIPE_NAME = TEXT("\\\\.\\pipe\\mynamedpipe");
#define BUFSIZE 512

Client::~Client() {
	if (_pipe != INVALID_HANDLE_VALUE) {
		std::cout << "Close handler" << std::endl;
		CloseHandle(_pipe);
	}
} 

bool Client::open(const std::string& pipe_name) {
	LPSTR p_name = const_cast<char*>(pipe_name.c_str());

	while (true)
	{
		_pipe = CreateFile(
			p_name, 
			GENERIC_READ | GENERIC_WRITE | FILE_FLAG_OVERLAPPED,
			0,              // no sharing 
			NULL,           // default security attributes
			OPEN_EXISTING,  // opens existing pipe 
			0,              // default attributes 
			NULL);          // no template file 
 

		if (_pipe != INVALID_HANDLE_VALUE)
			return true;

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

}

bool Client::sent_data(const std::string& data) {
	DWORD dwMode = PIPE_READMODE_MESSAGE;
	BOOL fSuccess = SetNamedPipeHandleState(
		_pipe,    
		&dwMode,  
		NULL,     // don't set maximum bytes 
		NULL);    // don't set maximum time 
	if (!fSuccess)
	{
		_tprintf(TEXT("SetNamedPipeHandleState failed. GLE=%d\n"), GetLastError());
		return false;
	}
	LPTSTR lpvMessage = const_cast<char*>(data.c_str());
	DWORD cbToWrite = (lstrlen(lpvMessage) + 1) * sizeof(TCHAR);
	DWORD cbWritten;
	fSuccess = WriteFile(
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
		// Read from the pipe. 

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

		int c = _getch();
		switch (c) {
		case 'q': run = false;
			break;
		case 's': {
			if (!client.sent_data("Hello world"))
				std::cout << "Failed to sent data" << std::endl;
			client.read_sync();
			break;
		}
		}

	}
	return 0;
}