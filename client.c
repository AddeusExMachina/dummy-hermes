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

#define PORT 50001
#define IP "127.0.0.1"

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
int main() {
	setRawMode();

	/* ================================================= Client socket creation ================================================= *
	 * To instantiate the client socket we rely on:
	 * 1. socket() to create a socket that allows communication between processes on different hosts connected by IPV4
	 * 2. htons() to convert values between host and network byte order to avoid conflicts about endianess format
	 * 3. inet_pton() to convert IPv4 and IPv6 addresses from text to binary form
	 * 4. connect() to initiate the connection to the server
	 * ======================================================================================================================== */
	int clientFD = createClient();
	//connectToServer(clientFD, IP, PORT);
	connectToServer(clientFD, "127.0.0.1", PORT);
	//struct sockaddr_in serverAddress;
	char buffer[1024] = { 0 };
	int stdinFD = fileno(stdin);

	//if ((clientFD = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
	//	perror("Socket creation error");
	//	exit(EXIT_FAILURE);
	//}

	//serverAddress.sin_family = AF_INET;
	//serverAddress.sin_port = htons(PORT);

	//if (inet_pton(AF_INET, IP, &serverAddress.sin_addr) <= 0) {
	//	perror("Invalid address/ Address not supported");
	//	exit(EXIT_FAILURE);
	//}

	//if (connect(clientFD, (struct sockaddr*) &serverAddress, sizeof(serverAddress)) < 0) {
	//	perror("Connection failed");
	//	exit(EXIT_FAILURE);
	//}

	/* Read the welcome message */
	read(clientFD, buffer, sizeof(buffer));
	printf("%s", buffer);
	memset(buffer, 0, sizeof buffer);
	
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
			 * If the last typed character is a new line '\n' we send all the
			 * the buffered data to the server, then we empty the input buffer.
			 * TODO: This is not so nice, a new line character may appear in any position
			 * in the input text. It should be improved. */
			if (fds[1].revents & POLLIN) {
				memset(buffer, 0, sizeof buffer);
				int bytesRead = read(stdinFD, buffer, sizeof(buffer));
				printf("%s", buffer);
    				fflush(stdout);
				memcpy(input + length, buffer, strlen(buffer));
				length += bytesRead;

				if (input[length - 1] == '\n') {
					send(clientFD, input, strlen(input), 0);
					memset(input, 0, sizeof input);
					length = 0;
					printf("you> ");
					fflush(stdout);
				}
			}

			/* If there is a message from the server, in order to display it:
			 * 1. we hide the text entered in input by the user
			 * 2. we show the message on the current line
			 * 3. we re-display the input text on the next line */
			if (fds[0].revents & POLLIN) {
				memset(buffer, 0, sizeof buffer);
				int bytesRead = read(clientFD, buffer, sizeof(buffer));

				if (bytesRead <= 0) {
					printf("Server disconnected. Bye bye\n");
					return 0;
				}

                		/* Save the cursor position */
                		printf("\033[s");
                		/* Move to the beginning of the line */
                		printf("\033[0G");
                		/* Clear the line */
                		printf("\033[K");
				/* Print the message */
				printf("%s\n", buffer);

				/* Reprint the user's input */
				printf("you> %s", input);
				fflush(stdout);

                		/* Restore the cursor position */
                		printf("\033[u");
				/* Move the cursor down by one line */
				printf("\033[B");
			}
		}
	}

	return 0;
}
