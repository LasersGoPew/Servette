#include <dirent.h>
#include <inttypes.h>

#define DIM(x) (sizeof(x)/sizeof(*(x)))

char * format_bytes(uint64_t size) {
  const char     *sizes[]   = { "EiB", "PiB", "TiB", "GiB", "MiB", "KiB", "B" };
  const uint64_t  exbibytes = 1024ULL * 1024ULL * 1024ULL *
                              1024ULL * 1024ULL * 1024ULL;
  char *result = (char *) malloc(sizeof(char) * 20);
  uint64_t multiplier = exbibytes;
  int i;
  for (i = 0; i < DIM(sizes); i++, multiplier /= 1024) {
    if (size < multiplier) continue;
    if (size % multiplier == 0) {
      sprintf(result, "%" PRIu64 " %s", size / multiplier, sizes[i]);
    }else{
      sprintf(result, "%.1f %s", (float) size / multiplier, sizes[i]);
    }
    return result;
  }
  strcpy(result, "0 B");
  return result;
}

void get_html_file_list(int client, char file_path[]) {
  DIR *dir;
  struct dirent *ent;

  char *table_open = "<table><tr>"
  "<td width=\"50\">TYPE</td>"
  "<td width=\"75\">SIZE</td>"
  "<td>FILE</td></tr>";

  write(client, table_open, strlen(table_open));

  if ((dir = opendir(file_path)) != NULL) {
    struct stat *stat_result;
    char line_buffer[4096], full_path[4096], link_path[4096], resource_type[5];
    char file_size[32];

    while ((ent = readdir(dir)) != NULL) {

      memset(line_buffer, 0, sizeof(line_buffer));
      memset(full_path, 0, sizeof(full_path));
      memset(link_path, 0, sizeof(link_path));
      memset(file_size, 0, sizeof(file_size));
      memset(resource_type, 0, sizeof(resource_type));

      strcat(full_path, file_path);
      if (full_path[strlen(full_path)-1] != '/') strcat(full_path, "/");
      strcat(full_path, ent->d_name);

      stat_result = malloc(sizeof(struct stat));
      if (stat(full_path, stat_result) < 0) {
        send_http_error(client, 500);
        return;
      }

      // Link path is full_path - SERVER_ROOT
      strncpy(link_path, full_path+strlen(SERVER_ROOT), strlen(full_path));
      link_path[strlen(full_path)] = '\0';

      if (S_ISREG(stat_result->st_mode)) {
        strcpy(file_size, format_bytes(stat_result->st_size));
        strcpy(resource_type, "FILE");
      }else{
        strcpy(file_size, "----");
        strcpy(resource_type, "DIR");
        // add trailing forward slash to link
        strcat(link_path, "/");
      }

      sprintf(
        line_buffer,
        "<tr>"
        "<td>%s</td>"
        "<td>%s</td>"
        "<td><a href=\"%s\">%s</a></td>"
        "</tr>",
        resource_type,
        file_size,
        link_path,
        ent->d_name
      );

      write(client, line_buffer, strlen(line_buffer));

    }

    closedir(dir);
    free(stat_result);
  } else {
    // could not open directory
    send_http_error(client, 403);
    return;
  }

  write(client, "</table>", strlen("</table>"));
}

void serve_directory(int client, char file_path[]) {

  send_http_status(client, 200);
  send_http_header(client, "Content-Type", "text/html");
  send_http_header(client, "Connection", "close");
  write(client, "\r\n", 2);

  get_html_file_list(client, file_path);
}
