int createServer(int port);

int acceptConnection(int serverFD);

int createClient();

void connectToServer(int clientFD, char *ip, int port);

