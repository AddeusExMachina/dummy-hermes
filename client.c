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
#include <termios.h>
#include <unistd.h>

#include "socketlib.h"

/* Set the terminal to raw mode.
 * Using raw mode we can avoid that text entered by user and text from incoming messages collide. */
void setRawMode() {
    struct termios newTermios;
    tcgetattr(STDIN_FILENO, &newTermios);
    newTermios.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newTermios);
}

/* In main() first we create a client socket to connect to the server, then
 * we continously listen for messages from other clients and for console input */
int main(int argc, char **argv) {
	if (argc < 3) {
		printf("Please specify server ip and port\n");
		exit(EXIT_FAILURE);
	}

	setRawMode();

	int clientFD = createClient();
	char *ip = argv[1];
	int port = atoi(argv[2]);
	connectToServer(clientFD, ip, port);

	char buffer[1024];
	int stdinFD = fileno(stdin);

	/* The set of file descriptors used to check incoming data is made up of:
	 * 1. the client file descriptor for data from the server, that is messages from other clients
	 * 2. the input console where the user types the message */
	struct pollfd fds[2];
	int nfds = 2;
	fds[0].fd = clientFD;
	fds[0].events = POLLIN;
	fds[1].fd = stdinFD;
	fds[1].events = POLLIN;

	/* Text typed by the user */
	char input[1024] = {0};
	int length = 0;

	printf("you> ");
	fflush(stdout);

	int timeout = 10000;
	while (1) {
		/* We wait for events */
		int numEvents = poll(fds, nfds, timeout);
		if (numEvents > 0) {
			/* When a user types on console we buffer the entered text.
			 * If a character is the new line '\n' we send all the
			 * the buffered data to the server, then we empty the input buffer. */
			if (fds[1].revents & POLLIN) {
				memset(buffer, 0, sizeof buffer);
				int bytesRead = read(stdinFD, buffer, sizeof(buffer));

				for (int i = 0; i < bytesRead; i++) {
					printf("%c", buffer[i]);
					fflush(stdout);
					input[length++] = buffer[i];

					if (buffer[i] == '\n') {
						send(clientFD, input, strlen(input), 0);
						if (strcmp(input, "\\exit\n") == 0) {
							printf("Bye bye\n");
							return 0;
						}
						memset(input, 0, sizeof input);
						length = 0;
						printf("you> ");
						fflush(stdout);
					}
				}

			}

			/* If there is a message from the server, in order to display it:
			 * 1. we hide the text entered in input by the user
			 * 2. we show the message on the current line
			 * 3. we re-display the input text on the next line
			 * Note that when the text typed by the user is on multiple lines and it receives a message, only the last line is hidden. */
			if (fds[0].revents & POLLIN) {
				memset(buffer, 0, sizeof buffer);
				int bytesRead = read(clientFD, buffer, sizeof(buffer));

				if (bytesRead <= 0) {
					printf("Server disconnected. Bye bye\n");
					return 0;
				}

                		/* Move to the beginning of the line */
                		printf("\033[0G");
                		/* Clear the line */
                		printf("\033[K");

				printf("%s", buffer);
				printf("you> %s", input);
				fflush(stdout);
			}
		}
	}

	return 0;
}
