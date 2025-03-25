#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <unistd.h>

#define bufferSize 512

int main() {
    int socketfd = socket(AF_INET, SOCK_STREAM, 0);
    if(socketfd == -1) {
        perror("socketfd creation error");
        close(socketfd);
        return -1;
    }
    
    printf("socketfd: %i\n", socketfd);
    
    struct sockaddr_in address = {
        AF_INET,
        htons(9999),
        htonl(INADDR_LOOPBACK)
    };
    
    int connection = connect(socketfd, (struct sockaddr *)&address, sizeof(address));
    if(connection == -1) {
        perror("connection error");
        close(socketfd);
        return -1;
    }
    printf("connection: %i\n", connection);
    
    struct pollfd fds[2] = {
        {
            STDIN_FILENO,
            POLLIN,
            0
        },
        {
            socketfd,
            POLLIN,
            0
        }
    };

    for (;;) {
        char buffer[bufferSize] = { 0 };
        
        poll(fds, 2, 10000);

        if (fds[0].revents & POLLIN) {
            read(0, buffer, bufferSize);
            if(buffer[0] == 10 || buffer[0] == 'e' && buffer[1] == 'x' && buffer[2] == 'i' && buffer[3] == 't') {
                printf("CLOSING THE CONNECTION BY THE CLIENT\n");
                break;
            }
            send(socketfd, buffer, bufferSize-1, 0);
        } else if (fds[1].revents & POLLIN) {
            if (recv(socketfd, buffer, bufferSize, 0) == 0) {
                printf("CLOSING THE CONNECTION BY THE SERVER\n");
                break;
            } 
            printf("%s", buffer);
        }
    }
    close(socketfd);
    printf("THE CONNECTION HAS BEEN CLOSED\n");
    return 0;
}
