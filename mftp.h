/* Christian Svinth - christian.svinth@wsu.edu	*/
/* CS 360 Final Project -- Due 4/28/19			*/
/*												*/
/* Header file with declarations and includes	*/
/* common to both executables, and one function	*/
/* declaration. 								*/

#ifndef MFTP_H
#define MFTP_H

/* Indicates what port to listen and connect on	*/
#define PORT_NUMBER 49999

/* Include statements */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <dirent.h>

/* sockGetLine function reads from socketFD until it 	*/
/* encounters a newline, and writes what it reads into	*/
/* the buffer pointed at by 'output'. It will read a 	*/
/* maximum of 'max' characters, and returns how many 	*/
/* characters it read. On error, -1 is returned. 		*/
int sockGetLine(int socketFD, void * output, int max);

#endif
