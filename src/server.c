#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#define INDEX_FILE "index.html"
#define BUFFER_SIZE 1024
#define MAX_HEADERS 255
#define MAX_CONNECTIONS 200
#define MAX_HEADER_KEY_LEN 255
#define MAX_HEADER_VALUE_LEN 4096
#define FILE_READ_BUFFER 1024
#define MAX_METHOD_LEN 32
#define MAX_URI_LEN 4096
#define MAX_PROTOCOL_LEN 32
#define ITER_SLEEP_TIME 2000

int server, client, SERVER_PORT, NUM_THREADS;
char SERVER_ROOT[4096];

#include "start_server.h"
#include "handle_request.h"
#include "thread_pool.h"

int main(int argc, char const *argv[]) {

  /**
   *  Check argument count
   */
  if (argc < 3) {
    puts("Usage: [thread count] [port] [directory]");
    return 0;
  }

  NUM_THREADS = atoi(argv[1]);
  SERVER_PORT = atoi(argv[2]);
  strcpy(SERVER_ROOT, argv[3]);

  /**
   * Initialize mutex that blocks for socket_queue struct
   */
  pthread_mutex_init(&lock_sq, NULL);

  /**
   * Build mime type data
   * @see mime_types.h
   */
  assemble_mime_types();

  /**
   *  Create worker threads
   */
  thread_data_t thread_data[NUM_THREADS];
  pthread_t thread[NUM_THREADS];
  int i, rc;
  for (i = 0; i < NUM_THREADS; ++i) {
    thread_data[i].tid = i;
    if ((rc = pthread_create(&thread[i], NULL, worker_thread, &thread_data[i]))) {
      fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
      return EXIT_FAILURE;
    }
  }

  /**
   *  Start server and listen forever
   */
  int server, client;
  server = start_server(SERVER_PORT, "/");
  while (1) {
    struct sockaddr_in client_addr;

    /**
     *  Wait for an incoming request
     */
    socklen_t namelen = sizeof(client_addr);
    if ((client = accept(server, (struct sockaddr *)&client_addr, &namelen)) < 0) {
      perror("Could not make socket listen, aborting");
      continue;
    }

    /**
     * Someone connected, add client socket to the socket queue
     * @see thread_pool.h
     */
    enqueue_socket(client);
  }

  /**
   *  Wait for all threads to finish
   */
  for (i = 0; i < NUM_THREADS; ++i) {
    pthread_join(thread[i], NULL);
  }

  /**
   * Close the socket
   */
  shutdown(client, SHUT_RDWR);

  return EXIT_SUCCESS;
}
