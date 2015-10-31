CC = gcc

LIBS =  -lsocket\
	/home/courses/cse533/Stevens/unpv13e_solaris2.10/libunp.a \
	-lnsl -lm

FLAGS =  -g 
CFLAGS = ${FLAGS} -I/home/courses/cse533/Stevens/unpv13e_solaris2.10/lib

all: server.o client.o get_ifi_info_plus.o rtt.o 
	${CC} -o server server.o get_ifi_info_plus.o  rtt.o  ${LIBS}
	${CC} -o client client.o get_ifi_info_plus.o  rtt.o  ${LIBS}

rtt.o: rtt.c
	${CC} ${CFLAGS} -c rtt.c
server.o: server.c
	${CC} ${CFLAGS} -c server.c 
client.o: client.c
	${CC} ${CFLAGS} -c client.c 
get_ifi_info_plus.o: get_ifi_info_plus.c
	${CC} ${CFLAGS} -c get_ifi_info_plus.c

clean:
	rm  server client  *.o *~ *#

