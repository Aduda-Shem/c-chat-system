#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/time.h>

#define MAX_USERS 10
#define MAX_MESSAGE_LEN 50000
#define CHAT_HISTORY_FILE "chat_history.txt"
#define USER_ACCOUNTS_FILE "user_accounts.txt"

struct UserAccount {
    char username[256];
    char password[256];
};

struct User {
    char username[256];
    int socket;
};

struct ChatHistory {
    char messages[MAX_MESSAGE_LEN][MAX_USERS];
    int message_count;
};

int sockfd;
int user_count = 0;
pthread_t threads[MAX_USERS];
struct User users[MAX_USERS];
struct ChatHistory chat_history;
struct UserAccount user_accounts[MAX_USERS];

unsigned int calculate_crc32(const char *data, int length) {
    unsigned int crc = 0xFFFFFFFF;
    for (int i = 0; i < length; i++) {
        crc ^= data[i] << 24;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80000000) {
                crc = (crc << 1) ^ 0xEDB88320;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

void error(const char *msg) {
    perror(msg);
    exit(1);
}

void hamming_encode(const char *data, char *encoded) {
    memset(encoded, 0, 8);
    encoded[2] = data[0];
    encoded[4] = data[1];
    encoded[5] = data[2];
    encoded[6] = data[3];
    encoded[0] = (encoded[2] ^ encoded[4] ^ encoded[6]) + '0';
    encoded[1] = (encoded[2] ^ encoded[5] ^ encoded[6]) + '0';
    encoded[3] = (encoded[4] ^ encoded[5] ^ encoded[6]) + '0';
}

void initialize_server(int port) {
    struct sockaddr_in serv_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        error("ERROR opening socket");
    }

    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        error("ERROR on binding");
    }
    listen(sockfd, 5);
}

void load_user_accounts() {
    FILE *file = fopen(USER_ACCOUNTS_FILE, "r");
    if (file == NULL) {
        perror("Failed to open user accounts file");
        exit(1);
    }

    user_count = 0;
    while (fscanf(file, "%s %s", user_accounts[user_count].username, user_accounts[user_count].password) != EOF) {
        user_count++;
    }

    fclose(file);
}

void save_user_accounts() {
    FILE *file = fopen(USER_ACCOUNTS_FILE, "w");
    if (file == NULL) {
        perror("Failed to open user accounts file");
        exit(1);
    }

    for (int i = 0; i < user_count; i++) {
        fprintf(file, "%s %s\n", user_accounts[i].username, user_accounts[i].password);
    }

    fclose(file);
}

void register_user(int client_socket) {
    char username[256];
    char password[256];

    int n = recv(client_socket, username, sizeof(username), 0);
    if (n < 0) {
        error("ERROR receiving username");
    }
    username[n] = '\0';

    n = recv(client_socket, password, sizeof(password), 0);
    if (n < 0) {
        error("ERROR receiving password");
    }
    password[n] = '\0';

    for (int i = 0; i < user_count; i++) {
        if (strcmp(username, user_accounts[i].username) == 0) {
            send(client_socket, "Username already taken", 22, 0);
            close(client_socket);
            pthread_exit(NULL);
        }
    }

    if (user_count < MAX_USERS) {
        strncpy(user_accounts[user_count].username, username, sizeof(user_accounts[user_count].username));
        strncpy(user_accounts[user_count].password, password, sizeof(user_accounts[user_count].password));
        user_count++;
        send(client_socket, "Registration successful", 23, 0);
        save_user_accounts();
    } else {
        send(client_socket, "Server is full", 13, 0);
    }
}

int authenticate_user(int client_socket) {
    char username[256];
    char password[256];

    int n = recv(client_socket, username, sizeof(username), 0);
    if (n < 0) {
        error("ERROR receiving username");
    }
    username[n] = '\0';

    n = recv(client_socket, password, sizeof(password), 0);
    if (n < 0) {
        error("ERROR receiving password");
    }
    password[n] = '\0';

    for (int i = 0; i < user_count; i++) {
        if (strcmp(username, user_accounts[i].username) == 0 && strcmp(password, user_accounts[i].password) == 0) {
            return 1;
        }
    }
    return 0;
}

void *handle_messages(void *user_data) {
    struct User *user = (struct User *)user_data;
    char client_message[MAX_MESSAGE_LEN];

    while (1) {
        bzero(client_message, sizeof(client_message));
        int n = recv(user->socket, client_message, sizeof(client_message) - 1, 0);
        if (n < 0) {
            error("ERROR reading from socket");
        }

        if (n == 0) {
            printf("Client disconnected.\n");
            close(user->socket);
            pthread_exit(NULL);
        } else {
            printf("Received message from client: %s\n", client_message);

            unsigned int crc = calculate_crc32(client_message, n);
            unsigned int received_crc;
            recv(user->socket, &received_crc, sizeof(received_crc), 0);

            if (crc == received_crc) {
                printf("CRC check passed.\n");
            } else {
                printf("CRC check failed.\n");
            }
        }
    }
    return NULL;
}

void create_client_threads() {
    while (1) {
        int client_socket;
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        client_socket = accept(sockfd, (struct sockaddr *)&client_addr, &client_len);

        if (client_socket < 0) {
            error("ERROR on accept");
        }

        int user_index = -1;
        for (int i = 0; i < MAX_USERS; i++) {
            if (users[i].socket == -1) {
                user_index = i;
                break;
            }
        }

        if (user_index == -1) {
            close(client_socket);
        } else {
            users[user_index].socket = client_socket;
            pthread_create(&threads[user_index], NULL, handle_messages, &users[user_index]);
        }
    }
}

void addMessageToHistory(const char *message) {
    if (chat_history.message_count < MAX_MESSAGE_LEN) {
        strncpy(chat_history.messages[chat_history.message_count], message, MAX_MESSAGE_LEN);
        chat_history.message_count++;
    }
}

void load_chat_history() {
    char line[MAX_MESSAGE_LEN];
    FILE *chat_history_file = fopen(CHAT_HISTORY_FILE, "a+");

    if (chat_history_file == NULL) {
        perror("Failed to open chat history file");
        exit(1);
    }

    chat_history.message_count = 0;

    while (fgets(line, sizeof(line), chat_history_file) != NULL) {
        line[strcspn(line, "\n")] = '\0';
        addMessageToHistory(line);
    }

    fclose(chat_history_file);
}

void user_interface() {
    char input[MAX_MESSAGE_LEN];
    while (1) {
        printf("Server Console: Type 'exit' to shut down the server.\n");
        printf("Server Console: Type 'list' to list connected clients.\n");
        printf("Server Console: Enter a message to broadcast to all clients:\n");

        fgets(input, sizeof(input), stdin);
        input[strcspn(input, "\n")] = '\0';

        if (strcmp(input, "exit") == 0) {
            break;
        } else if (strcmp(input, "list") == 0) {
            for (int i = 0; i < MAX_USERS; i++) {
                if (users[i].socket != -1) {
                    printf("Connected Client: %s\n", users[i].username);
                }
            }
        } else {
            for (int i = 0; i < MAX_USERS; i++) {
                if (users[i].socket != -1) {
                    int n = write(users[i].socket, input, strlen(input));
                    if (n < 0) {
                        error("ERROR writing to socket");
                    }
                }
            }
        }
    }
}

void run_tests() {
    // Your testing logic here
}

void cleanup() {
    for (int i = 0; i < MAX_USERS; i++) {
        if (users[i].socket != -1) {
            close(users[i].socket);
            users[i].socket = -1;
        }
    }
    close(sockfd);
    exit(0);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    int port = atoi(argv[1]);

    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;

    initialize_server(port);
    load_chat_history();
    load_user_accounts(); // Load user accounts from file
    create_client_threads();
    user_interface();
    run_tests();
    cleanup();

    return 0;
}
