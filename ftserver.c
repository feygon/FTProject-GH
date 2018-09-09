/*
*	Author: Feygon
*	Desc:	A file transfer server using the sockets API
*	Usage:	See README.md for instructions, or usage dialog for syntax
*/
//#define INSTRUMENT

#define LINUX
#ifdef LINUX
	#include <stdio.h>
	#include <sys/unistd.h>
	#include <sys/socket.h>
	#include <sys/wait.h>
	#include <netinet/in.h>
	#include <netdb.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <ctype.h>
#include <string.h>
#include <dirent.h>

/*
*	Requirements:
*		Bind and listen on user-defined port for Host A, TCP control connection P
*		Wait on P for client to send a command
*		Receive command on P
*			If command invalid, send error message on P
*		If valid, initiate TCP data connection Q w/ client on client-supplied port
*		Close connection Q
*/

struct connInfo {
	struct addrinfo* sockinfo;
	int sockFD;
};

struct cmdFile {
	char* cmdStr;
	char* fileStr;
};

int main(int argc, char** argv);
void usage(int argc, char** argv);
	// open a port to listen for client on control connection P


void openCCP(int argc, char** argv, struct connInfo* CCP);
struct connInfo* reopenCCP(int argc, char** argv, struct connInfo* CCP);
struct connInfo* getSocket(int argc, char** argv, char* port);
void freeConnInfo(struct connInfo* CCX);	// free connection data structures

// main operation of control connection P
void runCCP(int argc, char** argv, struct connInfo* CCP); 
int _validate(int argc, char** argv, char* port, struct cmdFile* CCQcmd);
void _fileNotFound(int signo);
void _operationComplete(int signo);

// main operation of control connection Q
int runCCQ(int argc, char** argv, char* port, struct cmdFile* CCQcmd);
int _forkCCQ(struct connInfo* CCQ, struct cmdFile* CCQcmd); // open CCQ child
void _openDataQ(int parent, struct connInfo* CCQ, struct cmdFile* CCQcmd); // return ???
void _sendDataQ(struct connInfo* CCQ, struct cmdFile* CCQcmd); // return? ???
void _fileXfer(int FD, FILE* target);
void _closeCCQ(int origPid);		// just to let the original know
void sendListing(int CCQ);
int CCPFD;

// GP Error function. -- I've used it for a while. I don't remember where I got it.
void error(const char *msg) { perror(msg); exit(1); }

int main(int argc, char** argv) {

	/*
	*	usage logic -- argc == ftserver host CCPport == 3 arguments
	*/

	/*
	*	openCCP
	*	 getSocket
	*	loop
	*	 runCCP
	*		validateCCP
	*		forkCCQ
	*			getSocket
	*			openDataQ
	*			sendDataQ
	*			closeCCQ
	*	 reopenCCP
	*/
	usage(argc, argv);

	struct connInfo* CCP = getSocket(argc, argv, 0);	// get a socket
	openCCP(argc, argv, CCP);	// bind to and listen on that socket.

	while(1) {
		runCCP(argc, argv, CCP);						// accept on socket, do the work.
		CCP = reopenCCP(argc, argv, CCP);		//reopen, re-bind, re-listen
	}

	return 0;
} // end main

void usage(int argc, char** argv) {
#ifdef INSTRUMENT
	printf("argv[2] is %s\n", argv[2]);

#endif
	if (argc != 3) {
		printf("Usage: ftserver <hostname> <port>\n");
		exit(1);
	}

	int num = atoi(argv[2]);
	if ((num <= 0 ) || (num > 65535)) { 
		printf("Usage: port must be an integer between 1 & 65535\n");
		exit(1);
	}
}

/*
*	generate socket info -- but don't bind or connect to it yet.
*/
struct connInfo* getSocket(int argc, char** argv, char* port) {
#ifdef INSTRUMENT
	printf("port is %s(ignore on CCP)\n", port);

#endif
	int status;
	struct addrinfo hints;
	struct connInfo* CCX = calloc(1, sizeof(struct connInfo));
	CCX->sockinfo = calloc(1,sizeof(struct addrinfo));

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	if (port == 0) {	// default port = 0
		status = getaddrinfo(argv[1], argv[2], &hints, &(CCX->sockinfo));
	} else {			// overload for DCQ -- get port from client on CCP
		status = getaddrinfo(argv[1], port, &hints, &(CCX->sockinfo));
	}
	CCX->sockFD = socket(CCX->sockinfo->ai_family,
						 CCX->sockinfo->ai_socktype,
						 CCX->sockinfo->ai_protocol);

#ifdef INSTRUMENT
	printf("");
	/**/
#endif
	if (status != 0) {
		printf("getSocket status = %d\n", status);
		error("getaddrinfo error \n" + status);
		exit(1);
	}
	return CCX;
} // end getSocket

/*
*	bind a socket and listen on it for a connection
*/
void openCCP(int argc, char** argv, struct connInfo* CCP) {


	int status = bind(CCP->sockFD, 
					  CCP->sockinfo->ai_addr, 
					  CCP->sockinfo->ai_addrlen);

	if (status != 0) {
		printf("Bind status = %d\n", status);
		error("getaddrinfo error \n" + status);
		exit(1);
	}

	status = listen(CCP->sockFD, 1);
		if (status != 0) {
		printf("Listen status = %d\n", status);
		error("getaddrinfo error \n" + status);
		exit(1);
	}
}

// after a message has been sent and the stream is terminated,
//  close, re-bind, and setup to listen again.

struct connInfo* reopenCCP(int argc, char** argv, struct connInfo* CCP) {

	freeConnInfo(CCP);
	CCP = getSocket(argc, argv, 0);
	openCCP(argc, argv, CCP);
	return CCP;
} // end reopenCCP

// bind, listen, and accept connection.
// thanks to Beej's Guide for basis of bind and accept method code
void runCCP(int argc, char** argv, struct connInfo* CCP) {
	int child = -5;
	struct sockaddr_storage their_addr;
	socklen_t addr_size = sizeof(struct sockaddr_storage);
		// will block for new connection?
	CCPFD = accept(CCP->sockFD, (struct sockaddr*)&their_addr, &addr_size);

	#ifdef INSTRUMENT
		printf("newFD is %d\n", CCPFD);
		/**/
	#endif

	/*get the client's message on P*/
	char* buf = calloc(255, sizeof(char));
	int bytes_received = recv(CCPFD, buf, 254, 0);
	printf("Received command: %s\n", buf);

	/* 
	*	Get the port number, command, and filename from the client's message
	*/
	char *port = 0, *context;
	struct cmdFile* CCQcmd = calloc(1, sizeof(struct cmdFile));
	CCQcmd->cmdStr = strtok_r(buf, " ", &context);
	if (strcmp(CCQcmd->cmdStr, "-g") == 0) {
		port = strtok_r(NULL, " ", &context);
		CCQcmd->fileStr = context;
	}
	if (strcmp(CCQcmd->cmdStr, "-l") == 0) { 
		port = context;
		CCQcmd->fileStr = NULL;
	}

	int valid = _validate(argc, argv, port, CCQcmd);
	if (!valid) { // note*
		 exit(1); 
	}
	/************************************************************************
	*	note* -- the program should never exit here, if receive-blocking	*
	*	 and remote socket-closing works the way I think it does.			*
	*	 It would exit(0) inside validate after the client closes P.		*
	************************************************************************/
	
	// set signal handler for file not found error.
	struct sigaction SIGUSR1_action = {0};
	SIGUSR1_action.sa_handler = _fileNotFound;
	sigaddset(&SIGUSR1_action.sa_mask, SIGUSR1);
	SIGUSR1_action.sa_flags = 0;
	sigaction(SIGUSR1, &SIGUSR1_action, NULL);
	sleep(0.1);

	// set signal handler for file io complete.
	struct sigaction SIGUSR2_action = {0};
	SIGUSR2_action.sa_handler = _operationComplete;
	sigaddset(&SIGUSR2_action.sa_mask, SIGUSR2);
	SIGUSR2_action.sa_flags = 0;
	sigaction(SIGUSR2, &SIGUSR2_action, NULL);
	sleep(0.1);

#ifdef INSTRUMENT
		printf("(INSTR) Valid command. Running CCQuery.\n");
		int y = 1; y++;
#endif

	child = runCCQ(argc, argv, port, CCQcmd);
//	pause();	// wait until SIGUSR1 or SIGUSR2
	int* status;
	waitpid(child, status, 0);
	exit(0);
} // end runCCP

int _validate(int argc, char** argv, char* port, struct cmdFile* CCQcmd) {  
#ifdef INSTRUMENT
	printf("command is %s, port is %s, filename is %s\n", 
		CCQcmd->cmdStr, port, CCQcmd->fileStr);
#endif
	int valid1 = (strcmp(CCQcmd->cmdStr, "-l") == 0), 
		valid2 = (strcmp(CCQcmd->cmdStr, "-g") == 0);

	if (!valid1 && !valid2) { // if cmd is neither of these commands...
		runCCQ(argc, argv, port, NULL);
		char msg[255] = "Server> Invalid command.\n";
		send(CCPFD, msg, strlen(msg), 0);
		recv(CCPFD, msg, 254, 0);	// just block until P is closed.
		return 0;
	}
	return 1;
}

void _fileNotFound(int signo) {
	char msg[255] = "Server> File not found.\n";
	send(CCPFD, msg, strlen(msg), 0);
//	recv(CCPFD, msg, 254, 0);	// just block until P is closed.
	sleep(1);
	raise(SIGINT);
	exit(0);
}

void _operationComplete(int signo) {
	char msg[255] = "Server> Directory operation complete.\n";
	send(CCPFD, msg, strlen(msg), 0);
//	recv(CCPFD, msg, 254, 0);	// just block until P is closed.
	sleep(1);
	raise(SIGINT);
	exit(0);
}


/*
*	 runCCP
*		validateCCP
*		runCCQ
*			getSocket
*		  	forkCCQ
*		    	openDataQ
*				sendDataQ
*				closeCCQ
*/

/*
*	get a socket for the data connection Q
*	fork a process for the DCQ
*/
int runCCQ(int argc, char** argv, char* port, struct cmdFile* CCQcmd) {
	struct connInfo* CCQ = getSocket(argc, argv, port);

	if (CCQcmd == NULL) {
		connect(CCQ->sockFD, CCQ->sockinfo->ai_addr, (int)(CCQ->sockinfo->ai_addrlen));
		close(CCQ->sockFD);
		return 0;
	} else {
		int child = _forkCCQ(CCQ, CCQcmd);
		return child;
	}
}

/*
*	fork a process for the DCQ
*	Connect to the DCQ on client-defined port.
*/
int _forkCCQ(struct connInfo* CCQ, struct cmdFile* CCQcmd) {
	int parent = getpid();
	int childpid = fork();

	if (childpid != 0) {
#ifdef INSTRUMENT 
		char msg1[20] = "This is parent.\n";
		write(1,msg1,16);
#endif
		return childpid;
	} else {
#ifdef INSTRUMENT
		char msg2[20] = "This is child.\n";
		write(1,msg2,15);
#endif
		_openDataQ(parent, CCQ, CCQcmd);

		/* Do I need to stall here? */
#ifdef INSTRUMENT
	printf("Stall needed.\n");

#endif
		return 0;
	}
}

void _openDataQ(int parent, struct connInfo* CCQ, struct cmdFile* CCQcmd) {
	int status;
#ifdef INSTRUMENT
	printf("(INSTR) Running _openDataQ:\nConnection info:\nCCQ->sockFD: %d\nCCQ->sockinfo->ai_flags: %d\n------------\n",
			CCQ->sockFD, CCQ->sockinfo->ai_flags);
#endif
	status = connect(CCQ->sockFD, CCQ->sockinfo->ai_addr, (int)(CCQ->sockinfo->ai_addrlen));

	if (status < 0) {
		printf("(ERR) CCQ Connection error %d\n", status); // will this work on a forked process?
		fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
		/*send sigint message to parent pid*/
		//raise(SIGINT);
		exit(1);
	}
	#ifdef INSTRUMENT
		char msg[50] = "Connected. Direnting.\n";
		write(1, msg, 22);
	#endif
	_sendDataQ(CCQ, CCQcmd);
}

/*
*	where the dirent magic happens.
*/
void _sendDataQ(struct connInfo* CCQ, struct cmdFile* CCQcmd) {
	DIR *servDir = opendir(".");
	struct dirent *entry = readdir(servDir);
	FILE* target;
	char* msg = calloc(1,255);
#ifdef INSTRUMENT
	printf("CCQcmd->cmdStr is %s\n", CCQcmd->cmdStr);

#endif
	/* directory listing branch */
	if (strcmp(CCQcmd->cmdStr, "-l") == 0) {
		while (entry != NULL) {
			//char* msg = entry->d_name;

			strcpy(msg, entry->d_name); // seg fault?
			strcat(msg,"\n");
#ifdef INSTRUMENT
			printf("Sending listing. Message is:...\n%s", msg);

#endif
			send(CCQ->sockFD, msg, strlen(msg), 0);
#ifdef INSTRUMENT
			printf("Listing sent.\n");

#endif
			entry = readdir(servDir);
		}
		close(CCQ->sockFD);
		raise(SIGUSR2);
	}

	/* file transfer branch */
	if (strcmp(CCQcmd->cmdStr, "-g") == 0) {
#ifdef INSTRUMENT

		char msg[50] = "-g found.\n";
#endif
		write(1,msg,10);
		while (entry != NULL) {
			if (strcmp(entry->d_name, CCQcmd->fileStr) == 0) {

				target = fopen(entry->d_name, "r");

				/*send that file.*/
				_fileXfer(CCQ->sockFD, target);

				fclose(target);

				close(CCQ->sockFD);
				raise(SIGUSR2);
				return;
			}
			entry = readdir(servDir);
		} // end readdir iteration -- if end reached, file not found.

		close(CCQ->sockFD);
		// trigger event on other thread to say file not found, then exit.
		raise(SIGUSR1);
	}
	exit(0);
}

/* 
*	GPAF File I/O
*	get all bytes from a file and send them along the file descriptor 
*/
void _fileXfer(int FD, FILE* target) {
	int bytes_read = -1;
	char buf[512];
	while (bytes_read != 0) {
		bytes_read = fread(buf, 1, 512, target);
		if (bytes_read != 0) {
			send(FD, buf, bytes_read, 0); // blocked if Q full
		}
	}
}

void freeConnInfo(struct connInfo* CCX) {
	free(CCX->sockinfo);
	free(CCX);
}