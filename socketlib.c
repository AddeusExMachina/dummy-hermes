/*
 * socketlib.c - library for basic TCP socket management
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
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>

/* To create the server we instantiate a socket relying on:
 * 1. socket() to create a socket that allows communication between processes on different hosts connected by IPV4
 * 2. setsockopt() to enable the reuse of address and port */
int createServer() {
	int serverFD;
	int opt = 1;

	if ((serverFD = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("Socket creation error");
		exit(EXIT_FAILURE);
	}

	if (setsockopt(serverFD, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
		perror("setsockopt error");
		exit(EXIT_FAILURE);
	}

	return serverFD;
}

/* We put the server listening on a given port using:
 * 1. bind() to bind the socket to (local) address and port
 * 2. listen() to actually make the socket capable of listening for connections */
void setListenMode(int serverFD, int port) {
	struct sockaddr_in address;
	socklen_t addrlen = sizeof(address);

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(port);

	if (bind(serverFD, (struct sockaddr*)&address, addrlen) == -1) {
		perror("bind error");
		exit(EXIT_FAILURE);
	}

	if (listen(serverFD, 3) == -1) {
		perror("listen error");
		exit(EXIT_FAILURE);
	}
}

/* Create a socket from a client connection request and return the relative file descriptor. 
 * Retry in case of error. */
int acceptConnection(int serverFD) {
	struct sockaddr_in address;
	socklen_t addrlen = sizeof(address);
	int clientFD;
	while ((clientFD = accept(serverFD, (struct sockaddr*) &address, &addrlen)) == -1);
	return clientFD;
}

int createClient() {
	int clientFD;
	if ((clientFD = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("Socket creation error");
		exit(EXIT_FAILURE);
	}
	return clientFD;
}

void connectToServer(int clientFD, char *ip, int port) {
	struct sockaddr_in serverAddress;

	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(port);

	if (inet_pton(AF_INET, ip, &serverAddress.sin_addr) <= 0) {
		perror("Address is invalid or not supported");
		exit(EXIT_FAILURE);
	}

	if (connect(clientFD, (struct sockaddr*) &serverAddress, sizeof(serverAddress)) < 0) {
		perror("Connection failed");
		exit(EXIT_FAILURE);
	}
}
