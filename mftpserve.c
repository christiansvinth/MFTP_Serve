/* Christian Svinth - christian.svinth@wsu.edu	*/
/* CS 360 Assignment 9 							*/
/* Due April 28th, 2019							*/
/* Server file									*/

#include "mftp.h"
#include <ctype.h>

static int DEBUG = 0;
static int thisPID = 0;


/* Read from socketFD 1 byte at a time until a newline is reached, */
/* or until max number of bytes has been read. Result gets written */
/* into ouput, and the number of bytes read is returned. 		   */
int sockGetLine(int socketFD, void * output, int max){
    int i = 0;
    int check;
    char buf;
    char *out;
    out = output;									/* Stash original pointer as to not modify it */
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
    if(check == -1){ return -1; }			/* Check for an error with read */
    *out = '\0';					/* Null terminate string */
    return i;
}



/* Establish a new data connection, using controlFD as the control connection */
/* Returns the fd of the data connection, or a -1 if an error occurred		  */
/* Error handling for accept, bind, and getsockname are dealt with by the 	  */
/* function which calls establishDC											  */
int establishDC(int controlFD){
	int listenFD, dataFD, check;
	listenFD = socket(AF_INET, SOCK_STREAM,0);			/* Set up port to listen for DC */
	if(listenFD < 0){
		fprintf(stderr,"Error creating socket -- %s\n",strerror(errno));
		return -1;
	}

	struct sockaddr_in servAddr;
	memset(&servAddr,0,sizeof(servAddr));
	servAddr.sin_family = AF_INET;
	servAddr.sin_port = htons(0);
	servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	
	if(bind(listenFD, (struct sockaddr *)&servAddr,sizeof(servAddr)) < 0){
		perror("bind");
		return -1;
	}
	struct sockaddr_in dataAddr;					/* Create struct for DC, initialize to 0 for getsockname */
	memset(&dataAddr,0,sizeof(dataAddr));
	socklen_t dataSize = sizeof(dataAddr);
	check = getsockname(listenFD, (struct sockaddr *)&dataAddr, &dataSize);
	if(check < 0){
		fprintf(stderr,"Problem retrieving socket name -- %s",strerror(errno));
		return -1;
	}
	int dataPort = ntohs(dataAddr.sin_port);
	if(DEBUG){ printf("Data connection socket bound to port %d\n",dataPort); }
	char buf[7];	
	sprintf(buf,"A%d\n",dataPort);
	write(controlFD,buf,7);			/*Write acknowledgment to the client with port number */

	listen(listenFD, 0);
	socklen_t length;
	length = sizeof(struct sockaddr_in);
	struct sockaddr_in clientAddr;
	if(DEBUG){ printf("Awaiting acceptance of client\n"); }
	dataFD = accept(listenFD, (struct sockaddr *) &clientAddr, &length);
	if(DEBUG){ printf("Data connection established successfully; using FD %d\n",dataFD); }
	return dataFD;

}

/* Retrieve the file at pathname and transmit over the 	*/
/* file descriptor dataFD. Send confirmation over the 	*/
/* controlFD if successful, otherwise transmit error. 	*/
void handleGet(int controlFD, int dataFD, char * pathname){
	char errorBuff[1024] = "\0";
	int readFD = open(pathname, O_RDONLY);								/* Open for reading, checking for error */
	if(readFD < 0){
		fprintf(stderr, "Could not open %s -- %s\n",pathname,strerror(errno));
		sprintf(errorBuff,"ECouldn't open remote file %s -- %s\n",pathname,strerror(errno));
		write(controlFD,errorBuff,strlen(errorBuff));
		fprintf(stdout,"Child %d> G %s failed to complete successfully\n",thisPID,pathname);
		return;
	}
	int bytesRead = 0;
	char writeBuf[4096];										/* Read from the file and write to the dataFD, checking for error */
	while((bytesRead = read(readFD, writeBuf, 4096)) > 0){
		if(write(dataFD, writeBuf, bytesRead) < 1){
			fprintf(stderr, "Error encountered writing to dataFD -- %s\n",strerror(errno));
			sprintf(errorBuff,"EProblem on server writing to the dataFD -- %s\n",strerror(errno));
			write(controlFD,errorBuff,strlen(errorBuff));
			fprintf(stdout,"Child %d> G %s failed to complete successfully\n",thisPID,pathname);
			return;
		}
	}
	if(bytesRead < 0){											/*Ensure reading successful */
		fprintf(stderr, "Error reading from %s -- %s\n",pathname,strerror(errno));
		sprintf(errorBuff,"EError reading from remote file %s -- %s\n",pathname,strerror(errno));
		write(controlFD,errorBuff,strlen(errorBuff));
		fprintf(stdout,"Child %d> G %s failed to complete successfully\n",thisPID,pathname);
		return;
	}
	write(controlFD,"A\n",2);	/*Send acknowledgement if successful */
	fprintf(stdout,"Child %d> G %s completed successfully\n",thisPID,pathname);
	close(readFD);
}

/* Write data read from dataFD into the file at 'pathname'.	 */
void handlePut(int controlFD, int dataFD, char * pathname){
	char errorBuff[1024] = "\0";
	int writeFD = open(pathname, O_CREAT | O_EXCL | O_WRONLY, 0777);	/* Open file for writing, checking for errors */
	if(writeFD < 0){
		fprintf(stderr,"Can't open file %s for creation -- %s\n",pathname,strerror(errno));
		sprintf(errorBuff,"ECouldn't open %s for creation -- %s\n",pathname,strerror(errno));
		write(controlFD,errorBuff,strlen(errorBuff));
		fprintf(stdout,"Child %d> P %s failed to complete successfully\n",thisPID,pathname);
		return;
	}

	int bytesRead = 0;
	char writeBuf[4096];
	while((bytesRead = read(dataFD, writeBuf, 4096)) > 0){				/* Read from the dataFD and write to the file, checking for errors */
		if(DEBUG){ printf("Read %d bytes from the dataFD\n",bytesRead); }
		if(write(writeFD, writeBuf, bytesRead) < 0){
			fprintf(stderr,"Problem writing to %s -- %s\n",pathname,strerror(errno));
			sprintf(errorBuff,"EError writing to %s -- %s\n",pathname,strerror(errno));
			write(controlFD,errorBuff,strlen(errorBuff));
			close(writeFD);
			fprintf(stdout,"Child %d> P %s failed to complete successfully\n",thisPID,pathname);
			return;
		}
		else{
			if(DEBUG){ printf("Wrote %d bytes to the writeFD\n",bytesRead); }
		}
	}
	if(DEBUG){printf("Finished put, closing file\n"); }
	write(controlFD, "A\n",2);							/* Send acknowledgment to client */
	close(writeFD);
	fprintf(stdout,"Child %d> P %s was completed succesfully\n",thisPID,pathname);
}

/* Ensure pathname is a valid directory, and navigate to it.*/
/* Write errors or acknowledgment over controlFD.			*/
void handleRCD(int controlFD, char * pathname){
	char errorBuff[1024] = "\0";
	DIR * dirCheck = opendir(pathname);
	if(dirCheck){
		closedir(dirCheck);
		chdir(pathname);
		char path[PATH_MAX];
		getcwd(path, PATH_MAX);
		fprintf(stdout,"Changed directory to %s\n",path);
		write(controlFD, "A\n", 2);
		fprintf(stdout,"Child %d> C %s completed successfully\n",thisPID,pathname);
	}
	else{
		fprintf(stderr,"Could not change directory to %s -- %s\n",pathname,strerror(errno));
		fprintf(stdout,"Child %d> C %s failed to complete\n",thisPID,pathname);
		sprintf(errorBuff,"ECouldn't change directory to %s -- %s\n",pathname,strerror(errno));
		write(controlFD,errorBuff,strlen(errorBuff));
	}
}

/* Fork of an ls process and redirect output to dataFD. */
void handleRLS(int controlFD, int dataFD){
	if(fork() == 0){
		close(1);
		dup2(dataFD,1);
		execlp("ls","ls","-l",(char *)NULL);
		fprintf(stderr,"Problem executing LS -- %s\n",strerror(errno));
	}
	else{
		close(dataFD);
		wait(0);
		write(controlFD, "A\n",2);
		if(DEBUG){printf("Finished ls process\n");}
	}
}

/* Read and handle input from controlFD until exit command or connection closes */
void connectionHandler(int controlFD){
	int dataFD = -1;
	while(1){
		char inputBuffer[1024] = "\0";
		char errorBuffer[1024] = "\0";
		char * pathname;
		char command;
		int check = sockGetLine(controlFD, inputBuffer, 1024); /* Read a line from the control socket */
		if(check == 0){
			fprintf(stderr, "Client disconnected unexpectedly\n"); 
			break; 
		}							  								/* Break if connection was closed */
		if(isspace(inputBuffer[0])){ continue; }
		command = inputBuffer[0];							/* Command is first char of input	*/
		pathname = inputBuffer + 1;							/* Optional pathname would begin 	*/
															/* with 2nd char of the buffer		*/
		/* Exit */
		if(command  == 'Q'){
			fprintf(stdout,"Child %d>Exit command received\n",thisPID);
			write(controlFD, "A\n",2);
			return;
		}

		/* RLS */
		if(command == 'L'){
			fprintf(stdout,"Child %d>Recieved command \'L\'\n",thisPID);
			if(dataFD < 0){
				fprintf(stderr, "RLS attempted with no open data connection\n");
				strcpy(errorBuffer, "EError: Data connection not previously established\n");
				write(controlFD, errorBuffer, strlen(errorBuffer));
			}
			else{
				if(DEBUG){printf("Calling rls; dataFD is %d\n",dataFD); }
				handleRLS(controlFD, dataFD);
				close(dataFD);
				dataFD = -1;
			}
			continue;
		}
		
		/* RCD */
		if(command == 'C'){
			fprintf(stdout,"Child %d> Received command \'C\' with arg \'%s\'\n",thisPID,pathname);
			handleRCD(controlFD, pathname);
			continue;
		}

		/* Put */
		if(command == 'P'){
			if(DEBUG){printf("Received call to P -- dataFD is %d\n",dataFD); }
			fprintf(stdout,"Child %d> Received command \'P\' with args \'%s\'\n",thisPID,pathname);
			if(dataFD < 0){
				fprintf(stderr,"Put call attempted with no open data socket\n");
				strcpy(errorBuffer,"ECannot execute Put -- No data connection established\n");
				write(controlFD,errorBuffer,strlen(errorBuffer));
			}
			else{
				handlePut(controlFD, dataFD, pathname);
				close(dataFD);
				dataFD = -1;
			}
			continue;
		}

		/* Get */
		if(command == 'G'){
			fprintf(stdout,"Child %d>Received call to \'G\' with args \'%s\'\n",thisPID,pathname);
			if(dataFD < 0){
				fprintf(stderr,"Get call attempted with no open data socket\n");
                strcpy(errorBuffer,"ECannot execute Get -- No data connection established\n");
                write(controlFD,errorBuffer,strlen(errorBuffer));

			}
			else{
				handleGet(controlFD, dataFD, pathname);
				close(dataFD);
				dataFD = -1;
			}
			continue;
		}

		/* Data connection */
		if(command == 'D'){
			fprintf(stdout,"Child %d>Received request for data connection\n",thisPID);
			dataFD = establishDC(controlFD);
			if(dataFD < 0){
                sprintf(errorBuffer,"EProblem establishing data connection -- %s\n",strerror(errno));
                write(controlFD,errorBuffer,strlen(errorBuffer));
			}
			continue;
		}
		/* Default case; print error and send error to client */
		printf("Child %d>Command \'%c\'did not match any known value -- Ignoring\n",thisPID,command);
        sprintf(errorBuffer,"ECommand \'%s\' did not match any known command\n",inputBuffer);
        write(controlFD,errorBuffer,strlen(errorBuffer));
		
	}
}

/* Main sets up listen on port PORT_NUMBER and awaits new connections */
int main(int argc, char * argv[]){
	if(argc == 2 && strcmp(argv[1],"-d") == 0){		/* Toggle DEBUG if we recieved the flag as the first arg */
		DEBUG = 1;
	}

	int listenFD, connectFD;
	listenFD = socket(AF_INET, SOCK_STREAM,0);
	if(listenFD < 0){
		fprintf(stderr,"Error creating socket -- %s\n",strerror(errno));
		exit(1);
	}
	struct sockaddr_in servAddr;
	memset(&servAddr,0,sizeof(servAddr));
	servAddr.sin_family = AF_INET;
	servAddr.sin_port = htons(PORT_NUMBER);
	servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	
	if(bind(listenFD, (struct sockaddr *)&servAddr,sizeof(servAddr)) < 0){
		perror("bind");
		exit(1);
	}
	listen(listenFD,4);
	socklen_t length;
	length = sizeof(struct sockaddr_in);
	while(1){
		struct sockaddr_in clientAddr;
		connectFD = accept(listenFD, (struct sockaddr *) &clientAddr, &length);
		int childPID = 0;
		if((childPID = fork())){
			/* Parent process */
			waitpid(-1, NULL, WNOHANG);	/* As to prevent zombie processes */
			continue;
		}
		else{
			/* Child Process */
			struct hostent * hostEntry;				/* Retrieve and print hostname */
			char* hostName;
			thisPID = getpid();
			hostEntry = gethostbyaddr(&(clientAddr.sin_addr),sizeof(struct in_addr), AF_INET);
			hostName = hostEntry->h_name;
			printf("Child %d>Hostname is %s\n",thisPID,hostName);
			connectionHandler(connectFD);				/* Pass socket FD off to connection handler */
			printf("Child %d>Client dissconnected\n",thisPID);
			close(connectFD);								/* Close the connection */
			exit(0);
		}
	}
}

