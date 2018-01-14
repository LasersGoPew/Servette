#include <sys/stat.h>

typedef struct {
  char *key;
  char *value;
} header[MAX_HEADERS];

typedef struct {
  int well_formed;
  char *method;
  char *uri;
  char *path;
  char *querystring;
  char *protocol;
  header *headers;
} request;

request parse_headers(char header_str[], int *header_count);
void send_http_error(int socket, int code);
void close_client_socket(int socket);
void send_http_status(int socket, int code);
void send_http_header(int socket, char key[], char value[]);
int method_supported(char method[]);
void log_request(char ip[], int port, char method[], char uri[]);
void handle_request(const int client);
void url_decode(char *str);
int dir_has_index(char path[]);

#include "get_status_message.h"
#include "mime_types.h"
#include "serve_file.h"
#include "serve_directory.h"

/**
 * Parse an HTTP header string into a request struct
 * @param  header_str   Raw HTTP header string
 * @param  header_count Number of headers
 * @return struct request
 */
request parse_headers(char header_str[], int *header_count) {
  int well_formed = 1;
  char ending[5];
  strcpy(ending, "\r\n");

  /**
   * Count number of headers, -1 to subtract first line
   */
  int hc = -1;
  const char *ptr, *last = NULL;
  for (ptr = header_str; (last = strstr(ptr, ending)); ptr = last+1) {
    hc++;
  }
  *header_count = hc;

  /**
   *  This means no complete lines were sent
   *  Consider malformed and abort to avoid segfault
   */
  if (hc <= 0) {
    request rq = {0};
    return rq;
  }

  /**
   *  Split raw headers into lines
   */
  char *line = strtok(header_str, ending);

  /**
   *  Parse first line: [METHOD] [URI] [PROTOCOL]
   */
  char method[MAX_METHOD_LEN], uri[MAX_URI_LEN], protocol[MAX_PROTOCOL_LEN];
  memset(method, 0, MAX_METHOD_LEN);
  memset(uri, 0, MAX_URI_LEN);
  memset(protocol, 0, MAX_PROTOCOL_LEN);
  sscanf(line, "%s %s %s", method, uri, protocol);

  if (strlen(method) < 1)               well_formed = 0;
  if (strlen(uri) < 1 || uri[0] != '/') well_formed = 0;
  if (strlen(protocol) < 1)             well_formed = 0;

  /**
   * Parse headers into header structs
   */
  header headers[hc];
  char format_str[32];
  sprintf(format_str, "%s[^:]: %s%ic", "%", "%", MAX_HEADER_VALUE_LEN);
  char key[MAX_HEADER_KEY_LEN], value[MAX_HEADER_VALUE_LEN];
  int j = 0;
  do {
    line = strtok(NULL, ending);
    if (line != NULL) {
      memset(&key, 0, MAX_HEADER_KEY_LEN);
      memset(&value, 0, MAX_HEADER_VALUE_LEN);
      sscanf(line, format_str, key, value);
      headers[j]->key = strdup(key);
      headers[j]->value = strdup(value);
      j++;
    }
  } while (line != NULL && j < hc);

  /**
   *  Split path and querystring
   */
  char path[MAX_URI_LEN], querystring[MAX_URI_LEN];
  char format_string[255];
  memset(path, 0, MAX_URI_LEN);
  memset(querystring, 0, MAX_URI_LEN);
  memset(format_string, 0, 255);
  if (strstr(uri, "?") != NULL) {
    sprintf(format_string, "%%%d[^?]%%%d[^\r\n]", MAX_URI_LEN, MAX_URI_LEN);
    sscanf(uri, format_string, path, querystring);
  }else{
    sprintf(format_string, "%%%d[^?\r\n]", MAX_URI_LEN);
    sscanf(uri, format_string, path);
  }

  url_decode(path);

  request rq = {
    well_formed,
    method,
    uri,
    path,
    querystring,
    protocol,
    headers
  };

  return rq;
}

/**
 * Decode URL-encoded characters
 * @param str String to decode
 */
void url_decode(char *str) {
  unsigned int i;
  char tmp[BUFSIZ];
  char *ptr = tmp;
  memset(tmp, 0, sizeof(tmp));
  for (i=0; i < strlen(str); i++) {
    if (str[i] != '%') {
      *ptr++ = str[i];
      continue;
    }
    if (!isdigit(str[i+1]) || !isdigit(str[i+2])) {
      *ptr++ = str[i];
      continue;
    }
    *ptr++ = ((str[i+1] - '0') << 4) | (str[i+2] - '0');
    i += 2;
  }
  *ptr = '\0';
  strcpy(str, tmp);
}

/**
 * Close the client socket if it's still open
 * @param socket Client socket
 */
void close_client_socket(int socket) {
  int val;
  socklen_t len = sizeof(val);
  // Try writing to it first...
  if (getsockopt(socket, SOL_SOCKET, SO_ACCEPTCONN, &val, &len) != -1){
    if (close(socket) < 0) {
      perror("Could not close client socket");
    }
  }
}

/**
 * Send the initial HTTP status message
 * @param socket Client socket
 * @param code   HTTP status code
 * @see get_status_message.h
 */
void send_http_status(int socket, int code) {
  const char *message = get_status_message(code);
  char out[255];
  sprintf(out, "HTTP/1.1 %d %s\r\n", code, message);
  write(socket, out, strlen(out));
}

/**
 * Send a single HTTP header
 * @param socket Client socket
 * @param key    Header name
 * @param value  Header value
 */
void send_http_header(int socket, char key[], char value[]) {
  char out[strlen(key)+strlen(value)+32];
  sprintf(out, "%s: %s\r\n", key, value);
  write(socket, out, strlen(out));
}

/**
 * Send an HTTP error header with body message and close connection
 * @param socket Client socket
 * @param code   HTTP status code
 */
void send_http_error(int socket, int code) {
  const char *message = get_status_message(code);
  char header[255], output[1024];
  sprintf(header, "HTTP/1.1 %d %s\r\n", code, message);
  write(socket, header, strlen(header));
  send_http_header(socket, "Content-Type", "text/html");
  send_http_header(client, "Connection", "close");
  write(socket, "\r\n\r\n", 4);
  sprintf(output,
    "<h1>HTTP %i: %s</h1><br>",
    code,
    message
  );
  write(socket, output, strlen(output));
  close_client_socket(socket);
}

/**
 * Is request method supported?
 * @param  method Request method
 */
int method_supported(char method[]) {
  char str[strlen(method)];
  strcpy(str, method);
  for(int i = 0; method[i]; i++) str[i] = toupper(method[i]);
  if (strcmp(str, "GET") == 0)     return 1;
  if (strcmp(str, "POST") == 0)    return 0;
  if (strcmp(str, "HEAD") == 0)    return 0;
  if (strcmp(str, "PUT") == 0)     return 0;
  if (strcmp(str, "DELETE") == 0)  return 0;
  if (strcmp(str, "OPTIONS") == 0) return 0;
  if (strcmp(str, "CONNECT") == 0) return 0;
  return 0;
}

/**
 * Check if a given directory has a file matching INDEX_FILE
 * @param  path Path to check
 */
int dir_has_index(char path[]) {
  struct stat stat_result;
  char index_path[strlen(path) + 1 + sizeof(INDEX_FILE)];
  memset(index_path, 0, sizeof(index_path));
  strcat(index_path, path);
  if (index_path[strlen(index_path)-1] != '/') strcat(index_path, "/");
  strcat(index_path, INDEX_FILE);
  if (stat(index_path, &stat_result) > -1) {
    return 1;
  }
  return 0;
}

/**
 * Print request into to the terminal
 * @param ip     Client IP
 * @param method HTTP method
 * @param uri    Requested URI
 */
void log_request(char ip[], int port, char method[], char uri[]) {
  printf("Request [%s:%i] %s %s\n", ip, port, method, uri);
}

/**
 * Handle the client's request
 * @param client      Client socket
 */
void handle_request(const int client) {

  char client_buffer[BUFFER_SIZE]; // Stores the request headers
  memset(client_buffer, 0, sizeof(client_buffer));

  /**
   *  Read headers and determine what they want
   */
  memset(client_buffer, 0, sizeof(client_buffer));
  read(client, client_buffer, BUFFER_SIZE);
  int header_count = 0;
  request rq = parse_headers(client_buffer, &header_count);

  /**
   * Get client IP and port
   */
  socklen_t len;
  struct sockaddr_storage addr;
  char client_ip[INET6_ADDRSTRLEN];
  int client_port;
  len = sizeof addr;
  getpeername(client, (struct sockaddr*)&addr, &len);
  if (addr.ss_family == AF_INET) {
    // IPv4
    struct sockaddr_in *s = (struct sockaddr_in *)&addr;
    client_port = ntohs(s->sin_port);
    inet_ntop(AF_INET, &s->sin_addr, client_ip, sizeof client_ip);
  } else {
    // IPv6
    struct sockaddr_in6 *s = (struct sockaddr_in6 *)&addr;
    client_port = ntohs(s->sin6_port);
    inet_ntop(AF_INET6, &s->sin6_addr, client_ip, sizeof client_ip);
  }

  /**
   *  Print the basic request info
   */
  log_request(client_ip, client_port, rq.method, rq.uri);

  /**
   *  Is request malformed?
   */
  if (rq.well_formed != 1) {
    // Send HTTP 400 Bad Request
    send_http_error(client, 400);
    return;
  }

  /**
   * Is request method supported?
   */
  if (method_supported(rq.method) != 1) {
    // Send HTTP 405 Method Not Supported
    send_http_error(client, 405);
    return;
  }

  /**
   *  Build file path
   */
  char file_path[sizeof(rq.path) + sizeof(SERVER_ROOT)];
  memset(file_path, 0, sizeof(file_path));
  strcat(file_path, SERVER_ROOT);
  strcat(file_path, rq.path);

  /**
   *  Stat path to determine what we're working with
   */
  struct stat *file_info;
  file_info = malloc(sizeof(struct stat));
  if (stat(file_path, file_info) < 0) {
    send_http_error(client, 404);
    return;
  }

  if (S_ISREG(file_info->st_mode)) {

    /**
     *  If it's a regular file, serve it
     *  @see serve_file.h
     */
    serve_file(client, file_path, file_info);

  }else{

    /**
     *  If it's a directory with an index.html, serve that instead
     *  @see serve_file.h
     */
    if (dir_has_index(file_path)) {
      char new_file_path[sizeof(file_path) + sizeof(INDEX_FILE) + 1];
      memset(new_file_path, 0, sizeof(new_file_path));

      strcat(new_file_path, file_path);
      if (new_file_path[strlen(new_file_path)-1] != '/') strcat(new_file_path, "/");
      strcat(new_file_path, INDEX_FILE);

      file_info = malloc(sizeof(struct stat));
      if (stat(new_file_path, file_info) < 0) {
        printf("Failed reading: %s\n", new_file_path);
        send_http_error(client, 500);
        return;
      }

      printf("Serving index file: %s\n", INDEX_FILE);
      serve_file(client, new_file_path, file_info);
    }

    /**
     *  If it's a directory with no index, build directory view
     *  @see serve_directory.h
     */
     serve_directory(client, file_path);
  }

  free(file_info);

  /**
   *  Close the client socket (if it's still open)
   */
  close_client_socket(client);
}
