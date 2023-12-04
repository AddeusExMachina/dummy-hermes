int createServer();

void setListenMode(int serverFD, int port);

int acceptConnection(int serverFD);

int createClient();

void connectToServer(int clientFD, char *ip, int port);

