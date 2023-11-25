/*
 * client.c - simple TCP chat client
 * Copyright (C) 2023 Antonio Addeo <antaddnf@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <arpa/inet.h>
#include <poll.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 50001
#define IP "127.0.0.1"

/* In main() first we create a client socket to connect to the server, then
 * we continously listen for messages from other clients and for console input */
int main() {

	/* ================================================= Client socket creation ================================================= *
	 * To instantiate the client socket we rely on:
	 * 1. socket() to create a socket that allows communication between processes on different hosts connected by IPV4
	 * 2. htons() to convert values between host and network byte order to avoid conflicts about endianess format
	 * 3. inet_pton() to convert IPv4 and IPv6 addresses from text to binary form
	 * 4. connect() to initiate the connection to the server
	 * ======================================================================================================================== */
	int client_fd;
	struct sockaddr_in serv_addr;
	char buffer[1024] = { 0 };
	int stdin_fd = fileno(stdin);

	if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("Socket creation error");
		exit(EXIT_FAILURE);
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);

	if (inet_pton(AF_INET, IP, &serv_addr.sin_addr) <= 0) {
		perror("Invalid address/ Address not supported");
		exit(EXIT_FAILURE);
	}

	if (connect(client_fd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0) {
		perror("Connection failed");
		exit(EXIT_FAILURE);
	}

	/* Read the welcome message */
	read(client_fd, buffer, sizeof(buffer));
	printf("%s", buffer);
	memset(buffer, 0, sizeof buffer);
	
	/* The set of file descriptors used to check incoming data is made up of:
	 * 1. the client file descriptor for incoming data from the server, that is messages from other clients
	 * 2. the input console where the user types the message */
	struct pollfd fds[2];
	int nfds = 2;
	fds[0].fd = client_fd;
	fds[0].events = POLLIN;
	fds[1].fd = stdin_fd;
	fds[1].events = POLLIN;

	int timeout = 10000;
	while (1) {
		/* We wait for events */
		int num_events = poll(fds, nfds, timeout);
		if (num_events > 0) {
			/* If there is a message from the server, let's show it */
			if (fds[0].revents & POLLIN) {
				memset(buffer, 0, sizeof buffer);
				read(client_fd, buffer, sizeof(buffer));
				printf("%s", buffer);
			}
			/* If the user has typed a message, let's send it to the server */
			if (fds[1].revents & POLLIN) {
				memset(buffer, 0, sizeof buffer);
				read(stdin_fd, buffer, sizeof(buffer));
				send(client_fd, buffer, strlen(buffer), 0);
			}
		}
	}

	return 0;
}
