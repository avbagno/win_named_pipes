# win_named_pipes

This is the demo for win client-server application running by named pipes. The client and server are single-threated. 
The server process read and write operations only asynchronously. The client can make both synchronous and asynchronous calls to the server. 
Asynchronous functionality is implemented via named pipes overlapped operations.<br/>
The client and server communicate by simple custom protocol. 
The idea is sending serialized commands and objects. Object serialization is realized with boost serialization lib. Example of commands - CREATE_OBJECT, GET_OBJECT_ELEMENT.
For each command obtained from client, server sends a response command with information about command execution:  successfully(ACK_OK), fails(ACK_FAIL). 
The server doesn't send a response to non-commands data, like string or int. As an example added two custom objects.<br/>
Server stores client data for each connection and objects created on the server according to client requests. Each object has unique id, the client gets that id in response with ACK_OK command.
Id can be used to retrieve objects, object members from server or execute member function on the server.

## Dependency
 - boost serialization
 - boost logging

## Usage

1. Run server.
2. Run client.
3. Choose available client commands, which is
	- 1 quite
	- 2 send simple data to server(string, int)
    - 3 read (async) message from server
	- 4 create object on server 
	- 5 print available objects ids
	- 6 get object from server 
	- 7 call object member 
	- 8 check pending operations
