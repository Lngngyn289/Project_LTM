#include "client_handler.h"
#include "utils.h"
#include "user_manager.h"
#include "user.h"
#include "message_handler.h"
#include "common.h"
#include <stdint.h>
#include "room_manager.h"

extern Client clients[MAX_CLIENTS];

int is_valid_request(char command, const char *payload);
int decode_websocket_message(char *buffer, char *out_payload, size_t *out_len);

void *client_handler(void *socket_desc)
{
  int client_sock = *(int *)socket_desc;
  char buffer[BUFFER_SIZE];
  char decoded_message[BUFFER_SIZE];
  size_t decoded_length;
  int user_id = -1;
  while (1)
  {
    memset(buffer, 0, BUFFER_SIZE);
    int read_size = recv(client_sock, buffer, BUFFER_SIZE, 0);
    if (read_size <= 0)
      break;
    int opcode = decode_websocket_message(buffer, decoded_message, &decoded_length); // decode
    char command, payload[BUFFER_SIZE];
    sscanf(decoded_message, "%c %[^\n]", &command, payload);
    if (is_valid_request(command, payload))
      log_message("Debug: command 0x%02x, payload %s", command, payload);

    switch (command)
    {
    case CMD_REGISTER:
    {
      char username[BUFFER_SIZE], password[BUFFER_SIZE];
      sscanf(payload, "%s %s", username, password);
      register_user(client_sock, username, &user_id, password);
      break;
    }
    case CMD_LOGIN:
    {
      char username[BUFFER_SIZE], password[BUFFER_SIZE];
      sscanf(payload, "%s %s", username, password);
      login(client_sock, username, &user_id, password);
      break;
    }
    case CMD_CHAT:
    {
      char receiver_id[BUFFER_SIZE], sender_id[BUFFER_SIZE], message[BUFFER_SIZE];
      sscanf(payload, "%s %s %[^\n]", sender_id, receiver_id, message);
      send_private_message(atoi(sender_id), atoi(receiver_id), message);
      break;
    }
    case CMD_CHAT_OFFLINE: // 0x04: Chat offline
    {
      char receiver_id[BUFFER_SIZE], sender_id[BUFFER_SIZE], message[BUFFER_SIZE];
      sscanf(payload, "%s %s %[^\n]", sender_id, receiver_id, message);
      send_offline_message(atoi(sender_id), atoi(receiver_id), message);
      break;
    }
    case CMD_RETRIEVE: // 0x05: Retrieve message
    {
      char receiver_id[BUFFER_SIZE];
      char sender_id[BUFFER_SIZE];
      sscanf(payload, "%s %s", sender_id, receiver_id);
      retrieve_message(client_sock, atoi(sender_id), atoi(receiver_id));
      break;
    }
    case CMD_REQUEST_CHAT: // 0x15: Request private message
    {
      char receiver_id[BUFFER_SIZE], sender_id[BUFFER_SIZE];
      sscanf(payload, "%s %s %[^\n]", sender_id, receiver_id);
      request_private_message(atoi(sender_id), atoi(receiver_id));
      break;
    }
    case CMD_CHECK_PARTNERSHIP: // 0x18: Check chat partnership
    {
      char receiver_id[BUFFER_SIZE], sender_id[BUFFER_SIZE];
      sscanf(payload, "%s %s %[^\n]", sender_id, receiver_id);
      check_chat_partnership(atoi(sender_id), atoi(receiver_id));
      break;
    }
    case CMD_DISCONNECT_CHAT: // 0x19: Disconnect chat
    {
      char receiver_id[BUFFER_SIZE], sender_id[BUFFER_SIZE];
      sscanf(payload, "%s %s %[^\n]", sender_id, receiver_id);
      disconnect_chat(atoi(sender_id), atoi(receiver_id));
      break;
    }
    case CMD_ACCEPT_CHAT: // 0x1A: Accept chat request
    {
      char receiver_id[BUFFER_SIZE], sender_id[BUFFER_SIZE];
      sscanf(payload, "%s %s %[^\n]", sender_id, receiver_id);
      accept_chat_request(atoi(sender_id), atoi(receiver_id));
      break;
    }
    case CMD_ADDFR: // 0x06: Add friend
    {
      char username[BUFFER_SIZE];
      sscanf(payload, "%s", username);
      if (is_number(username))
      {
        int receiver_id = atoi(username);
        printf("%d\n", receiver_id);
        pthread_mutex_lock(&clients_mutex);

        if (receiver_id >= 0 && receiver_id < MAX_CLIENTS)
        {
          int result = send_friend_request(&clients[user_id], &clients[receiver_id]);
          pthread_mutex_unlock(&clients_mutex);

          if (result == 1)
          {
            char response[BUFFER_SIZE];
            snprintf(response, BUFFER_SIZE, "%d", RESPONSE_ADDFR);
            send_websocket_message(client_sock, response, strlen(response), 0);
          }
          else
          {
            char response[BUFFER_SIZE];
            snprintf(response, BUFFER_SIZE, "%d", FAIL_ADDFR);
            send_websocket_message(client_sock, response, strlen(response), 0);
          }
        }
        else
        {
          pthread_mutex_unlock(&clients_mutex);
          uint8_t response = FAIL_ADDFR;
          send_websocket_message(client_sock, (char *)&response, sizeof(response), 0);
        }
      }
      else
      {
        uint8_t response = FAIL_ADDFR;
        send_websocket_message(client_sock, (char *)&response, sizeof(response), 0);
      }
      break;
    }
    case CMD_ACCEPT: /// 0x07: Accept friend
    {
      char username[BUFFER_SIZE];
      sscanf(payload, "%s", username);
      if (is_number(username))
      {
        int sender_id = atoi(username);
        pthread_mutex_lock(&clients_mutex);

        if (sender_id >= 0 && sender_id < MAX_CLIENTS)
        {
          int result = accept_friend_request(&clients[user_id], &clients[sender_id]);
          pthread_mutex_unlock(&clients_mutex);

          if (result == 1)
          {
            send_websocket_message(client_sock, "Friend request accepted successfully.\n", strlen("Friend request accepted successfully.\n"), 0);
          }
          else
          {
            send_websocket_message(client_sock, "Failed to accept friend request. Maybe list full.\n", strlen("Failed to accept friend request. Maybe list full.\n"), 0);
          }
        }
        else
        {
          pthread_mutex_unlock(&clients_mutex);
          send_websocket_message(client_sock, "Invalid user ID or user is offline.\n", strlen("Invalid user ID or user is offline.\n"), 0);
        }
      }
      else
      {
        send_websocket_message(client_sock, "Invalid user ID format.\n", strlen("Invalid user ID format.\n"), 0);
      }
      break;
    }
    case CMD_DECLINE: /// 0x08: Decline friend
    {
      char username[BUFFER_SIZE];
      sscanf(payload, "%s", username);
      if (is_number(username))
      {
        int sender_id = atoi(username);
        pthread_mutex_lock(&clients_mutex);

        int result = decline_friend_request(&clients[user_id], sender_id);
        pthread_mutex_unlock(&clients_mutex);

        if (result == 1)
        {
          send_websocket_message(client_sock, "Friend request declined successfully.\n", strlen("Friend request declined successfully.\n"), 0);
        }
        else
        {
          send_websocket_message(client_sock, "Failed to decline friend request. Request not found.\n", strlen("Failed to decline friend request. Request not found.\n"), 0);
        }
      }
      else
      {
        send_websocket_message(client_sock, "Invalid user ID format.\n", strlen("Invalid user ID format.\n"), 0);
      }
      break;
    }
    case CMD_LISTFR: /// 0x09: List friend
    {
      char userID[BUFFER_SIZE];
      sscanf(payload, "%s", userID);
      user_id = atoi(userID);

      char *friends_list = get_friends(clients[user_id]);
      char response[BUFFER_SIZE];
      snprintf(response, BUFFER_SIZE, "%d %s", RESPONSE_LISTFR, friends_list);
      printf("friends_list: %s\n", friends_list);

      send_websocket_message(client_sock, response, strlen(response), 0);
      break;
    }
    case CMD_CANCEL: /// 0x0A: Cancel friend request
    {
      char username[BUFFER_SIZE];
      sscanf(payload, "%s", username);
      if (is_number(username))
      {
        int receiver_id = atoi(username);
        pthread_mutex_lock(&clients_mutex);

        if (receiver_id >= 0 && receiver_id < MAX_CLIENTS && clients[receiver_id].is_online)
        {
          int result = cancel_friend_request(&clients[user_id], &clients[receiver_id]);
          pthread_mutex_unlock(&clients_mutex);

          if (result == 1)
          {
            send_websocket_message(client_sock, "Friend request canceled successfully.\n", strlen("Friend request canceled successfully.\n"), 0);
          }
          else
          {
            send_websocket_message(client_sock, "Failed to cancel friend request. Request not found.\n", strlen("Failed to cancel friend request. Request not found.\n"), 0);
          }
        }
        else
        {
          pthread_mutex_unlock(&clients_mutex);
          send_websocket_message(client_sock, "Invalid user ID or user is offline.\n", strlen("Invalid user ID or user is offline.\n"), 0);
        }
      }
      else
      {
        send_websocket_message(client_sock, "Invalid user ID format.\n", strlen("Invalid user ID format.\n"), 0);
      }
      break;
    }
    case CMD_LISTREQ: /// 0x0B: List friend request
    {
      char userID[BUFFER_SIZE];
      sscanf(payload, "%s", userID);
      user_id = atoi(userID);

      char *req = get_friend_requests(clients[user_id]);
      if (strlen(req) > 0)
      {
        char response[BUFFER_SIZE];
        snprintf(response, BUFFER_SIZE, "%d %s", RESPONSE_LISTREQ, req);
        printf("request_list: %s\n", req);
        send_websocket_message(client_sock, response, strlen(response), 0);
      }
      else
      {
        send_websocket_message(client_sock, "No friend requests received.\n", strlen("No friend requests received.\n"), 0);
      }
      break;
    }
    case CMD_REMOVE: /// 0x0C: Remove friend
    {
      char username[BUFFER_SIZE];
      sscanf(payload, "%s", username);
      if (is_number(username))
      {
        int sender_id = atoi(username);
        pthread_mutex_lock(&clients_mutex);

        int result = remove_friend(&clients[user_id], &clients[sender_id]);
        pthread_mutex_unlock(&clients_mutex);
        if (result == 1)
        {
          send_websocket_message(client_sock, "Friend remove successfully.\n", strlen("Friend request declined successfully.\n"), 0);
        }
        else
        {
          send_websocket_message(client_sock, "Failed to remove friend. Request not found.\n", strlen("Failed to decline friend request. Request not found.\n"), 0);
        }
      }
      else
      {
        send_websocket_message(client_sock, "Invalid user ID format.\n", strlen("Invalid user ID format.\n"), 0);
      }
      break;
    }
    case CMD_CREATE_ROOM:
    {
      char room_name[BUFFER_SIZE];
      int creator_id;
      sscanf(payload, "%s %d", room_name, &creator_id);

      // Tạo room
      int room_id = create_room(room_name, creator_id);
      if (room_id >= 0)
      {
        // Thêm người tạo vào phòng (join_room)
        if (join_room(room_id, creator_id))
        {
          char response[BUFFER_SIZE];
          snprintf(response, sizeof(response), "Room created and joined successfully with ID: %d", room_id);
          send_websocket_message(client_sock, response, strlen(response), 0);
        }
        else
        {
          send_websocket_message(client_sock, "Failed to join room after creation.",
                                 strlen("Failed to join room after creation."), 0);
        }
      }
      else if (room_id == -2)
      {
        char error_msg[BUFFER_SIZE];
        snprintf(error_msg, sizeof(error_msg), "error_duplicate_room_name Room with name '%s' already exists.", room_name);
        send_websocket_message(client_sock, error_msg, strlen(error_msg), 0);
      }
      else
      {
        send_websocket_message(client_sock, "Failed to create room. Maximum rooms reached.", 
                               strlen("Failed to create room. Maximum rooms reached."), 0);
      }
      break;
    }
    case CMD_JOIN_ROOM:
    {
      int room_id, user_id;
      sscanf(payload, "%d %d", &room_id, &user_id);

      if (join_room(room_id, user_id))
      {
        char response[BUFFER_SIZE];
        snprintf(response, sizeof(response), "Joined room successfully with ID: %d", room_id);
        send_websocket_message(client_sock, response, strlen(response), 0);
      }
      else
      {
        send_websocket_message(client_sock, "Failed to join room. Room does not exist or you are already a member.",
                               strlen("Failed to join room. Room does not exist or you are already a member."), 0);
      }
      break;
    }
    case CMD_ROOM_MESSAGE:
    {
      char room_name[BUFFER_SIZE];
      char message[BUFFER_SIZE];
      int user_id;

      // Tách room_name và user_id
      char *token = strtok(payload, " "); // Lấy room_name
      if (token)
      {
        strncpy(room_name, token, BUFFER_SIZE);
        room_name[BUFFER_SIZE - 1] = '\0'; // Đảm bảo kết thúc chuỗi

        token = strtok(NULL, " "); // Lấy user_id
        if (token)
        {
          user_id = atoi(token);

          // Phần còn lại của payload là message
          char *message_start = payload + strlen(room_name) + 1 + strlen(token) + 1;
          strncpy(message, message_start, BUFFER_SIZE);
          message[BUFFER_SIZE - 1] = '\0'; // Đảm bảo kết thúc chuỗi

          // Debug để kiểm tra dữ liệu
          printf("Debug: Room Name: %s, User ID: %d, Message: %s\n", room_name, user_id, message);

          // Tiếp tục xử lý gửi tin nhắn
          pthread_mutex_lock(&rooms_mutex);
          int room_id = -1;

          for (int i = 0; i < MAX_ROOMS; i++)
          {
            if (rooms[i].id != -1 && strcmp(rooms[i].name, room_name) == 0)
            {
              room_id = i;
              break;
            }
          }
          pthread_mutex_unlock(&rooms_mutex);

          if (room_id != -1)
          {
            if (room_message(room_id, user_id, message, client_sock))
            {
              send_websocket_message(client_sock, "room_message_success", strlen("room_message_success"), 0);
              printf("Message sent successfully.\n");
            }
            else
            {
              send_websocket_message(client_sock, "error_sending_message", strlen("error_sending_message"), 0);
            }
          }
          else
          {
            send_websocket_message(client_sock, "room_not_found", strlen("room_not_found"), 0);
          }
        }
      }
      break;
    }
    case CMD_ADD_TO_ROOM:
    {
      char room_name[BUFFER_SIZE];
      int target_user_id;

      sscanf(payload, "%s %d", room_name, &target_user_id);

      pthread_mutex_lock(&rooms_mutex);
      int room_id = -1;

      for (int i = 0; i < MAX_ROOMS; i++)
      {
        if (rooms[i].id != -1 && strcmp(rooms[i].name, room_name) == 0)
        {
          room_id = i;
          break;
        }
      }
      pthread_mutex_unlock(&rooms_mutex);

      if (room_id != -1 && add_user_to_room(room_id, target_user_id))
      {
        send_websocket_message(client_sock, "user_added_to_room_success", strlen("user_added_to_room_success"), 0);
      }
      else
      {
        send_websocket_message(client_sock, "user_add_failed", strlen("user_add_failed"), 0);
      }
      break;
    }
    case CMD_LEAVE_ROOM:
    {
      char room_name[BUFFER_SIZE];
      int user_id;

      sscanf(payload, "%s %d", room_name, &user_id);

      pthread_mutex_lock(&rooms_mutex);
      int room_id = -1;

      for (int i = 0; i < MAX_ROOMS; i++)
      {
        if (rooms[i].id != -1 && strcmp(rooms[i].name, room_name) == 0)
        {
          room_id = i;
          break;
        }
      }
      pthread_mutex_unlock(&rooms_mutex);

      if (room_id != -1 && leave_room(room_id, user_id))
      {
        send_websocket_message(client_sock, "left_room_success", strlen("left_room_success"), 0);
      }
      else
      {
        send_websocket_message(client_sock, "left_room_failed", strlen("left_room_failed"), 0);
      }
      break;
    }
    case CMD_REMOVE_USER: // 0x12: Remove user from room
    {
      char room_name[BUFFER_SIZE];
      int remover_id;
      int user_id_to_remove;

      // Lấy tên phòng, ID người thực hiện, và ID người cần xoá
      sscanf(payload, "%s %d %d", room_name, &remover_id, &user_id_to_remove);
      // printf("room_id %d", room_name);
      // printf("remover_id %d", remover_id);
      // printf("user_id_to_remove %d", user_id_to_remove);
      pthread_mutex_lock(&rooms_mutex);
      int room_id = -1;

      // Tìm phòng theo tên
      for (int i = 0; i < MAX_ROOMS; i++)
      {
        if (rooms[i].id != -1 && strcmp(rooms[i].name, room_name) == 0)
        {
          room_id = i;
          break;
        }
      }
      pthread_mutex_unlock(&rooms_mutex);

      // Xử lý xoá người dùng khỏi phòng
      if (remove_user_from_room(room_id, remover_id, user_id_to_remove))
      {
        send_websocket_message(client_sock, "user_removed_from_room_success", strlen("user_removed_from_room_success"), 0);
      }
      else
      {
        send_websocket_message(client_sock, "user_remove_failed", strlen("user_remove_failed"), 0);
      }
      break;
    }
    case CMD_LIST_ROOMS: // 0x13: List rooms
    {
      int requested_user_id;
      sscanf(payload, "%d", &requested_user_id); // Lấy user_id từ payload

      char user_rooms[BUFFER_SIZE] = "";
      get_user_rooms(requested_user_id, user_rooms);

      if (strlen(user_rooms) > 0)
      {
        char response[BUFFER_SIZE];
        send_websocket_message(client_sock, user_rooms, strlen(user_rooms), 0);
      }
      else
      {
        send_websocket_message(client_sock, "You have not joined any rooms.\n", strlen("You have not joined any rooms.\n"), 0);
      }
      break;
    }
    case CMD_LOAD_ROOM_MESSAGES: // 0x16: Load room messages
    {
      char room_name[BUFFER_SIZE];
      int user_id;

      sscanf(payload, "%s %d", room_name, &user_id);

      char room_file[BUFFER_SIZE];
      snprintf(room_file, sizeof(room_file), "../server/room_data/room_%s.txt", room_name);

      FILE *file = fopen(room_file, "r");
      if (file)
      {
        char line[BUFFER_SIZE];
        char all_messages[BUFFER_SIZE * 10] = ""; // Chứa toàn bộ tin nhắn

        while (fgets(line, sizeof(line), file))
        {
          strcat(all_messages, line);
        }
        fclose(file);

        char response[BUFFER_SIZE * 10];
        snprintf(response, sizeof(response), "room_messages %s", all_messages);
        send_websocket_message(client_sock, response, strlen(response), 0);
      }
      else
      {
        send_websocket_message(client_sock, "room_messages No messages found.", strlen("room_messages No messages found."), 0);
      }
      break;
    }
    case CMD_LOGOUT: // 0x14: Logout
    {
      char userID[BUFFER_SIZE];
      sscanf(payload, "%s", userID);
      int logout_user_id = atoi(userID);

      pthread_mutex_lock(&clients_mutex);
      if (logout_user_id >= 0 && logout_user_id < MAX_CLIENTS && clients[logout_user_id].is_online)
      {
        clients[logout_user_id].is_online = 0;
        clients[logout_user_id].chatting_partner_id = -1;
        pthread_mutex_unlock(&clients_mutex);

        char response[BUFFER_SIZE];
        snprintf(response, BUFFER_SIZE, "%c Logout successful", RESPONSE_LOGOUT);
        send_websocket_message(client_sock, response, strlen(response), 0);
        log_message("User %d logged out successfully", logout_user_id);
      }
      else
      {
        pthread_mutex_unlock(&clients_mutex);
        send_websocket_message(client_sock, "Logout failed: User not found or already logged out.",
                               strlen("Logout failed: User not found or already logged out."), 0);
      }
      break;
    }
    case CMD_RECONNECT: // 0x1B: Reconnect
    {
      char userID[BUFFER_SIZE];
      sscanf(payload, "%s", userID);
      int reconnect_user_id = atoi(userID);

      pthread_mutex_lock(&clients_mutex);
      if (reconnect_user_id >= 0 && reconnect_user_id < MAX_CLIENTS)
      {
        clients[reconnect_user_id].is_online = 1;
        clients[reconnect_user_id].socket = client_sock;
        user_id = reconnect_user_id;
        pthread_mutex_unlock(&clients_mutex);

        log_message("User %d reconnected successfully, socket %d", reconnect_user_id, client_sock);
        send_websocket_message(client_sock, "Reconnect successful", strlen("Reconnect successful"), 0);
      }
      else
      {
        pthread_mutex_unlock(&clients_mutex);
        send_websocket_message(client_sock, "Reconnect failed: Invalid user ID",
                               strlen("Reconnect failed: Invalid user ID"), 0);
      }
      break;
    }
    case CMD_VIEW_ROOM_MEMBERS: // 0x17: View room members
    {
      char room_name[BUFFER_SIZE];
      sscanf(payload, "%s", room_name);

      pthread_mutex_lock(&rooms_mutex);
      int room_id = -1;
      for (int i = 0; i < MAX_ROOMS; i++)
      {
        if (rooms[i].id != -1 && strcmp(rooms[i].name, room_name) == 0)
        {
          room_id = i;
          break;
        }
      }
      pthread_mutex_unlock(&rooms_mutex);

      if (room_id != -1)
      {
        char members_list[BUFFER_SIZE * 10];
        get_room_members(room_id, members_list);
        send_websocket_message(client_sock, members_list, strlen(members_list), 0);
      }
      else
      {
        send_websocket_message(client_sock, "members_list Room not found or invalid room ID.", strlen("members_list Room not found or invalid room ID."), 0);
      }
      break;
    }
    }
  }

  remove_client(user_id);
  log_message("Client disconnected: ID %d", user_id);
  close(client_sock);
  free(socket_desc);
  return NULL;
}

const char valid_commands[] = {
    CMD_REGISTER, CMD_LOGIN, CMD_CHAT, CMD_CHAT_OFFLINE, CMD_RETRIEVE, CMD_REQUEST_CHAT,
    CMD_CHECK_PARTNERSHIP, CMD_DISCONNECT_CHAT, CMD_ACCEPT_CHAT, CMD_ADDFR, CMD_ACCEPT,
    CMD_DECLINE, CMD_LISTFR, CMD_CANCEL, CMD_LISTREQ, CMD_REMOVE, CMD_CREATE_ROOM, CMD_JOIN_ROOM,
    CMD_ROOM_MESSAGE, CMD_ADD_TO_ROOM, CMD_LEAVE_ROOM, CMD_REMOVE_USER, CMD_LIST_ROOMS,
    CMD_LOAD_ROOM_MESSAGES, CMD_VIEW_ROOM_MEMBERS, CMD_LOGOUT, CMD_RECONNECT};

int is_valid_request(char command, const char *payload)
{
  if (!command || !payload)
  {
    return 0;
  }
  for (int i = 0; i < sizeof(valid_commands); i++)
  {
    if (valid_commands[i] == command)
    {
      return 1;
    }
  }
  return 0;
}

// Function to decode WebSocket frames
int decode_websocket_message(char *buffer, char *out, size_t *out_len)
{
  uint8_t *p = (uint8_t *)buffer;
  size_t payload_len;
  size_t mask_offset = 0;
  size_t i;

  // First byte contains the FIN bit and opcode
  uint8_t first_byte = p[0];
  uint8_t opcode = first_byte & 0x0F; // Extract the opcode (lower 4 bits)

  // Second byte contains the masking bit and payload length
  uint8_t second_byte = p[1];
  int mask = (second_byte & 0x80) >> 7; // Extract masking bit
  payload_len = second_byte & 0x7F;     // Extract payload length

  // Check for extended payload length (if >= 126)
  if (payload_len == 126)
  {
    payload_len = (p[2] << 8) | p[3]; // 2 bytes for extended payload length
    mask_offset = 4;                  // Move to the masking key
  }
  else if (payload_len == 127)
  {
    // For simplicity, we won't handle this case (8-byte length)
    return -1; // Unsupported frame size
  }
  else
  {
    mask_offset = 2; // Move to the masking key
  }

  // Get the masking key if it's a masked frame
  uint8_t masking_key[4] = {0};
  if (mask)
  {
    for (i = 0; i < 4; i++)
    {
      masking_key[i] = p[mask_offset + i];
    }
  }

  // Decode the payload
  *out_len = payload_len;
  for (i = 0; i < payload_len; i++)
  {
    out[i] = mask ? (p[mask_offset + 4 + i] ^ masking_key[i % 4]) : p[mask_offset + 4 + i];
  }
  out[payload_len] = '\0'; // Null-terminate the output

  return opcode; // Return the opcode of the message
}

void init_clients()
{
  for (int i = 0; i < MAX_CLIENTS; i++)
  {
    clients[i].socket = -1;
    clients[i].chatting_partner_id = -1;
    clients[i].is_online = 0;
    clients[i].id = -1;
  }
}

int add_client(int client_sock, int id, const char *username, const char *password)
{
  pthread_mutex_lock(&clients_mutex);
  if (id >= 0 && id < MAX_CLIENTS)
  {
    clients[id].id = id;
    clients[id].is_online = 1;
    // strcpy(clients[id].password, password);
    // strcpy(clients[id].username, username);
    clients[id].socket = client_sock;
    strncpy(clients[id].username, username, BUFFER_SIZE);
    strncpy(clients[id].password, password, BUFFER_SIZE);
  }
  pthread_mutex_unlock(&clients_mutex);
  return id;
}

void remove_client(int id)
{
  pthread_mutex_lock(&clients_mutex);
  if (id >= 0 && id < MAX_CLIENTS)
  {
    clients[id].socket = -1;
    clients[id].is_online = 0;
  }
  pthread_mutex_unlock(&clients_mutex);
}
