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
#define MAX_CHANNELS 100
#define PORT 50001

/* For each client we keep information about the username, the position in the file descriptor set and the cannel.
 * Moreover there are pointers to next and previous clients in the chat and in the same channel. */
struct Client {
	char* username;
	int fdsIndex;
	struct Client* nextInChat;
	struct Client* prevInChat;
	struct Client* nextInChannel;
	struct Client* prevInChannel;
	struct Channel* channel;
};
struct Client* chatHead;
struct Client* chatTail;

/* A container from which a given client can be found: the key is actually the
 * client's username. */
struct ClientBucket {
	char* key;
	struct Client* value;
	struct ClientBucket* nextInChat;
};
struct ClientBucket *clientHashtable[MAX_CLIENTS];

struct Channel {
	char *name;
	struct Channel *nextInChat;
	struct Client* head;
	struct Client* tail;
};

/* A container from which a given channel can be found: the key is actually the
 * channel's name. */
struct ChannelBucket {
	char* key;
	struct Channel* value;
	struct ChannelBucket* nextInChat;
};
struct ChannelBucket *channelHashtable[MAX_CHANNELS];

/* Simple hash evaluation for a string as in section 6.6 of 'The C Programming Language' */
int hash(char* s, int size) {
	int hashValue;
	for (hashValue = 0; *s != '\0'; s++) {
		hashValue = *s + 31 * hashValue % size;
	}
	return hashValue;
}

/* Remove a client's bucket from the hashtable collection by the key (client's username).
 * The removal is inspired by Linus Torvalds linked list argument where we take
 * advantage of using the undirect pointer b to avoid handling the special case
 * for removing the head. */
void deleteClientByUsername(char* username) {
	struct ClientBucket **b = &clientHashtable[hash(username, MAX_CLIENTS)];
	while (*b != NULL && strcmp((*b)->key, username) != 0) {
		b = &(*b)->nextInChat;
	}
	if (*b != NULL) {
		*b = (*b)->nextInChat;
	}
}

/* Insert a pair username-client in clientHashtable.
 * There is the same reasoning for the undirect pointer b as in deleteClientByUsername. */
void insertClient(char* username, struct Client* c) {
	struct ClientBucket **b = &clientHashtable[hash(username, MAX_CLIENTS)];
	while (*b != NULL) {
		b = &(*b)->nextInChat;
	}

	struct ClientBucket* newBucket;
	newBucket = malloc(sizeof(newBucket));
	newBucket->key = strdup(username);
	newBucket->value = c;
	*b = newBucket;
}

/* Insert a pair name-channel in channelHashtable.
 * There is the same reasoning for the undirect pointer b as in deleteClientByUsername. */
void insertChannel(char* name, struct Channel* c) {
	struct ChannelBucket **b = &channelHashtable[hash(name, MAX_CHANNELS)];
	while (*b != NULL) {
		b = &(*b)->nextInChat;
	}

	struct ChannelBucket* newBucket;
	newBucket = malloc(sizeof(newBucket));
	newBucket->key = strdup(name);
	newBucket->value = c;
	*b = newBucket;
}

/* Find the client, if present, contained in the bucket whose key is username. */
struct Client* getClientByUsername(char* username) {
	int hashValue = hash(username, MAX_CLIENTS);
	struct ClientBucket* curr = clientHashtable[hashValue];
	while (curr != NULL) {
		if (strcmp(curr->key, username) == 0) {
			/* Client found */
			return curr->value;
		} else {
			curr = curr->nextInChat;
		}
	}
	/* No client with that username has been found. */
	return NULL;
}

/* Find the channel, if present, contained in the bucket whose key is name. */
struct Channel* getChannelByName(char* name) {
	int hashValue = hash(name, MAX_CHANNELS);
	struct ChannelBucket* curr = channelHashtable[hashValue];
	while (curr != NULL) {
		if (strcmp(curr->key, name) == 0) {
			/* Channel found */
			return curr->value;
		} else {
			curr = curr->nextInChat;
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
 * As side effect we update the fds entries and if necessary also head and tail for the chat and the channel. */
void freeClient(struct Client* client) {
	close(fds[client->fdsIndex].fd);
	free(client->username);

	/* chatTail client will occupy the entry in fds where the currently deleting client was at. 
	 * To do it we overwrite the client data in its fds entry with the chatTail data and invalidate the
	 * data present at chatTail index. */
	fds[client->fdsIndex].fd = fds[chatTail->fdsIndex].fd;
	fds[chatTail->fdsIndex].fd = -1;
	fds[chatTail->fdsIndex].events = 0;
	int revents = fds[chatTail->fdsIndex].revents;
	fds[chatTail->fdsIndex].revents = 0;
	chatTail->fdsIndex = client->fdsIndex;
	/* Ignore the returned events of chatTail only if it is the client removed, 
	 * otherwise restore that info for chatTail. */
	fds[chatTail->fdsIndex].revents = chatTail == client ? 0 : revents;

	/* The client that might be the new tail. */
	struct Client *prevTail = chatTail->prevInChat;

	/* Update chatTail next pointer and client next prev paying attention if: 
	 * 1. we remove chatTail (NULL reference)
	 * 2. we remove the client before chatTail (we may create a loop) */
	if (client != chatTail && client != prevTail) {
		chatTail->nextInChat = client->nextInChat;
		client->nextInChat->prevInChat = chatTail;
	}
	chatTail->prevInChat = client->prevInChat;
	if (client == chatHead) {
		/* If chatHead is being removed update chatHead with chatTail only if there is at least another client. */
		chatHead = chatHead == chatTail ? NULL : chatTail;
	} else {
		/* For any other client just update the next pointer of its previous. */
		client->prevInChat->nextInChat = chatTail;
	}
	/* We update chatTail to prevTail only if we're not deleting prevTail, otherwise chatTail is unchanged. */
	if (prevTail != client) {
		chatTail = prevTail;
		if (chatTail != NULL) {
			chatTail->nextInChat = NULL;
		}
	}

	struct Channel* channel = client->channel;
	struct Client* channelHead = channel->head;
	struct Client* channelTail = channel->tail;
	/* The client that might be the new channel tail. */
	prevTail = channelTail->prevInChannel;

	/* Update channelTail next pointer and client next prev paying attention if: 
	 * 1. we remove channelTail (NULL reference)
	 * 2. we remove the client before channelTail (we may create a loop) */
	if (client != channelTail && client != prevTail) {
		channelTail->nextInChannel = client->nextInChannel;
		client->nextInChannel->prevInChannel = channelTail;
	}
	channelTail->prevInChannel = client->prevInChannel;
	if (client == channelHead) {
		/* If channel head is being removed update channel head with channelTail only if there is at least another client. */
		channel->head = channelHead == channelTail ? NULL : channelTail;
	} else {
		/* For any other client just update the next pointer of its previous. */
		client->prevInChannel->nextInChannel = channelTail;
	}
	/* We update channelTail to prevTail only if we're not deleting prevTail, otherwise tail is unchanged. */
	if (prevTail != client) {
		channel->tail = prevTail;
		if (channelTail != NULL) {
			channelTail->nextInChannel = NULL;
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

				if (chatHead == NULL) {
					/* First client inserted. */
					chatHead = client;
				} else {
					/* Append the client to chatTail. */
					chatTail->nextInChat = client;
					client->prevInChat = chatTail;
				}
				chatTail = client;

				/* Update clientHashtable. */
				insertClient(username, client);

				fds[client->fdsIndex].fd = clientFD;
				fds[client->fdsIndex].events = POLLIN;

				numClients++;

				send(clientFD, welcomeMessage, strlen(welcomeMessage), 0);
			}

			for (struct Client *client = chatHead; client != NULL; client = client->nextInChat) {
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

								/* If the username already exists we ignore the command,
								 * otherwise we update the client's username in clientHashtable. */
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
							} else if (strncmp(buffer+1, "join", 4) == 0) {
								/* The user wants to join a channel, the channel's name
								 * is the string after '\join ' whose length is 6. */
								int length = bytesRead - 6;
								char* name = malloc(length);
								memcpy(name, buffer + 6, length);
								name[length - 1] = '\0';

								/* Create the channel if it doesn't exist. */
								struct Channel* channel;
								if ((channel = getChannelByName(name)) == NULL) {
									channel = malloc(sizeof(channel));
									channel->name = name;
									insertChannel(name, channel);
								}

								/* Update the channel state appending the current client. */
								client->channel = channel;
								if (channel->head == NULL) {
									channel->head = client;
								} else {
									channel->tail->nextInChannel = client;
									client->prevInChannel = channel->tail;
								}
								channel->tail = client;
							}
						} else {
							/* The client sent a message, broadcast the message */
							if (client->channel == NULL) {
								/* Ignore the message if the client is in none channel. */
								continue;
							}

							/* Otherwise broadcast the message in that channel. */
							char message[1024];
							int messageLength = snprintf(message, sizeof(message), "%s> %s", client->username, buffer);

							for (struct Client *c = client->channel->head; c != NULL; c = c->nextInChannel) {
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
