Please start the server with the following command:
	serv <host> <port>
where the host can be localhost and the port is an integer between 1 & 65535.

Then start the client with the following command:
	ftclient <host> <CCPport> <DCQport> <command> [filename]
where the host can be localhost,
 the communication channel P (CCP) is the port you started the server on,
 data connection Q (DCQ) is another integer between 1 & 65535
 the command is either '-l' for a file list or '-g' for get file
 and the filename is the file to get with '-g'.


For instance:
	serv localhost 3000
	 -- opens the server to listen on localhost for connection P on port 3000.
	python3 ftclient.py localhost 3000 3001 -l
	 -- opens the client to connect to the server on localhost using port 3000,
	    and requesting a data connection on port 3001 to receive a directory listing.
	Subsequently,
	python3 ftclient.py localhost 3002 3003 -g foo.txt
	python3 ftclient.py localhost 3004 3005 -g bar.baz


Extra Credit: The server is multi-threaded. The primary thread handles 
 control connection P, and the secondary thread handles connecting to and 
 transmitting on the data connection Q.

The repeating server requirement was implemented in a bash script requiring the client to increment their port selections. It informs the server admin of the new base port, and generally assumes that clients will be using a port that is plus one for the data port -- thus, it increments by 2 each time, allowing for incrementing of the data port as well.