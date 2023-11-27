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

/* For each client we keep information about its username and the file descriptor.
 * TODO: the value of username is set to the string "userN" where N is the file descriptor of the client.
 * A client should be allowed to choose its own username */
struct Client {
	char* username;
	int fd;
};

struct Client *clients[MAX_CLIENTS];

/* In main() first we create the server socket, then
 * we listen for connection requests and for messages from connected clients */
int main() {

	/* ================================================= Server socket creation ================================================= *
	 * To instantiate the server socket we rely on:
	 * 1. socket() to create a socket that allows communication between processes on different hosts connected by IPV4
	 * 2. setsockopt() to enable the reuse of address and port
	 * 3. bind() to bind the socket to address and port
	 * 4. listen() to make the socket wait for incoming connection requests
	 * ======================================================================================================================== */
	int server_fd;
	struct sockaddr_in address;
	int opt = 1;
	socklen_t addrlen = sizeof(address);
	char buffer[1024] = { 0 };

	if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("Socket creation error");
		exit(EXIT_FAILURE);
	}

	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
		perror("setsockopt error");
		exit(EXIT_FAILURE);
	}

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(PORT);
	if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) == -1) {
		perror("bind error");
		exit(EXIT_FAILURE);
	}

	if (listen(server_fd, 3) == -1) {
		perror("listen error");
		exit(EXIT_FAILURE);
	}

	/* The set of file descriptors used to check incoming data: one for the server plus one for each client */
	struct pollfd fds[MAX_CLIENTS + 1];
	/* At the beginning we will look for events on a single file descriptor,
	 * that is the server looking for new connections.
	 * The first entry in the set is always occupied by the server */
	fds[0].fd = server_fd;
	fds[0].events = POLLIN;
	int nfds = 1;

	char* welcome_message =
		"=============================\n"
		" Hello, Welcome in this chat \n"
		"=============================\n";

	int timeout = 10000;

	while (1) {
		/* We wait for events */
		int num_events = poll(fds, nfds, timeout);
		if (num_events == -1) {
			perror("poll() error");
			exit(1);
		} else if (num_events) {
			/* Some file descriptors reported an event */
			if (fds[0].revents & POLLIN) {
			/* If the server received a connection request:
			 * 1. we append a new file descriptor to the set of file descriptors to be monitored for reading
			 * 2. we append a new client to clients */
				int client_socket = accept(server_fd, (struct sockaddr*) &address, &addrlen);
				send(client_socket, welcome_message, strlen(welcome_message), 0);
				fds[nfds].fd = client_socket;
				fds[nfds].events = POLLIN;

				struct Client *c = malloc(sizeof(*c));
				/* We evaluate the size of the string made up of "user" followed by
				 * the client file descriptor, an integer known at runtime */
				int username_length = snprintf(NULL, 0, "user%d", client_socket) + 1;
				char *username = (char *) malloc(username_length);
				snprintf(username, username_length, "user%d", client_socket);
				c->username = username;
				c->fd = client_socket;
				clients[nfds - 1] = c;

				nfds += 1;
			}
			for (int i = 1; i < nfds; i++) {
				if (fds[i].revents & POLLIN) {
					/* If there is activity on a client it means:
					 * 1. there's a message from the client, or
					 * 2. the client disconnected */
					int bytes_received = read(fds[i].fd, buffer, sizeof(buffer) - 1);
					if (bytes_received <= 0) {
						/* If the client disconnected we discard all info about it: we do it by
						 * releasing the related resources and by overriding the entry of that client
						 * in the file descriptor set and in the clients array with the last entry,
						 * the data in the last entry of both collections is then invalidated. */
						close(fds[i].fd);
						fds[i].fd = fds[nfds - 1].fd;
						fds[nfds - 1].fd = -1;
						fds[nfds - 1].events = 0;
						fds[nfds - 1].revents = 0;

						free(clients[i - 1]->username);
						clients[i - 1] = clients[nfds - 2];
						clients[nfds - 1] = NULL;

						nfds -= 1;
						memset(buffer, 0, sizeof buffer);
					} else {
						/* If the client sent a message, broadcast the message */
						int message_length = strlen(clients[i - 1]->username) + strlen(buffer) + 2;
						char message[message_length];
						snprintf(message, message_length, "%s> %s", clients[i - 1]-> username, buffer);
						for (int j = 1; j < nfds; j++) {
							if (i != j) {
								send(fds[j].fd, message, message_length, 0);
							}
						}
						memset(buffer, 0, sizeof buffer);
					}
				}
			}
		}
	}

	return 0;
}
