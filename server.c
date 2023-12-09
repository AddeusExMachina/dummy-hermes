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

#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "socketlib.h"

#define MAX_CLIENTS 1000
#define PORT 50001

/* For each client we keep information about its username and the relative position in the file descriptor set. */
struct Client {
	char* username;
	int fdsIndex;
};
struct Client *clients[MAX_CLIENTS];

/* The set of file descriptors used to check incoming data: one for the server plus one for each client */
struct pollfd fds[MAX_CLIENTS + 1];

/* Notes about the number of connected clients:
 * 1. numClients also represents the index of the last client in clients
 * 2. numClients+1 represents the index of the last client in fds, keep in mind that the 
 *    first entry in the set is always occupied by the server. */
int numClients = 0;

/* Discard all info about a client by releasing the related resources and by overriding
 * the entry of that client in the file descriptor set and in clients with the last entry,
 * the data in the last entry of both collections is then invalidated. */
void freeClient(struct Client *client, int i) {
	int fdsIndex = client->fdsIndex;
	close(fds[client->fdsIndex].fd);
	free(client->username);

	fds[client->fdsIndex].fd = fds[numClients].fd;
	fds[numClients].fd = -1;
	fds[numClients].events = 0;
	fds[numClients].revents = 0;

	/* Last client takes the position of current deleting client in file descriptor set */
	clients[numClients - 1]->fdsIndex = fdsIndex;
	clients[i] = clients[numClients - 1];
	clients[numClients - 1] = NULL;

	numClients--;
}

/* In main() first we create the server socket, then
 * we listen for connection requests and for messages from connected clients */
int main() {
	int serverFD = createServer();
	setListenMode(serverFD, PORT);

	char buffer[1024] = { 0 };

	/* At the beginning we will look for events on a single file descriptor,
	 * that is the server looking for new connections. */
	fds[0].fd = serverFD;
	fds[0].events = POLLIN;

	char* welcomeMessage =
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
				int usernameLength = snprintf(NULL, 0, "user%d", clientFD) + 1;
				char *username = (char *) malloc(usernameLength);
				snprintf(username, usernameLength, "user%d", clientFD);
				struct Client *client = malloc(sizeof(*client));
				client->username = username;
				client->fdsIndex = numClients + 1;

				clients[numClients] = client;

				fds[client->fdsIndex].fd = clientFD;
				fds[client->fdsIndex].events = POLLIN;

				numClients++;

				send(clientFD, welcomeMessage, strlen(welcomeMessage), 0);
			}

			for (int i = 0; i < numClients; i++) {
				struct Client* client = clients[i];
				int fdsIndex = client->fdsIndex;

				if (fds[fdsIndex].revents & POLLIN) {
					/* If there is activity on a client it means:
					 * 1. the client disconnected, or
					 * 2. there's a message from the client */
					int bytesRead = read(fds[fdsIndex].fd, buffer, sizeof(buffer) - 1);
					if (bytesRead <= 0) {
						/* The client disconnected. */
						freeClient(client, i);
					} else {
						/* The message may be a command or a text message for the other clients */
						if (buffer[0] == '\\') {
							if (strncmp(buffer+1, "setusername", 11) == 0) {
								/* The new username is the string after '\setusername ',
								 * whose length is 13. */
								int newUsernameLength = bytesRead - 13;
								char* newUsername = malloc(newUsernameLength);
								memcpy(newUsername, buffer + 13, newUsernameLength);
								newUsername[newUsernameLength - 1] = '\0';
								free(client->username);
								client->username = newUsername;
							} else if (strncmp(buffer+1, "exit", 4) == 0) {
								/* The user closed the connection */
								freeClient(client, i);
							}
						} else {
							/* If the client sent a message, broadcast the message */
							char message[1024];
							int messageLength = snprintf(message, sizeof(message), "%s> %s", client->username, buffer);

							for (int j = 0; j < numClients; j++) {
								if (i != j) {
									send(fds[clients[j]->fdsIndex].fd, message, messageLength, 0);
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
