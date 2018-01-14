/**
 * Thread argument struct for passing to worker_thread()
 */
typedef struct _thread_data_t {
  int tid;
  int available;
} thread_data_t;

/**
 * Socket queue
 */
typedef struct socket_queue_t {
  int head;
  int tail;
  int size;
  int clients[MAX_CONNECTIONS];
} socket_queue_t;
socket_queue_t socket_queue;
pthread_mutex_t lock_sq;
pthread_cond_t append_queue;
pthread_mutex_t append_queue_mx;

/**
 * Add a socket to the queue
 * @param client Client socket
 */
void enqueue_socket(int client) {
  pthread_cond_broadcast(&append_queue);
  pthread_mutex_lock(&lock_sq);
  if (socket_queue.size != MAX_CONNECTIONS) {
    if (socket_queue.tail == MAX_CONNECTIONS-1) {
       socket_queue.tail = -1;
    }
    socket_queue.clients[(int)++socket_queue.tail] = client;
    socket_queue.size++;
  }
  pthread_mutex_unlock(&lock_sq);
}

/**
 * Dequeue the first socket
 */
int dequeue_socket() {
  pthread_mutex_lock(&lock_sq);
  if (socket_queue.size == 0) {
    pthread_mutex_unlock(&lock_sq);
    return 0;
  }
  int client = socket_queue.clients[(int)socket_queue.head++];
  if (client > 0) {
    if (socket_queue.head == MAX_CONNECTIONS) {
      socket_queue.head = 0;
    }
    socket_queue.size--;
  }
  pthread_mutex_unlock(&lock_sq);
  return client;
}

/**
 * Remove a socket from the queue and serve its request
 * @param  arg [description]
 * @return     [description]
 */
void *worker_thread(void *arg) {
  thread_data_t *thread = (thread_data_t *)arg;
  while(1) {
    // Look for client sockets to dequeue
    int client = dequeue_socket();
    if (client > 0) {
      // Client socket is now out of queue, serve them
      thread->available = 0;

      printf(
        "Serving client %i via thread #%d\n",
        client,
        thread->tid
      );

      /**
       * Serve the request
       * @see handle_request.h
       */
      handle_request(client);

      thread->available = 1;
    } else {
      // Queue was empty, block until something gets added
      pthread_cond_wait(&append_queue, &append_queue_mx);
    }

    usleep(ITER_SLEEP_TIME);
  }
  pthread_exit(NULL);
}
