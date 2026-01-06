#include "user_manager.h"

#include "utils.h"
#include "client_handler.h"
#include "user_manager.h"

void login(int client_sock, const char *username, int *user_id, const char *password)
{
  char info_file[BUFFER_SIZE];
  snprintf(info_file, sizeof(info_file), "%s/%s/info.txt", BASE_DIR, username);
  // printf("info_file: %s\n", info_file);
  if (access(info_file, F_OK) == -1)
  {
    send_websocket_message(client_sock, "Account does not exist.\n", strlen("Account does not exist.\n"), 0);
  }
  else
  {
    FILE *file = fopen(info_file, "r");
    int stored_id;
    char stored_password[BUFFER_SIZE];
    fscanf(file, "ID:%d\nPASSWORD:%s", &stored_id, stored_password);
    fclose(file);

    if (strcmp(stored_password, password) == 0)
    {
      *user_id = stored_id;
      clients[*user_id].id = stored_id;
      strncpy(clients[*user_id].username, username, BUFFER_SIZE);
      strncpy(clients[*user_id].password, password, BUFFER_SIZE);
      clients[*user_id].is_online = 1;
      clients[*user_id].socket = client_sock;
      char response[BUFFER_SIZE];
      snprintf(response, BUFFER_SIZE, "%d %s", *user_id, username);
      printf("User logged in: ID=%d, socket=%d, is_online=%d\n", clients[*user_id].id, clients[*user_id].socket, clients[*user_id].is_online);
      send_websocket_message(client_sock, response, strlen(response), 0);
    }
    else
    {
      send_websocket_message(client_sock, "Incorrect password.\n", strlen("Incorrect password.\n"), 0);
    }
  }
}

void register_user(int client_sock, const char *username, int *user_id, const char *password)
{
  char user_dir[BUFFER_SIZE];
  snprintf(user_dir, sizeof(user_dir), "%s/%s", BASE_DIR, username);

  if (access(user_dir, F_OK) != -1)
  {
    send_websocket_message(client_sock, "Account already exists.\n", strlen("Account already exists.\n"), 0);
  }
  else
  {
    int new_id = create_user_directory(username, password);

    if (new_id != -1)
    {
      int id = add_client(client_sock, new_id, username, password);
      // printf("%d\n", id);
      *user_id = new_id;
      strncpy(clients[*user_id].username, username, BUFFER_SIZE);
      strncpy(clients[*user_id].password, password, BUFFER_SIZE);
      clients[*user_id].id = new_id;
      clients[*user_id].is_online = 1;
      clients[*user_id].socket = client_sock;
      char response[BUFFER_SIZE];
      snprintf(response, BUFFER_SIZE, "%d", new_id);
      send_websocket_message(client_sock, response, strlen(response), 0);
      printf("%d %d %d %s %s\n", *user_id, clients[*user_id].id, clients[*user_id].is_online, clients[*user_id].username, clients[*user_id].password);
    }
    else
    {
      send_websocket_message(client_sock, "Registration failed.\n", strlen("Registration failed.\n"), 0);
    }
  }
}

int load_next_id()
{
  int next_id;
  FILE *file = fopen(ID_FILE, "r");
  if (file)
  {
    fscanf(file, "%d", &next_id);
    fclose(file);
  }
  else
  {
    next_id = 0;
  }
  return next_id;
}

void save_next_id(int next_id)
{
  FILE *file = fopen(ID_FILE, "w");
  if (file)
  {
    fprintf(file, "%d", next_id);
    fclose(file);
  }
}

int create_user_directory(const char *username, const char *password)
{
  char user_dir[BUFFER_SIZE];
  snprintf(user_dir, sizeof(user_dir), "%s/%s", BASE_DIR, username);

  if (mkdir(user_dir, 0700) == 0)
  {
    int user_id = load_next_id();
    int next_id = user_id + 1;
    save_next_id(next_id);

    char info_file[BUFFER_SIZE];
    snprintf(info_file, sizeof(info_file), "%s/info.txt", user_dir);

    FILE *file = fopen(info_file, "w");
    if (file)
    {
      fprintf(file, "ID:%d\nPASSWORD:%s\n", user_id, password);
      fclose(file);
    }
    file = fopen(U_FILE, "a");
    fprintf(file, "%s\n", username);
    fclose(file);
    return user_id;
  }
  return -1;
}

int load_user_name(Client *clients, int max_clients)
{
  int i = 0, user_id;
  FILE *file = fopen(U_FILE, "r");
  if (file == NULL)
  {
    fprintf(stderr, "Error: Unable to open user file.\n");
    return -1;
  }
  // Don't reset clients array here - it's already initialized in init_clients()
  // Just initialize friend/request arrays for all clients
  for (int j = 0; j < MAX_CLIENTS; j++)
  {
    for (int k = 0; k < MAX_REQUESTS; k++)
    {
      clients[j].add_friend_requests[k] = -1;
    }
    for (int k = 0; k < MAX_FRIENDS; k++)
    {
      clients[j].friends[k] = -1;
    }
    clients[j].friend_count = 0;
    clients[j].request_count = 0;
  }
  char buffer[BUFFER_SIZE];
  while (i < max_clients && fgets(buffer, sizeof(buffer), file) != NULL)
  {
    buffer[strcspn(buffer, "\n")] = '\0';
    char info_file[BUFFER_SIZE];
    snprintf(info_file, sizeof(info_file), "%s/%s/info.txt", BASE_DIR, buffer);
    FILE *inf_file = fopen(info_file, "r");
    if (inf_file == NULL)
    {
      fprintf(stderr, "Error: Unable to open info file for user '%s'.\n", buffer);
      continue;
    }

    char buff[BUFFER_SIZE];
    if (fgets(buff, sizeof(buff), inf_file) != NULL)
    {
      sscanf(buff, "ID:%d", &user_id);
    }
    if (fgets(buff, sizeof(buff), inf_file) != NULL)
    {
      sscanf(buff, "PASSWORD:%s", clients[user_id].password);
    }

    strncpy(clients[user_id].username, buffer, BUFFER_SIZE - 1);
    clients[user_id].username[sizeof(clients[user_id].username) - 1] = '\0';
    clients[user_id].id = user_id;
    fclose(inf_file);
    read_friend_list(&clients[user_id]);
    read_friend_request(&clients[user_id]);
    i++;
  }
  fclose(file);
  return i;
}

void trim_whitespace(char *str)
{
  char *end;

  // Trim leading spaces
  while (isspace((unsigned char)*str))
    str++;

  // If string is all spaces
  if (*str == 0)
    return;

  // Trim trailing spaces
  end = str + strlen(str) - 1;
  while (end > str && isspace((unsigned char)*end))
    end--;

  // Write null terminator
  *(end + 1) = '\0';
}

void read_friend_request(Client *receiver)
{
  char directory_name[BUFFER_SIZE];
  char request_file[BUFFER_SIZE];
  FILE *f_request;

  snprintf(directory_name, sizeof(directory_name), "%s/%s", BASE_DIR, receiver->username);
  snprintf(request_file, sizeof(request_file), "%s/listreq.txt", directory_name);

  f_request = fopen(request_file, "r");
  if (f_request == NULL)
  {
    return;
  }

  receiver->request_count = 0;

  char line[BUFFER_SIZE];
  if (fgets(line, sizeof(line), f_request))
  {
    trim_whitespace(line);
    char *token = strtok(line, " ");
    while (token != NULL && receiver->request_count < MAX_REQUESTS)
    {
      receiver->add_friend_requests[receiver->request_count++] = atoi(token);
      token = strtok(NULL, " ");
    }
  }

  fclose(f_request);
}

void read_friend_list(Client *receiver)
{
  char directory_name[BUFFER_SIZE];
  char friend_file[BUFFER_SIZE];
  FILE *f_friend;

  snprintf(directory_name, sizeof(directory_name), "%s/%s", BASE_DIR, receiver->username);
  snprintf(friend_file, sizeof(friend_file), "%s/friend.txt", directory_name);

  f_friend = fopen(friend_file, "r");
  if (f_friend == NULL)
  {
    return;
  }

  receiver->friend_count = 0;
  char line[BUFFER_SIZE];
  if (fgets(line, sizeof(line), f_friend))
  {
    trim_whitespace(line);
    char *token = strtok(line, " ");
    while (token != NULL && receiver->friend_count < MAX_FRIENDS)
    {
      receiver->friends[receiver->friend_count++] = atoi(token);
      token = strtok(NULL, " ");
    }
  }
  fclose(f_friend);
}
