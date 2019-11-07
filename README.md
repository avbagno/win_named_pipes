# win_named_pipes

This is the demo for win client-server application running by named pipes.
The client and server are single-threated. The server process read and write operations only asynchronously. 
The client can make both synchronous and asynchronous calls to the server. The client and server communicate by simple custom protocol.
The idea is sending serialized commands and objects.
Example of commands - CREATE_OBJECT, GET_OBJECT_ELEMENT.
Object serialization is realized with boost serialization lib.
As an example added two custom objects. 

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
