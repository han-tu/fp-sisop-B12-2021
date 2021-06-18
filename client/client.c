#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#define PORT 8080
  
int main(int argc, char const *argv[]) {
    // if (argc != 5 || argc != 1) {
    //     printf("Try Again!%d\n", argc);
    //     return 0 ;
    // }
    // else if (strcmp(argv[1], "-u") != 0 || strcmp(argv[3], "-p") != 0) {
    //     printf("Wrong argument(s), please try again\n") ;
    //     return 0 ;
    // }
    struct sockaddr_in address;
    int sock = 0, valread;
    struct sockaddr_in serv_addr;
    char *hello = "Hello from client";
    char buffer[1024] = {0};
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }
  
    memset(&serv_addr, '0', sizeof(serv_addr));
  
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
      
    if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<=0) {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }
  
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
        return -1;
    }

    int uid = getuid() ; // UID = 0 itu superuser   
    char struid[10] ; sprintf(struid, "%d", uid) ;
    send(sock, struid, strlen(struid), 0) ; // Ngirim UID
    

    if (uid != 0) {
        read(sock, buffer, 1024) ;  // Cuma supaya gak
        bzero(buffer, 1024) ;       // send 2x

        sprintf(buffer, "%s:%s\n", argv[2], argv[4]) ;
        send(sock, buffer, strlen(buffer), 0) ;

    }

    bzero(buffer, 1024) ;
    read(sock, buffer, 1024) ; // Membaca hasil authentikasi

    printf("%s\n", buffer) ;

    if (strcmp(buffer, "Username or password is invalid") == 0) {
        return 0 ;
    }

    char query[255] ;
    // Start Here
    while (1) {
        bzero(query, 255) ;
        printf(">> ") ;
        scanf(" %[^\n]", query) ;
        send(sock, query, strlen(query), 0) ;

        bzero(buffer, 1024) ;
        read(sock, buffer, 1024) ;

        printf("%s\n", buffer) ;
    }

    return 0;
}