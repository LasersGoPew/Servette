void serve_file(int client, char file_path[], struct stat *file_info) {
  /**
   *  Setup file handling
   */
  char buf[FILE_READ_BUFFER];
  memset(buf, 0, FILE_READ_BUFFER);
  FILE *file;
  size_t nread;

  /**
   *  Get MIME type
   *  @see mime_types.h
   */
  char mime_type[255], file_ext[32];
  memset(mime_type, 0, 255);
  memset(file_ext, 0, 32);
  if (strchr(file_path, '.') != NULL) {
    strcpy(file_ext, strrchr(file_path, '.'));
  }
  strcpy(mime_type, ext_to_mime_type(file_ext));

  long long unsigned int fs_int = file_info->st_size;
  char file_size[32];
  snprintf(file_size, 32, "%llu", fs_int);

  send_http_status(client, 200);
  send_http_header(client, "Content-Length", file_size);
  send_http_header(client, "Content-Type", mime_type);
  send_http_header(client, "Connection", "close");
  write(client, "\r\n", 2);

  if (file_size == 0) {
    /**
     * Don't bother sending them nothing
     */
    close_client_socket(client);
    return;
  }

  /**
   * Send response body
   */
  file = fopen(file_path, "r");
  if (file) {
    while ((nread = fread(buf, 1, sizeof buf, file)) > 0) {
      write(client, buf, sizeof(buf));
      memset(buf, 0, FILE_READ_BUFFER);
    }

    if (ferror(file)) {
      /* deal with error */
      puts("error!");
    }

    fclose(file);
  }else{
    puts("File not found");
  }


}
