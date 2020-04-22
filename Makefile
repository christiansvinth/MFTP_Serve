.SUFFIXES: .c .o
CC = gcc
CCFLAGS = -pedantic -Wall -g

all: mftp mftpserve

mftp: mftp.c
	${CC} ${CCFLAGS} -o mftp mftp.c

mftpserve: mftpserve.c
	${CC} ${CCFLAGS} -o mftpserve mftpserve.c

clean:
	rm -f mftp mftpserve

