#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define MAX_MESSAGE_LEN 50000
#define MAX_BUFFER_LEN (MAX_MESSAGE_LEN * 2 + 1)
#define MAX_MESSAGE_SIZE (MAX_BUFFER_LEN + 9)
#define CRC32_POLYNOMIAL 0x04C11DB7

unsigned int calculate_crc32(const char *data, int length) {
    unsigned int crc = 0xFFFFFFFF;
    for (int i = 0; i < length; i++) {
        crc ^= data[i] << 24;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80000000) {
                crc = (crc << 1) ^ CRC32_POLYNOMIAL;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
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

int hamming_decode(char *encoded, char *decoded) {
    memset(decoded, 0, 4);
    int p1 = (encoded[0] - '0') ^ (encoded[2] - '0') ^ (encoded[4] - '0') ^ (encoded[6] - '0');
    int p2 = (encoded[1] - '0') ^ (encoded[2] - '0') ^ (encoded[5] - '0') ^ (encoded[6] - '0');
    int p4 = (encoded[3] - '0') ^ (encoded[4] - '0') ^ (encoded[5] - '0') ^ (encoded[6] - '0');
    int error_position = p1 + p2 * 2 + p4 * 4;
    if (error_position != 0) {
        encoded[error_position - 1] = (encoded[error_position - 1] == '0') ? '1' : '0';
    }
    decoded[0] = encoded[2];
    decoded[1] = encoded[4];
    decoded[2] = encoded[5];
    decoded[3] = encoded[6];
    return error_position;
}

void error(const char *msg) {
    perror(msg);
    exit(1);
}

void send_request(int sockfd, const char *request) {
    unsigned int crc32 = calculate_crc32(request, strlen(request));

    char request_with_crc[MAX_MESSAGE_SIZE + 9];
    snprintf(request_with_crc, sizeof(request_with_crc), "<REQUEST>%s%08X</REQUEST>", request, crc32);

    char encoded_request[8];
    hamming_encode(request_with_crc, encoded_request);

    int n = write(sockfd, encoded_request, 8);

    if (n < 0) {
        error("ERROR writing to socket");
    }
}

void receive_response(int sockfd) {
    char buffer[8];
    bzero(buffer, sizeof(buffer));
    int n = read(sockfd, buffer, 8);

    if (n < 0) {
        error("ERROR reading from socket");
    }

    char decoded_response[8];
    int decode_result = hamming_decode(buffer, decoded_response);

    if (decode_result == 0) {
        printf("Server response (decoded): %s\n", decoded_response);
    } else {
        printf("Hamming detected an error at position %d. Corrected message: %s\n", decode_result, decoded_response);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 6) {
        fprintf(stderr, "Usage: %s <server_ip> <server_port> <username> <password> <message>\n", argv[0]);
        exit(1);
    }

    int sockfd, portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    char username[MAX_MESSAGE_SIZE];
    char password[MAX_MESSAGE_SIZE];
    char message[MAX_MESSAGE_SIZE];

    portno = atoi(argv[2]);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) {
        error("ERROR opening socket");
    }

    server = gethostbyname(argv[1]);

    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host\n");
        exit(1);
    }

    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        error("ERROR connecting");
    }

    strncpy(username, argv[3], MAX_MESSAGE_SIZE);
    strncpy(password, argv[4], MAX_MESSAGE_SIZE);
    strncpy(message, argv[5], MAX_MESSAGE_SIZE);

    // Send a login request with the username and password
    char login_request[MAX_MESSAGE_SIZE];
    snprintf(login_request, sizeof(login_request), "<LOGIN><USERNAME>%s</USERNAME><PASSWORD>%s</PASSWORD></LOGIN>", username, password);
    send_request(sockfd, login_request);
    receive_response(sockfd);

    // Send a message to the server
    char msg_request[MAX_MESSAGE_SIZE];
    snprintf(msg_request, sizeof(msg_request), "<MSG><FROM>%s</FROM><TO>c2</TO><BODY>%s</BODY></MSG>", username, message);
    send_request(sockfd, msg_request);
    receive_response(sockfd);

    close(sockfd);

    return 0;
}
