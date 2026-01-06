#include "message_handler.h"
#include "common.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <time.h>
#include <dirent.h>

extern Client clients[MAX_CLIENTS];

// Generates a fixed-length ID using a charset and checks for collisions
void generate_fixed_id(char *buffer)
{
  const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  char path[BUFFER_SIZE];
  int unique = 0;

  while (!unique)
  {
    for (size_t i = 0; i < CONVERSATION_ID_LENGTH; ++i)
    {
      buffer[i] = charset[rand() % (sizeof(charset) - 1)];
    }
    buffer[CONVERSATION_ID_LENGTH] = '\0';

    snprintf(path, BUFFER_SIZE, "conversation_data/%s.txt", buffer);
    FILE *file = fopen(path, "r");
    if (file)
    {
      fclose(file);
    }
    else
    {
      unique = 1;
    }
  }
}

// Ensures the directory exists, creates it if it doesn't
void ensure_directory_exists(const char *path)
{
  struct stat st = {0};
  if (stat(path, &st) == -1)
  {
    mkdir(path, 0755);
  }
}

// Retrieves or creates a shared conversation ID between sender and receiver
char *get_or_create_shared_conversation_id(const char *sender_file, const char *receiver_file, char *buffer)
{
  FILE *file = fopen(sender_file, "r");
  if (file)
  {
    if (fgets(buffer, CONVERSATION_ID_LENGTH + 2, file))
    {
      buffer[strcspn(buffer, "\n")] = '\0';
      fclose(file);
      // printf("Sender ID: %s\n", buffer);
      return buffer;
    }
    fclose(file);
  }

  file = fopen(receiver_file, "r");
  if (file)
  {
    if (fgets(buffer, CONVERSATION_ID_LENGTH + 2, file))
    {
      buffer[strcspn(buffer, "\n")] = '\0';
      fclose(file);

      file = fopen(sender_file, "w");
      if (file)
      {
        fprintf(file, "%s\n", buffer);
        fclose(file);
      }
      // printf("Receiver ID: %s\n", buffer);
      return buffer;
    }
    fclose(file);
  }

  // If no ID exists, generate a new one
  generate_fixed_id(buffer);
  file = fopen(sender_file, "w");
  if (file)
  {
    fprintf(file, "%s\n", buffer);
    // printf("Sender file created\n");
    fclose(file);
  }
  file = fopen(receiver_file, "w");
  if (file)
  {
    // printf("Receiver file created\n");
    fprintf(file, "%s\n", buffer);
    fclose(file);
  }

  printf("buffer: %s\n", buffer);
  return buffer;
}

// Stores a message in the conversation file
void store_message(int sender_id, int receiver_id, const char *message)
{
  char sender_folder[BUFFER_SIZE], receiver_folder[BUFFER_SIZE];
  char sender_conversation_file[BUFFER_SIZE], receiver_conversation_file[BUFFER_SIZE];
  char conversation_folder[BUFFER_SIZE] = "conversation_data";
  char conversation_file[BUFFER_SIZE], conversation_id[CONVERSATION_ID_LENGTH + 2] = {0};

  // Ensure user_data/{username}/conversations directories exist
  snprintf(sender_folder, BUFFER_SIZE, "user_data/%s/conversations", clients[sender_id].username);
  snprintf(receiver_folder, BUFFER_SIZE, "user_data/%s/conversations", clients[receiver_id].username);
  ensure_directory_exists(sender_folder);
  ensure_directory_exists(receiver_folder);
  // printf("sender_folder: %s\n", sender_folder);
  // printf("receiver_folder: %s\n", receiver_folder);

  // Paths to individual conversation files
  snprintf(sender_conversation_file, BUFFER_SIZE, "%s/%s.txt", sender_folder, clients[receiver_id].username);
  snprintf(receiver_conversation_file, BUFFER_SIZE, "%s/%s.txt", receiver_folder, clients[sender_id].username);
  // printf("sender_conversation_file: %s\n", sender_conversation_file);
  // printf("receiver_conversation_file: %s\n", receiver_conversation_file);

  pthread_mutex_lock(&clients_mutex);
  get_or_create_shared_conversation_id(sender_conversation_file, receiver_conversation_file, conversation_id);
  pthread_mutex_unlock(&clients_mutex);

  // Ensure conversation_data folder exists
  ensure_directory_exists(conversation_folder);
  snprintf(conversation_file, BUFFER_SIZE, "%s/%s.txt", conversation_folder, conversation_id);
  // printf("conversation_file: %s\n", conversation_file);

  // Append the message to the conversation file
  FILE *file = fopen(conversation_file, "a");
  if (file)
  {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);

    fprintf(file, "[%s] %s: %s\n", timestamp, clients[sender_id].username, message);
    fclose(file);
  }
}

// Request private message
void request_private_message(int sender_id, int receiver_id)
{
  // printf("Requesting private message from %s to %s\n", clients[sender_id].username, clients[receiver_id].username);
  if (sender_id < 0 || sender_id >= MAX_CLIENTS || receiver_id < 0 || receiver_id >= MAX_CLIENTS)
  {
    // printf("Invalid sender or receiver ID\n");
    const char *response = "Invalid sender or receiver ID";
    send_websocket_message(clients[sender_id].socket, response, strlen(response), 0);
    return;
  }
  if (clients[sender_id].chatting_partner_id >= 0)
  {
    // printf("You are already in a private chat with %s\n", clients[clients[sender_id].chatting_partner_id].username);
    char response_with_username[BUFFER_SIZE];
    snprintf(response_with_username, BUFFER_SIZE, "You are already in a private chat with %s", clients[clients[sender_id].chatting_partner_id].username);
    char alert_response[BUFFER_SIZE];
    snprintf(alert_response, BUFFER_SIZE, "%d %s", STATUS_ERROR, response_with_username);
    send_websocket_message(clients[sender_id].socket, alert_response, strlen(alert_response), 0);
    return;
  }
  else if (clients[receiver_id].chatting_partner_id >= 0)
  {
    // printf("%s is already in a private chat\n", clients[receiver_id].username);
    char response_with_username[BUFFER_SIZE];
    snprintf(response_with_username, BUFFER_SIZE, "%s is already in a private chat", clients[receiver_id].username);
    char alert_response[BUFFER_SIZE];
    snprintf(alert_response, BUFFER_SIZE, "%d %s", STATUS_ERROR, response_with_username);
    send_websocket_message(clients[sender_id].socket, alert_response, strlen(alert_response), 0);
    return;
  }
  else
  {
    // printf("Requesting private message from %s to %s\n", clients[sender_id].username, clients[receiver_id].username);
    char message_buffer[BUFFER_SIZE];
    char final_message[BUFFER_SIZE];
    snprintf(message_buffer, BUFFER_SIZE, "%d:%s", clients[sender_id].id, clients[sender_id].username);
    add_response_header(final_message, RESPONSE_CHAT_REQUEST, message_buffer, strlen(message_buffer));
    send_websocket_message(clients[receiver_id].socket, final_message, strlen(final_message), 0);
  }
}

void check_chat_partnership(int sender_id, int receiver_id)
{
  char final_response[BUFFER_SIZE];
  if (clients[sender_id].chatting_partner_id == receiver_id && clients[receiver_id].chatting_partner_id == sender_id)
  {
    // printf("You are already in a private chat with %s\n", clients[receiver_id].username);
    const char *response = "true";
    add_response_header(final_response, CHECK_PARTNERSHIP, response, strlen(response));
    send_websocket_message(clients[sender_id].socket, final_response, strlen(final_response), 0);
  }
  else
  {
    // printf("You are not in a private chat with %s\n", clients[receiver_id].username);
    const char *response = "false";
    add_response_header(final_response, CHECK_PARTNERSHIP, response, strlen(response));
    send_websocket_message(clients[sender_id].socket, final_response, strlen(final_response), 0);
  }
}

void accept_chat_request(int sender_id, int receiver_id)
{
  clients[sender_id].chatting_partner_id = receiver_id;
  clients[receiver_id].chatting_partner_id = sender_id;
  char final_response[BUFFER_SIZE];
  const char *response = "true";
  add_response_header(final_response, CHECK_PARTNERSHIP, response, strlen(response));
  send_websocket_message(clients[sender_id].socket, final_response, strlen(final_response), 0);
  send_websocket_message(clients[receiver_id].socket, final_response, strlen(final_response), 0);
}

void disconnect_chat(int sender_id, int receiver_id)
{
  clients[sender_id].chatting_partner_id = -1;
  clients[receiver_id].chatting_partner_id = -1;
  char final_response[BUFFER_SIZE];
  const char *response = "false";
  add_response_header(final_response, CHECK_PARTNERSHIP, response, strlen(response));
  send_websocket_message(clients[sender_id].socket, final_response, strlen(final_response), 0);
  send_websocket_message(clients[receiver_id].socket, final_response, strlen(final_response), 0);
  char alert_response[BUFFER_SIZE];
  snprintf(alert_response, BUFFER_SIZE, "%d\n%s has disconnected from the chat", STATUS_ERROR, clients[sender_id].username);
  send_websocket_message(clients[receiver_id].socket, alert_response, strlen(alert_response), 0);
  char message_buffer[BUFFER_SIZE];
  snprintf(message_buffer, BUFFER_SIZE, "%s has disconnected from the chat", clients[sender_id].username);
  store_message(sender_id, receiver_id, message_buffer);
  log_file("User %s disconnect from chat with %s", clients[sender_id].username, clients[receiver_id].username);
}

// Sends a private message to the receiver if they are online, otherwise stores it
void send_private_message(int sender_id, int receiver_id, const char *message)
{
  if (!message)
  {
    const char *response = "No message to send";
    send_websocket_message(clients[sender_id].socket, response, strlen(response), 0);
    return;
  }

  if (sender_id < 0 || sender_id >= MAX_CLIENTS || receiver_id < 0 || receiver_id >= MAX_CLIENTS)
  {
    const char *response = "Invalid sender or receiver ID";
    send_websocket_message(clients[sender_id].socket, response, strlen(response), 0);
    return;
  }

  char message_buffer[BUFFER_SIZE];
  char final_response[BUFFER_SIZE];
  snprintf(message_buffer, BUFFER_SIZE, "%s:%s", clients[sender_id].username, message);
  if (clients[receiver_id].is_online && clients[receiver_id].socket != -1)
  {
    if (sender_id != receiver_id)
    {
      add_response_header(final_response, RESPONSE_CHAT, message_buffer, strlen(message_buffer));
      send_websocket_message(clients[receiver_id].socket, final_response, strlen(final_response), 0);
      log_file("User %s sent private message to %s", clients[sender_id].username, clients[receiver_id].username);
      store_message(sender_id, receiver_id, message);
      log_message("Stored private message from %s to %s", clients[sender_id].username, clients[receiver_id].username);
    }
  }
  else
  {
    char response[BUFFER_SIZE];
    snprintf(response, BUFFER_SIZE, "User %s is not online.", clients[receiver_id].username);
    send_websocket_message(clients[sender_id].socket, response, strlen(response), 0);
  }
}

// Sends an offline message and stores it
void send_offline_message(int sender_id, int receiver_id, const char *message)
{
  if (!message)
  {
    const char *response = "No message to send";
    send_websocket_message(clients[sender_id].socket, response, strlen(response), 0);
    return;
  }

  if (sender_id < 0 || sender_id >= MAX_CLIENTS || receiver_id < 0 || receiver_id >= MAX_CLIENTS)
  {
    const char *response = "Invalid sender or receiver ID";
    send_websocket_message(clients[sender_id].socket, response, strlen(response), 0);
    return;
  }

  char message_buffer[BUFFER_SIZE];
  char final_response[BUFFER_SIZE];
  snprintf(message_buffer, BUFFER_SIZE, "%s: %s", clients[sender_id].username, message);
  log_file("User %s sent offline message to %s", clients[sender_id].username, clients[receiver_id].username);

  store_message(sender_id, receiver_id, message);
  log_message("Stored offline message from %s to %s", clients[sender_id].username, clients[receiver_id].username);

  snprintf(final_response, BUFFER_SIZE, "%d", RESPONSE_CHAT_OFFLINE);
  send_websocket_message(clients[sender_id].socket, final_response, strlen(final_response), 0);
}

// Retrieves messages from the conversation file
void retrieve_message(int client_sock, int sender_id, int receiver_id)
{
  if (sender_id < 0 || sender_id >= MAX_CLIENTS || receiver_id < 0 || receiver_id >= MAX_CLIENTS)
  {
    const char *response = "Invalid sender or receiver ID";
    send_websocket_message(clients[sender_id].socket, response, strlen(response), 0);
    return;
  }

  char sender_conversation_file[BUFFER_SIZE];
  char conversation_id[CONVERSATION_ID_LENGTH + 2] = {0};
  MessageList *message_list = malloc(sizeof(MessageList));
  message_list->count = 0;

  char line[BUFFER_SIZE];
  char *response_content = malloc(RESPONSE_SIZE);
  size_t response_size = 0;

  snprintf(sender_conversation_file, BUFFER_SIZE, "user_data/%s/conversations/%s.txt",
           clients[sender_id].username, clients[receiver_id].username);

  printf("Sender conversation file: %s\n", sender_conversation_file);

  FILE *file = fopen(sender_conversation_file, "r");
  if (file == NULL)
  {
    printf("Error opening file: %s\n", sender_conversation_file);
    response_content[0] = '\0';
    free(message_list);
  }
  else
  {
    if (fgets(conversation_id, CONVERSATION_ID_LENGTH + 2, file))
    {
      conversation_id[strcspn(conversation_id, "\n")] = '\0';
    }
    fclose(file);

    char conversation_file[BUFFER_SIZE];
    snprintf(conversation_file, BUFFER_SIZE, "conversation_data/%s.txt", conversation_id);

    file = fopen(conversation_file, "r");
    if (file == NULL)
    {
      response_content[0] = '\0';
      free(message_list);
      return;
    }
    while (fgets(line, sizeof(line), file) && message_list->count < MAX_MESSAGES)
    {
      Message *msg = &message_list->messages[message_list->count++];
      if (sscanf(line, "[%19[^]]] %[^:]: %[^\n]", msg->timestamp, msg->username, msg->message) == 3)
      {
        // Valid line parsed
      }
      else
      {
        --message_list->count; // Revert count if line is invalid
      }
    }
    fclose(file);

    for (int i = 0; i < message_list->count; i++)
    {
      Message *msg = &message_list->messages[i];
      int written = snprintf(response_content + response_size, RESPONSE_SIZE - response_size,
                             "[%s] %s: %s\n", msg->timestamp, msg->username, msg->message);
      if (written < 0 || written >= RESPONSE_SIZE - response_size)
      {
        break;
      }
      response_size += written;
    }
  }

  char *final_response = malloc(RESPONSE_SIZE);

  add_response_header(final_response, RESPONSE_RETRIEVE, response_content, response_size);
  send_websocket_message(client_sock, final_response, strlen(final_response), 0);

  free(response_content);
  free(final_response);
}
