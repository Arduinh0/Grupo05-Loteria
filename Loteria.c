#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>

struct sockaddr_in adrs;

int main(void){
	int svr_sckt = socket(AF_INET6, SOCK_STREAM, 0);
	
	adrs.sin_family = AF_INET6;
	adrs.sin_port = htons(8080);
	adrs.sin_addr.s_addr = INADDR_ANY;
}