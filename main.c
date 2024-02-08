#include <Ws2tcpip.h>
#include <corecrt.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <string.h>
#include <errno.h>

#define STRINGIFY(T) #T
#define DEFAULT_PORT "8080"
#define DEFAULT_BUFLEN 1024
#define DEFAULT_RESPONSE_FILE "./index.html"

char* response_html = NULL;
size_t response_len;

errno_t load_response_html(const char* path, char** result_html, size_t* len);
int serve_connection(SOCKET client_socket);


int main(){
    errno_t e = load_response_html( DEFAULT_RESPONSE_FILE , 
        &response_html, 
        &response_len);
    if(e != 0){
        fprintf(stderr, "Failed to load HTML response file");
        return 1;
    }

    int result = 0;

    WSADATA wsaData;

    SOCKET listen_socket = INVALID_SOCKET;

    struct sockaddr_in service;

    if((result = WSAStartup(MAKEWORD(2, 2), &wsaData)) != 0){
        fprintf(stderr, STRINGIFY(WSAStartup)" failed: %d", result);
        return 1;
    }

    struct addrinfo *a_info = NULL, *ptr = NULL, hints;

    SecureZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    if((result = getaddrinfo(NULL, DEFAULT_PORT, &hints, &a_info)) != 0){
        fprintf(stderr, STRINGIFY(getaddrinfo)" failed: %d", result);
        WSACleanup();
        return 1;
    }

    listen_socket = socket(a_info->ai_family, a_info->ai_socktype, a_info->ai_protocol);

    if(listen_socket == INVALID_SOCKET){
        fprintf(stderr, "Error at " STRINGIFY(socket) "(): %d\n", WSAGetLastError());
        freeaddrinfo(a_info);
        WSACleanup();
        return 1;
    }
    if(bind(listen_socket, a_info->ai_addr, a_info->ai_addrlen) == SOCKET_ERROR){
        fprintf(stderr, "bind failed: %d\n", WSAGetLastError());
        freeaddrinfo(a_info);
        closesocket(listen_socket);
        return 1;
    }
    freeaddrinfo(a_info);
    
    printf("Listening for connection...\n");
    //while(0){
        if((result = listen(listen_socket, SOMAXCONN)) == SOCKET_ERROR){
            fprintf(stderr, "Listen failed with error: %d\n", WSAGetLastError());
            closesocket(listen_socket);
            WSACleanup();
            return 1;
        }

        SOCKET client_socket;
        client_socket = accept(listen_socket, NULL, NULL);
        if (client_socket == INVALID_SOCKET){
            fprintf(stderr, "accept failed: %d\n", WSAGetLastError());
            closesocket(listen_socket);
            WSACleanup();
            return 1;
        }
        serve_connection(client_socket);
        if((result = shutdown(client_socket, SD_SEND)) == SOCKET_ERROR){
            fprintf(stderr, "shutdown failed: %d\n", WSAGetLastError());
            closesocket(client_socket);
            WSACleanup();
            return 1;
        }
        // if(serve_connection(client_socket) != 0)
        //     return 1;
    //}
    free(response_html);
    return 0;
}

errno_t load_response_html(const char* path, char** result_html, size_t* len){
    FILE* file;
    errno_t result;
    result = fopen_s(&file, path, "r");
    if(result != 0) return result;
    if(file == NULL) {
        fprintf(stderr, "could not find file\n");
        return 1;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char* buff = malloc(size + 1);

    size_t new_len = fread(buff, sizeof(char), size, file);
    if (ferror(file) != 0) {
        fputs("Error reading file\n", stderr);
    } else {
        buff[new_len++] = '\0'; 
    }
    fclose(file);
    *result_html = buff;
    *len = new_len;
    return 0;
}
int serve_connection(SOCKET client_socket){
    char recv_buf[DEFAULT_BUFLEN];
    int i_res, i_send_res;
    int recv_buf_len = DEFAULT_BUFLEN;
    char header[50] = {0};
    sprintf(header, "HTTP/1.1 200 OK\r\nContent-Length: %lld\r\n\n", response_len); 
    size_t header_len = strlen(header);

    do {
        i_res = recv(client_socket, recv_buf, recv_buf_len, 0);
        if(i_res > 0){
            printf("Bytes received: %d\n", i_res);
            printf("%.*s\n", recv_buf_len, recv_buf);
            i_send_res = send(client_socket, header, header_len, 0);
            i_send_res += send(client_socket, response_html, response_len, 0);
            if(i_send_res == SOCKET_ERROR){
                closesocket(client_socket);
                WSACleanup();
                return 1;
            }
            printf("Bytes sent: %d\n", i_send_res);
            printf("Response HTTP:\n%s", header);
        } else if(i_res == 0) {
            printf("Closing connection...");
        } else {
            fprintf(stderr, "recv failed: %d\n", WSAGetLastError());
            closesocket(client_socket);
            WSACleanup();
            return 1;
        }
    }
    while(i_res > 0);
    return 0;
}