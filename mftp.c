/* Christian Svinth - christian.svinth@wsu.edu	*/
/* CS 360 Final Project							*/
/* Due April 28th, 2019							*/
/* Client file									*/

#include "mftp.h"
#include <sys/types.h>

int DEBUG = 0;

const char *hostInput;

/* Read from socketFD 1 byte at a time until a newline is reached, */
/* or until max number of bytes has been read. Result gets written */
/* into ouput, and the number of bytes read is returned. 		   */
int sockGetLine(int socketFD, void * output, int max){
    int i = 0;
    int check;
    char buf;
    char *out;
    out = output;			/* Stash original pointer as to not modify it */
    while((check = read(socketFD, &buf, 1)) >= 0){      /* Read one byte at a time */
        if(check == 0){
            fprintf(stderr, "Arg not terminated with newline\n");
            return 0;
        }
        
        if(buf == '\n'){                    /* Break if we are done */
            break;
        }
        else{
            i++;
            *out++ = buf;                   /*Otherwise, copy char read into buffer */
			if(i == max){
				break;
			}
        }
    }
    if(check == -1){ return -1; }
    *out = '\0';
    return i;
}

/* Given the socket id, return the portnumber of a new data connection	*/
/* on the host. If connection is not successful, print an error and 	*/
/* return -1.															*/
int establishDC(int controlFD){
	char *data = "D\n";					/* Indicate to server we want to establish a data connection */
	int portNum = 0;
	write(controlFD,data,2);
	if(DEBUG){printf("Sent request for DC to server\n"); }	

	char response[1024];										/* Read response from the server	*/
	int check = sockGetLine(controlFD, response, 1024);
	if(check <= 0){
		fprintf(stderr,"Connection closed unexpectedly or could not be read from -- %s\n",strerror(errno));
		exit(0);
	}
	if(DEBUG){printf("Read %s from the server\n",response); }
	if(response[0] == 'A'){
		char *port = strtok(response, "A\n");					/* Translate port number if positive ackowledgment */
		portNum = atoi(port);
	}
	else{
		fprintf(stderr,"Error reading port number from server -- %s\n",(response + 1));
		return -1;
	}
	int dataFD;
	dataFD = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in servAddr;
	struct hostent* hostEntry;
	struct in_addr **pptr;	

	/* First, determine if input was hostname or IPv4 Address */	
	unsigned char serverAddress[sizeof(struct in6_addr)];
	memset(&servAddr, 0, sizeof(servAddr));
	servAddr.sin_family = AF_INET;
	servAddr.sin_port = htons(portNum);
	
	if(inet_pton(AF_INET, hostInput, serverAddress) > 0){ /* if argument was a valid address, */
		hostEntry = gethostbyaddr(serverAddress, sizeof(serverAddress),AF_INET);	/* set up connection using the address */
	}
	else{
		hostEntry = gethostbyname(hostInput);	/* Otherwise, set up connection using name */
	}

	if(hostEntry == NULL){	/* Check for an error with setting up host */
		herror("hostent");
		return -1;
	}	

	pptr = (struct in_addr **) hostEntry->h_addr_list;	/* M a g i c  b u s i n e s s  */
	memcpy(&servAddr.sin_addr, *pptr, sizeof(struct in_addr));

	/* Connect, checking for error */
	if(connect(dataFD, (struct sockaddr *) &servAddr,sizeof(servAddr)) < 0){
		fprintf(stderr,"Problem connecting to server -- %s\n",strerror(errno));
		return -1;
	}
	else{
		fprintf(stdout,"Established data connection to server; using port %d\n",portNum);
	}
	return dataFD;

}

/* Ensure directory pathname exists, and make it the current directory */
void handleCD(char * pathname){
	DIR * dirCheck = opendir(pathname);
	if(dirCheck){
		closedir(dirCheck);
		chdir(pathname);
		char path[PATH_MAX];
		getcwd(path, PATH_MAX);
		printf("CWD is now %s\n",path);
	}
	else{
		fprintf(stderr,"Could not change directory to %s -- %s\n",pathname,strerror(errno));
	}
}

/* Using two child processes, pipe the output of "ls -l" to */
/* more, displaying 20 lines at a time. Wait until both of 	*/
/* the processes to exit before returning. 					*/
void handleLS(){
	int childPID = 0, child2PID = 0;
	int fd[2];

	if(pipe(fd) < 0){
		fprintf(stderr,"Error creating pipe -- %s\n",strerror(errno));
		return;
	}

	int rdr = fd[0], wtr = fd[1];
	if((childPID = fork()) == 0){
		close(rdr);
		close(1);
		dup2(wtr,1);
		execlp("ls","ls","-l",(char *)NULL);
		fprintf(stderr,"Error executing ls -- %s\n",strerror(errno));
	}
	else if((child2PID = fork()) == 0){
		close(wtr);
		close(0);
		dup2(rdr,0);
		execlp("more","more","-20",(char *)NULL);
		fprintf(stderr,"Error executing more -- %s\n",strerror(errno));
	}
	else{
		close(fd[0]);		/* Close the pipes in the parent, wait for children	*/
		close(fd[1]);
		wait(0);
		wait(0);
	}
}

/*Execute ls on the server, and pipe it's output to	*/
/* "more", displaying 20 lines at a time 			*/
void handleRLS(int controlFD){
	char *com = "L\n";
	char ackBuf[1024];
	int dataFD = establishDC(controlFD);		/* Establish a data connection with the server 	*/
	if(dataFD < 0){
		fprintf(stderr,"Data connection could not be established. Abandoning RLS call\n");
		return;
	}
	if(write(controlFD, com, 2) < 0){					/* Indicate we wish to execute LS				*/
		fprintf(stderr,"Error writing to the server -- %s\n",strerror(errno));
		exit(0);
	}
	if(sockGetLine(controlFD, ackBuf, 1024) <= 0){
		fprintf(stderr,"Control socket could not be read from or closed unexpectedly -- %s\n",strerror(errno));
		exit(0);
	}

	if(ackBuf[0] == 'A'){printf("Recieved acknowledgment\n"); }
	else{
		fprintf(stderr,"Error recieved from server: %s\n",(ackBuf+1));
		return;
	}
	int childPID = 0;
	if((childPID = fork()) == 0){		/* Fork off process to execute more */
		close(0);
		dup2(dataFD, 0);
		execlp("more","more","-20",(char *)NULL);
		fprintf(stderr,"Error executing more -- %s\n",strerror(errno));
		exit(0);
	}

	else{
		close(dataFD);		/* Close dataFD in the parent, then wait for more to finish executing */
		wait(0);
	}
}

/*Output the contents of remote file 'pathname' to stdout, 20 lines at a time */
void handleShow(int controlFD, char * pathname){
	char ackBuf[1024];
	int dataFD = establishDC(controlFD);			/* Establish a data connection */
	if(dataFD < 0){
		fprintf(stderr,"Data connection could not be established, abandoning 'show' call\n");
		return;
	}
	char argBuf[PATH_MAX + 3];
	argBuf[0] = '\0';
	strcpy(argBuf,"G");
	strcat(argBuf,pathname);		/* Write Get command to the server */
	strcat(argBuf,"\n");
	if(write(controlFD, argBuf, strlen(argBuf)) < 0){
		fprintf(stderr,"Error writing to the controlFD -- %s\n",strerror(errno));
		exit(0);
	}
	if(sockGetLine(controlFD, ackBuf, 1024) <= 0) {
		fprintf(stderr,"Control socket could not be read from or closed unexpectedly -- %s\n",strerror(errno));
		exit(0);
	}

	if(ackBuf[0] == 'A') {
		if(DEBUG){ printf("Received acknowledgment\n" ); }
	}
	else {
        fprintf(stderr,"Error recieved from server: %s\n",(ackBuf+1));
        return;
    }

	int childPID = 0;
	if((childPID = fork()) == 0){		/* Fork a new process to use 'more' for output from dataFD */
		close(0);
		dup2(dataFD,0);
		execlp("more","more","-20",(char *)NULL);
		fprintf(stderr,"Error executing 'more' -- %s\n",strerror(errno));
		exit(0);
	}
	else{
		close(dataFD);
		wait(0);		/* Wait for more to finish before returning to input loop	 */
	}
}

/* Get the contents of remote file at path 'pathname' on the server	*/
/* and write it to a new local file with the name of the remote file*/
void handleGet(int controlFD, char * pathname){
	char fullPath[PATH_MAX];		/* Parse file name from full path	*/
	strcpy(fullPath, pathname);		/* to determine name for local file	*/
	char * fileName;
	char * token;
	char ackBuf[512];
	fileName = pathname;
	token = strtok(pathname,"/");
	while(token != NULL){			/* This should get last bit of pathname, if present */
		fileName = token;
		token = strtok(NULL,"/");
	}

	int writeFD = open(fileName, O_CREAT | O_EXCL | O_WRONLY ,0777);	/* Create new file, checking for errors */
	if(writeFD < 0){
		fprintf(stderr,"Error opening file %s for creation -- %s\n",pathname,strerror(errno));
		return;
	}

	int dataFD = establishDC(controlFD);				/* Establish data connnection	*/
	if(dataFD < 0){
		fprintf(stderr,"Could not establish data connection. Abandoning 'get' call\n");
		close(writeFD);
		return;
	}

	char argBuf[PATH_MAX + 3];			/* Send get call over to the server */
	argBuf[0] = '\0';
	strcpy(argBuf,"G");
	strcat(argBuf,fullPath);
	strcat(argBuf,"\n");
	if(write(controlFD, argBuf, strlen(argBuf)) < 0){
		fprintf(stderr, "Error writing to socket -- %s\n",strerror(errno));
		exit(0);
	}
	if(sockGetLine(controlFD, ackBuf, 512) <= 0){
		fprintf(stderr,"Control socket could not be read from or closed unnexpectedly -- %s\n",strerror(errno));
		exit(0);
	}

	if(ackBuf[0] == 'A'){
		if(DEBUG){printf("Acknowldgement recieved\n"); }
	}
	else{
        fprintf(stderr,"Error recieved from server: %s\n",(ackBuf+1));
        return;
    }

	int bytesRead = 0;						/* Read from the data FD and write to the local file */
	char writeBuf[4096];
	while((bytesRead = read(dataFD, writeBuf, 4096)) > 0){
		if(DEBUG){ printf("Read %d bytes\n",bytesRead); }
		if(write(writeFD, writeBuf, bytesRead) < 0){
			fprintf(stderr,"Error writing to file %s -- %s\n",pathname,strerror(errno));
			close(writeFD);
			return;
		}
	}
	close(writeFD);							/* Close when done */
	close(dataFD);
	if(DEBUG){ printf("Get completed successfully\n"); }
	return;
}

/* Open local file pathname, and write it's contents 	*/
/* over a new data connection. 							*/
void handlePut(int controlFD, char * pathname){
	char fullPath[PATH_MAX];					/* Store the full path for open call, then break up */
	strcpy(fullPath, pathname);					/* pathname between '/' to find last element of the */
	char * token;								/* path (the file name)								*/
	char * filename = pathname;
	token = strtok(pathname, "/");
	while(token){
		filename = token;
		token = strtok(NULL,"/");
	}
	int readFD = open(fullPath, O_RDONLY);
	if(readFD < 0){
		fprintf(stderr, "Error opening %s for reading -- %s\n",fullPath,strerror(errno));
		return;
	}

	int dataFD = establishDC(controlFD);													/* Establish data connection */
	if(dataFD < 0){
		fprintf(stderr, "Problem establishing data connection -- %s\n",strerror(errno));
		return;
	}

	char tellServ[PATH_MAX + 3] = "\0";							/*Send put command to server */
	strcpy(tellServ,"P");
	strcat(tellServ,filename);
	strcat(tellServ,"\n");
	if(write(controlFD,tellServ,strlen(tellServ)) < 0){
		fprintf(stderr,"Error: Control socket could not be written to -- %s\n",strerror(errno));
		exit(0);
	}

	char ackBuf[1024];
	if(sockGetLine(controlFD,ackBuf,1024) <= 0){
		fprintf(stderr,"Control socket could not be read from or closed unexpectedly -- %s\n",strerror(errno));
		exit(0);
	}

	if(ackBuf[0] == 'A'){
		if(DEBUG) { printf("Received acknowledgment\n"); }
	}
	else{
        fprintf(stderr,"Error recieved from server: %s\n",(ackBuf+1));
        return;
    }

	int bytesToWrite = 0;
	char writeBuf[4096];
	while((bytesToWrite = read(readFD,writeBuf,4096)) > 0){
		if(write(dataFD,writeBuf,bytesToWrite) < 1){
			fprintf(stderr,"Problem writing to dataFD -- %s\n",strerror(errno));
			return;
		}
	}
	if(bytesToWrite < 0){
		fprintf(stderr,"Error reading from the file -- %s\n",strerror(errno));
		close(readFD);
		close(dataFD);
		return;
	}
	if(DEBUG){printf("Put completed successfully\n"); }
	close(readFD);
	close(dataFD);
}

/* Send C command over controlFD with given pathname as the argument.*/
/* Read acknowledgment or print error if necessary 					 */
void handleRCD(int controlFD, char * pathname){
    char ackBuf[512];
	write(controlFD, "C",1);
	write(controlFD,pathname,strlen(pathname));
	write(controlFD,"\n",1);
    int check = sockGetLine(controlFD, ackBuf, 512);
    if(check < 1){
        fprintf(stderr,"Could not read from server -- %s\n",strerror(errno));
        exit(0);
    }
    if(ackBuf[0] == 'A'){
		if(DEBUG){ printf("Ackowledgment received\n"); }
	}
	else{
		fprintf(stderr, "Error received from server -- %s\n",ackBuf+1);
	}
}

/* Called once control connection has been established	*/
/* Constantly reads and handles input from stdin 		*/
void inputLoop(int controlFD){
	char inputString[1024];
	char seperators[] = " ,\t\n";
	char * token;
	char * arg;
	char * pathname;

	/* Loop reads in a line from the user, then parses it as arguments to the	*/
	/* command specified by the first token of the input. It will then call		*/
	/* the appropriate handler. 			 									*/
	while(1){
		printf("MFTP>");
		fgets(inputString,1024,stdin);
		token = strtok(inputString, seperators);
		if(!token){continue;}
		arg = token;								
		token = strtok(NULL,seperators);	
		int argCount = 0;
		if(token){ 
			pathname = token;	/*If we got an argument, it should be a pathname */
			argCount++;
		}
		while(token){
			token = strtok(NULL,seperators);
			argCount++;
		}
		
		/* Exit */
        if(strcmp(arg,"exit") == 0){
            char exitBuf[2] = "Q\n";
            write(controlFD,exitBuf,2);
            break;
        }

		/* cd */
		if(strcmp(arg,"cd") == 0){					/* If we recieved cd command, ensure proper arguments and go to handler */
			if(argCount != 2){
				fprintf(stderr,"Too many arguments to cd! Usage is: cd <pathname>\n");
			 }
			else{
				handleCD(pathname);
			}
			continue;
		}

		/* get */
		if(strcmp(arg,"get") == 0){
			if(argCount != 2){
				fprintf(stderr,"Incorrect use of get. Usage: get <remotePathname>\n");
			}
			else{
				handleGet(controlFD, pathname);
			}
			continue;
		}
		
		/* LS */
		if(strcmp(arg,"ls") == 0){
			if(argCount != 0){
				fprintf(stderr,"ls takes no arguments. Usage is: ls\n");
			}
			else{
				handleLS();
			}
			continue;
		}

		/* RLS */
		if(strcmp(arg,"rls")==0){
			handleRLS(controlFD);
			continue;
		}

		/* RCD */
		if(strcmp(arg, "rcd") == 0){
			if(argCount != 2){
				fprintf(stderr,"Incorrect arg format for rcd -- usage is: rcd <pathname>\n");
				continue;
			}
			else{
				handleRCD(controlFD, pathname);
			}
			continue;
		}

		/* Show */
		if(strcmp(arg,"show") == 0){
			if(argCount != 2){
				fprintf(stderr, "Incorrect usage of show -- usage is: show <remotePathname>\n");
			}
			else{
				handleShow(controlFD, pathname);
			}
			continue;
		}
		if(strcmp(arg,"put") == 0){
			if(argCount != 2){
				fprintf(stderr, "Incorrect usage of put -- usage is: put <localPathname>\n");
			}
			else{
				handlePut(controlFD, pathname);
			}
			continue;
		}
		
		/* Default case */
		fprintf(stdout,"Unknown command \"%s\" -- ignoring\n",arg);
	}
	return;
	
}

/*Checks for debug, then establishes connection with given host info*/
int main(int argc, char * argv[]){
	if(argc == 3 && strcmp(argv[1],"-d") == 0){
		DEBUG = 1;
		hostInput = argv[2];
	}
	else if(argc != 2){
		fprintf(stderr,"Please enter address or name of host. Usage is:\n\t./mftp [-d] <hostname>\n");
		exit(0);
	}
	else{
		hostInput = argv[1];
	}

	int socketFD;
	socketFD = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in servAddr;
	struct hostent* hostEntry;
	struct in_addr **pptr;	

	/* First, determine if input was hostname or IPv4 Address */	
	unsigned char serverAddress[sizeof(struct in6_addr)];
	memset(&servAddr, 0, sizeof(servAddr));
	servAddr.sin_family = AF_INET;
	servAddr.sin_port = htons(PORT_NUMBER);
	
	if(inet_pton(AF_INET, hostInput, serverAddress) > 0){ /* if argument was a valid address, */
		hostEntry = gethostbyaddr(serverAddress, sizeof(serverAddress),AF_INET);	/* set up connection using the address */
	}
	else{
		hostEntry = gethostbyname(hostInput);	/* Otherwise, set up connection using name */
	}

	if(hostEntry == NULL){	/* Check for an error with setting up host */
		herror("hostent");
		exit(0);
	}	

	pptr = (struct in_addr **) hostEntry->h_addr_list;	/* M a g i c  b u s i n e s s  */
	memcpy(&servAddr.sin_addr, *pptr, sizeof(struct in_addr));

	/* Connect, checking for error */
	if(connect(socketFD, (struct sockaddr *) &servAddr,sizeof(servAddr)) < 0){
		fprintf(stderr,"Problem connecting to server -- %s\n",strerror(errno));
		exit(0);
	}
	else{
		fprintf(stdout,"Established connection to server -- Hostname: %s\n",hostEntry->h_name);
	}
	
	inputLoop(socketFD);		/* Go to input loop */
	close(socketFD);				/* Close the socket				*/
	exit(0);		
}

