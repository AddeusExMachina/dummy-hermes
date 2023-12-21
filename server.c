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

/* For each client we keep information about the username, the position in the file descriptor set
 * and pointers to next and previous clients. Clients are managed in a double linked list fashion
 * to easily iterate among them. */
struct Client {
	char* username;
	int fdsIndex;
	struct Client* next;
	struct Client* prev;
};
struct Client* head;
struct Client* tail;

/* A container from which a given client can be found: the key is actually the
 * client's username. */
struct bucket {
	char* key;
	struct Client* value;
	struct bucket* next;
};
struct bucket *hashtable[MAX_CLIENTS];

/* Simple hash evaluation for a string as in section 6.6 of 'The C Programming Language' */
int hash(char* s) {
	int hashValue;
	for (hashValue = 0; *s != '\0'; s++) {
		hashValue = *s + 31 * hashValue;
	}
	return hashValue % MAX_CLIENTS;
}

/* Remove a client's bucket from the hashtable collection by the key (client's username).
 * The removal is inspired by Linus Torvalds linked list argument where we take
 * advantage of using the undirect pointer b to avoid handling the special case
 * for removing the head. */
void deleteClientByUsername(char* username) {
	struct bucket **b = &hashtable[hash(username)];
	while (*b != NULL && strcmp((*b)->key, username) != 0) {
		b = &(*b)->next;
	}
	if (*b != NULL) {
		*b = (*b)->next;
	}
}

/* Insert a pair username-client in hashtable.
 * There is the same reasoning for the undirect pointer b as in deleteClientByUsername. */
void insertClient(char* username, struct Client* c) {
	struct bucket **b = &hashtable[hash(username)];
	while (*b != NULL) {
		b = &(*b)->next;
	}

	struct bucket* newBucket;
	newBucket = malloc(sizeof(newBucket));
	newBucket->key = strdup(username);
	newBucket->value = c;
	*b = newBucket;
}

/* Find the client, if present, contained in the bucket whose key is username. */
struct Client* getClientByUsername(char* username) {
	int hashValue = hash(username);
	struct bucket* curr = hashtable[hashValue];
	while (curr != NULL) {
		if (strcmp(curr->key, username) == 0) {
			/* Client found */
			return curr->value;
		} else {
			curr = curr->next;
		}
	}
	/* No client with that username has been found. */
	return NULL;
}

/* The set of file descriptors used to check incoming data: one for the server plus one for each client */
struct pollfd fds[MAX_CLIENTS + 1];

/* The number of connected clients: it's useful to specify how many items are in the fds array in poll(). */
int numClients = 0;

/* Discard all info about a client by releasing and overwriting the related resources.
 * As side effect we update the fds entries and if necessary also head and tail. */
void freeClient(struct Client *client) {
	/* Close that client's file descriptor. */
	close(fds[client->fdsIndex].fd);
	/* Deallocate the space for username string. */
	free(client->username);

	/* Tail client will occupy the entry in fds where the currently deleting client was at. 
	 * To do it we overwrite the client data in its fds entry with the tail data and invalidate the
	 * data present at tail index. */
	fds[client->fdsIndex].fd = fds[tail->fdsIndex].fd;
	fds[tail->fdsIndex].fd = -1;
	fds[tail->fdsIndex].events = 0;
	int revents = fds[tail->fdsIndex].revents;
	fds[tail->fdsIndex].revents = 0;
	tail->fdsIndex = client->fdsIndex;
	/* Ignore the returned events of tail only if it is the client removed, 
	 * otherwise restore that info for tail. */
	fds[tail->fdsIndex].revents = tail == client ? 0 : revents;

	/* The client that might be the new tail. */
	struct Client *prevTail = tail->prev;

	/* Update tail next pointer and client next prev paying attention if: 
	 * 1. we remove the tail (NULL reference)
	 * 2. we remove the client before tail (we may create a loop) */
	if (client != tail && client != prevTail) {
		tail->next = client->next;
		client->next->prev = tail;
	}
	tail->prev = client->prev;
	if (client == head) {
		/* If head is being removed update head with tail only if there is at least another client. */
		head = head == tail ? NULL : tail;
	} else {
		/* For any other client just update the next pointer of its previous. */
		client->prev->next = tail;
	}
	/* We update tail to prevTail only if we're not deleting prevTail, otherwise tail is unchanged. */
	if (prevTail != client) {
		tail = prevTail;
		if (tail != NULL) {
			tail->next = NULL;
		}
	}
}

/* In main() first we create the server socket, then
 * we listen for connection requests and for messages from connected clients */
int main() {
	int serverFD = createServer(PORT);

	char buffer[1024];

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
				fds[0].events = POLLIN;

				if (head == NULL) {
					/* First client inserted. */
					head = client;
				} else {
					/* Append the client to tail. */
					tail->next = client;
					client->prev = tail;
				}
				tail = client;

				insertClient(username, client);

				fds[client->fdsIndex].fd = clientFD;
				fds[client->fdsIndex].events = POLLIN;

				numClients++;

				send(clientFD, welcomeMessage, strlen(welcomeMessage), 0);
			}

			for (struct Client *client = head; client != NULL; client = client->next) {
				int fdsIndex = client->fdsIndex;

				if (fds[fdsIndex].revents & POLLIN) {
					/* If there is activity on a client it means:
					 * 1. the client disconnected, or
					 * 2. there's a message from the client */
					int bytesRead = read(fds[fdsIndex].fd, buffer, sizeof(buffer) - 1);
					if (bytesRead <= 0) {
						/* The client disconnected. */
						deleteClientByUsername(client->username);
						freeClient(client);
						numClients--;
					} else {
						/* The message may be a command or a text message for the other clients */
						if (buffer[0] == '\\') {
							/* Commands start with '\'. */
							if (strncmp(buffer+1, "setusername", 11) == 0) {
								/* The new username is the string after '\setusername ',
								 * whose length is 13. */
								int newUsernameLength = bytesRead - 13;
								char* newUsername = malloc(newUsernameLength);
								memcpy(newUsername, buffer + 13, newUsernameLength);
								newUsername[newUsernameLength - 1] = '\0';

								if (getClientByUsername(newUsername) != NULL) {
									send(fds[fdsIndex].fd, "Username already exists\n", 24, 0);
									memset(buffer, 0, sizeof buffer);
									continue;
								}
								deleteClientByUsername(client->username);
								insertClient(newUsername, client);
								free(client->username);
								client->username = newUsername;
							} else if (strncmp(buffer+1, "exit", 4) == 0) {
								/* The user closed the connection */
								deleteClientByUsername(client->username);
								freeClient(client);
							}
						} else {
							/* If the client sent a message, broadcast the message */
							char message[1024];
							int messageLength = snprintf(message, sizeof(message), "%s> %s", client->username, buffer);

							for (struct Client *c = head; c != NULL; c = c->next) {
								if (c != client) {
									send(fds[c->fdsIndex].fd, message, messageLength, 0);
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
