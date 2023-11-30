/*
 * server.c - simple TCP chat server
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

#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_CLIENTS 1000
#define PORT 50001

/* For each client we keep information about its username and the relative position in the file descriptor set. */
struct Client {
	char* username;
	int fdsIndex;
};

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

struct Client *clients[MAX_CLIENTS];

/* In main() first we create the server socket, then
 * we listen for connection requests and for messages from connected clients */
int main() {
	int serverFD = createServer();
	setListenMode(serverFD, PORT);

	char buffer[1024] = { 0 };

	/* The set of file descriptors used to check incoming data: one for the server plus one for each client */
	struct pollfd* fds = malloc(MAX_CLIENTS + 1);
	/* At the beginning we will look for events on a single file descriptor,
	 * that is the server looking for new connections. */
	fds[0].fd = serverFD;
	fds[0].events = POLLIN;
	/* Notes about the number of connected clients:
	 * 1. numClients also represents the index of the last client in clients
	 * 2. numClients+1 represents the index of the last client in fds, keep in mind that the 
	 *    first entry in the set is always occupied by the server. */
	int numClients = 0;

	char* welcome_message =
		"=============================\n"
		" Hello, Welcome in this chat \n"
		"=============================\n";
	int timeout = 10000;

	while (1) {
		/* We wait for events */
		int numEvents = poll(fds, numClients + 1, timeout);
		if (numEvents == -1) {
			perror("poll() error");
			exit(EXIT_FAILURE);
		} else if (numEvents) {
			/* Some file descriptors reported an event */
			if (fds[0].revents & POLLIN) {
			/* If the server received a connection request we append a new client
			 * whose file descriptor will be monitored for reading */
				int clientFD = acceptConnection(serverFD);

 				/* The default value of username is set to the string "user<FD>" where <FD> is the file descriptor of that client. */
				int username_length = snprintf(NULL, 0, "user%d", clientFD) + 1;
				char *username = (char *) malloc(username_length);
				snprintf(username, username_length, "user%d", clientFD);
				struct Client *client = malloc(sizeof(*client));
				client->username = username;
				client->fdsIndex = numClients + 1;

				clients[numClients] = client;

				fds[client->fdsIndex].fd = clientFD;
				fds[client->fdsIndex].events = POLLIN;

				numClients += 1;

				send(clientFD, welcome_message, strlen(welcome_message), 0);
			}

			for (int i = 0; i < numClients; i++) {
				struct Client* client = clients[i];
				int fdsIndex = client->fdsIndex;

				if (fds[fdsIndex].revents & POLLIN) {
					/* If there is activity on a client it means:
					 * 1. the client disconnected, or
					 * 2. there's a message from the client */
					int bytes_received = read(fds[fdsIndex].fd, buffer, sizeof(buffer) - 1);
					if (bytes_received <= 0) {
						/* If the client disconnected we discard all info about it: we do it by
						 * releasing the related resources and by overriding the entry of that client
						 * in the file descriptor set and in clients with the last entry,
						 * the data in the last entry of both collections is then invalidated. */
						close(fds[fdsIndex].fd);
						free(client->username);

						fds[fdsIndex].fd = fds[numClients].fd;
						fds[numClients].fd = -1;
						fds[numClients].events = 0;
						fds[numClients].revents = 0;

						/* Last client takes the position of current deleting client in file descriptor set */
						clients[numClients - 1]->fdsIndex = fdsIndex;
						clients[i] = clients[numClients - 1];
						clients[numClients - 1] = NULL;

						numClients -= 1;
					} else {
						/* The message may be a command or a text message for the other clients */
						if (buffer[0] == '\\') {
						/* Assume for now that when the first character in a message is a backslash
						 * the client entered the command for changing its username */
							/* the new username is the string right after "\setusername ",
							 * whose length is 13 (including the space) */
							int new_username_length = bytes_received - 13;
							char* new_username = malloc(new_username_length);
							memcpy(new_username, buffer + 13, new_username_length);
							new_username[new_username_length - 1] = '\0';
							free(client->username);
							client->username = new_username;
						} else {
							/* If the client sent a message, broadcast the message */
							int message_length = strlen(client->username) + bytes_received + 2;
							char message[message_length];
							snprintf(message, message_length, "%s> %s", client->username, buffer);

							for (int j = 0; j < numClients; j++) {
								if (i != j) {
									send(fds[clients[j]->fdsIndex].fd, message, message_length, 0);
								}
							}
						}
					}
					memset(buffer, 0, sizeof buffer);
				}
			}
		}
	}

	return 0;
}
