int start_server(int port, char path[]) {
  int server;
  struct sockaddr_in serv_addr;

  puts("Starting server...");

  /**
   *  Fill the sockaddr_in struct
   */
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family      = PF_INET;
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  serv_addr.sin_port        = htons(SERVER_PORT);

  /**
   *  Create a socket
   */
  if ((server = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
    errno = 13;
    perror("Could not create socket");
    exit(EXIT_FAILURE);
  }

  /**
   *  Assign socket address
   */
  if (bind(server, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
    errno = 13;
    perror("Could not bind to socket");
    exit(EXIT_FAILURE);
  }

  /**
   * Fill in local address
   */
  socklen_t sa_len = sizeof(struct sockaddr_in);
  getsockname(
    server,
    (struct sockaddr*) &serv_addr,
    &sa_len
  );

  puts("Socket created");

  /**
   * Begin listening, allow up to 1024 pending connections
   */
  if (listen(server, 1024) < 0) {
    errno = 13;
    perror("Could not make socket listen");
    exit(EXIT_FAILURE);
  }

  printf("Listening on port %i\n", ntohs(serv_addr.sin_port));

  /**
   *  Make the address reusable
   */
  if (setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) {
    errno = 13;
    perror("Could not make address reusable");
    exit(EXIT_FAILURE);
  }

  /**
   *  Make the port reusable
   */
  if (setsockopt(server, SOL_SOCKET, SO_REUSEPORT, &(int){1}, sizeof(int)) < 0) {
    errno = 13;
    perror("Could not make port reusable");
    exit(EXIT_FAILURE);
  }

  /**
   *  Return socket number
   */
  return server;
}
