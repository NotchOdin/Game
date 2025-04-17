#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>

#define bufferSize 512
#define TRUE 1
#define FALSE 0

typedef struct playersList {
    int playerID;
    int playerfd;
    int isStopped;
    char fleetSize;
    char ships[11][24];
    struct playersList* nextPlayer;
    //pthread_t* connectionThread;
    //pthread_t* nextThread;
} playersList;

playersList* headOfPlayers = NULL;
pthread_t gameManagerThread = 0;

int gameStartedFlag = FALSE;
int socketfd = 0; // Server's socket
int joined = 0; // Numbers of users connected to the server
int lastPlayerID = 1;
int headOfPlayersSize = 0;
int playerTurnFlagID = 0;

void* instancesManager(void* arg);
void* warshipChoosing(void* arg);
int removePlayer(int playerID);
void* connectionInstance(void* arg);
int sendTheMap(playersList* player);
void sendToAllPlayers(char* message);
void* gameManager(void* args);

int disconnectAll() {
    
}

int main() {
    socketfd = socket(AF_INET, SOCK_STREAM, 0); // Create a server socket.
    if(socketfd == -1) {
        perror("socketfd creation error");
        close(socketfd);
        return -1;
    }
    printf("socketfd: %i\n", socketfd);
    
    struct sockaddr_in address = {
        AF_INET,
        htons(9999),
        htonl(INADDR_ANY)
    };

    int bnd = bind(socketfd, (struct sockaddr *)&address, sizeof(address)); // Binding.
    if(bnd == -1) {
        perror("binding error");
        close(socketfd);
        return -1;
    }
    printf("bind: %i\n", bnd);

    int lis = listen(socketfd, 4); // Start listening.
    if(lis == -1) {
        perror("listening error");
        close(socketfd);
        return -1;
    }
    printf("listen: %i\n", lis);;

    pthread_t instancesManagerThread = 0;
    pthread_create(&instancesManagerThread, NULL, instancesManager, NULL); // Create instancesManager thread.
    pthread_detach(instancesManagerThread);

    pthread_create(&gameManagerThread, NULL, gameManager, NULL); // Create gameManager thread.
    pthread_detach(gameManagerThread);

    struct pollfd fd[1] = {
        {
            STDIN_FILENO,
            POLLIN,
            0
        },
    };

    char buffer[10] = { 0 };
    while (TRUE) {
        int pollStatus = poll(fd, 1, 2000);
        if(pollStatus == 0) {
            continue;
        } else if (fd[0].revents & POLLIN) {
            read(0, buffer, sizeof(buffer)-1);
            if (buffer[0] == 'E' && buffer[1] == 'X' && buffer[2] == 'I' && buffer[3] == 'T') {
                disconnectAll();
                close(socketfd);
                return 0;
            }
        }
    }
}

void* instancesManager(void* arg) {
    pthread_t* warshipChoosingThread = NULL;
    while(TRUE) {
        int* clientfd = (int*)malloc(sizeof(int)); //  Create a dynamically allocated client socket.
        *clientfd = accept(socketfd, 0, 0); // Accept the newly created client socket.
        if(*clientfd == -1) {
            perror("accept error");
        }
        joined++;

        warshipChoosingThread = (pthread_t*)malloc(sizeof(pthread_t));
        pthread_create(warshipChoosingThread, NULL, warshipChoosing, clientfd); // Create a warshipChoosing thread for the new client
        pthread_detach(*warshipChoosingThread);
        
        if(joined == 4) {
            printf("The server is full\n");
            while(joined == 4) {
                sleep(3);
            }
            printf("The server is no longer full\n");
        }
    }
}

void* warshipChoosing(void* arg) {
    int* args = (int*)arg;
    int clientfd = *args;
    free(args);

    char warshipChoosingBuffer[bufferSize] = { 0 };
    int sendID = 0;

    playersList* newPlayer = (playersList*)calloc(1, sizeof(playersList)); // memory initialized to 0
    newPlayer->playerfd = clientfd;
    sprintf(warshipChoosingBuffer, "-Next you will choose your warships...\n\n");
    sendID = send(clientfd, warshipChoosingBuffer, strlen(warshipChoosingBuffer), 0);
    if (sendID < 0) {
        free(newPlayer);
        return NULL;
    }
    sleep(1);

    char map[11][24] = {0};
    map[0][0] = ' ';
    map[0][1] = ' ';
    int temp = 0;
    for(int i = 2; i<22; i = i + 2) {
        map[0][i] = 48+temp;
        map[0][i+1] = ' ';
        temp++;
    }
    map[0][22] = '\n';
    for(int i = 1; i<11; i++) {
        map[i][0] = i+47;
        map[i][1] = ' ';
        for(int b = 2; b<22; b = b + 2) {
            map[i][b] = 'O';
            map[i][b+1] = ' ';
        }
        map[i][22] = '\n';
    }

    int response = 0;

    for(int i = 0; i<15; i++) {
        for(int b = 0; b<11; b++) {
            snprintf(warshipChoosingBuffer, 24, "%s", map[b]);
            sendID = send(clientfd, warshipChoosingBuffer, strlen(warshipChoosingBuffer), 0);
            if (sendID < 0) {
                free(newPlayer);
                return NULL;
            }
        }

        sprintf(warshipChoosingBuffer, "%i/15: Choose your next point [Example: 57 for (7, 5)]\n", i+1);
        sendID = send(clientfd, warshipChoosingBuffer, strlen(warshipChoosingBuffer), 0);
        if (sendID < 0) {
            free(newPlayer);
            return NULL;
        }
        
        response = recv(clientfd, warshipChoosingBuffer, bufferSize, 0);

        if (response == 0) {
            close(clientfd);
            break;
        } else if (response == -1) {
            perror("response error line ~200");
            break;
        }

        if((warshipChoosingBuffer[0] >= '0' && warshipChoosingBuffer[0] <= '9') && (warshipChoosingBuffer[1] >= '0' && warshipChoosingBuffer[1] <= '9')) {
            if(map[warshipChoosingBuffer[0]-47][(warshipChoosingBuffer[1]-48)*2+2] != 'W') {
                map[warshipChoosingBuffer[0]-47][(warshipChoosingBuffer[1]-48)*2+2] = 'W';
            } else {
                sprintf(warshipChoosingBuffer, "%s", "This point is already chosen!\n");
                sendID = send(clientfd, warshipChoosingBuffer, strlen(warshipChoosingBuffer), 0);
                if (sendID < 0) {
                    free(newPlayer);
                    return NULL;
                }
                i = i-1;
            }
        } else {
            sprintf(warshipChoosingBuffer, "%s", "Wrong input!\n");
            sendID = send(clientfd, warshipChoosingBuffer, strlen(warshipChoosingBuffer), 0);
            if (sendID < 0) {
                free(newPlayer);
                return NULL;
            }
            i = i-1;
        }
    }

    if (response == 0) {
        free(newPlayer);
        return NULL;
    }

    for(int b = 0; b<11; b++) {
            snprintf(warshipChoosingBuffer, 24, "%s", map[b]);
            sendID = send(clientfd, warshipChoosingBuffer, strlen(warshipChoosingBuffer), 0);
            if (sendID < 0) {
                free(newPlayer);
                return NULL;
            }
        }
        
    for(int i = 0; i<11; i++) {
        for(int j = 0; j<24; j++) {
            newPlayer->ships[i][j] = map[i][j];
        }
    }

    newPlayer->fleetSize = 15;

    sprintf(warshipChoosingBuffer, "\nGreat! now you can wait for other players...\n");
    sendID = send(clientfd, warshipChoosingBuffer, sizeof(warshipChoosingBuffer), 0);
    if (sendID < 0) {
        free(newPlayer);
        return NULL;
    }

    pthread_t* newConnectionThread = (pthread_t*)malloc(sizeof(pthread_t));
    //newPlayer->connectionThread = newConnectionThread;
    //newPlayer->nextThread = NULL;

    pthread_create(newConnectionThread, NULL, connectionInstance, newPlayer);
    //newPlayer->connectionThread = newConnectionThread;
    pthread_detach(*newConnectionThread);
    //newPlayer->nextThread = NULL;

    if(gameStartedFlag == 1) {
        sprintf(warshipChoosingBuffer, "\nThe game has already started, wait for its end...\n");
        sendID = send(clientfd, warshipChoosingBuffer, sizeof(warshipChoosingBuffer), 0);
        if (sendID < 0) {
            newPlayer->playerID = -1;
            sleep(1); // for synchronization with connectionInstance
            free(newPlayer);
            return NULL;
        }
    }
    while(gameStartedFlag == 1) {
        sleep(1);
    }

    newPlayer->playerID = lastPlayerID;
    lastPlayerID++;
    newPlayer->nextPlayer = NULL;
    printf("A new player has connected: #%i\n", newPlayer->playerID);
    sleep(1);
    sprintf(warshipChoosingBuffer, "{You are player #%i}\n", newPlayer->playerID);
    sendID = send(clientfd, warshipChoosingBuffer, sizeof(warshipChoosingBuffer), 0);
    if (sendID < 0) {
        newPlayer->playerID = -1;
        sleep(1); // for synchronization with connectionInstance
        free(newPlayer);
        return NULL;
    }

    if(headOfPlayers == NULL) {
        headOfPlayers = newPlayer;
    } else {
        playersList* helperPlayer = headOfPlayers;
        while (helperPlayer->nextPlayer != NULL) {
            helperPlayer = helperPlayer->nextPlayer;
        }
        helperPlayer->nextPlayer = newPlayer;
    }
    headOfPlayersSize++;
}

void* connectionInstance(void* arg) {
    playersList* player = (playersList*)arg;
    int clientID = player->playerID;
    int clientfd = player->playerfd;
    //pthread_t* connectionThread = player->connectionThread;

    struct pollfd fd[1] = {
        {
            clientfd,
            POLLIN,
            0
        }
    };

    char clientBuffer[bufferSize] = { 0 };
    int recvID = 0;
    while(TRUE) { 
        if (player->playerID == -1) { // Close the client socket and the connectionInstance
            close(clientfd);
            joined--;
            break;
        }
        if (player->playerID == -2) { // Just close the connectionInstance
            break;
        }
        if(player->isStopped != 1) {
            int pollStatus = poll(fd, 1, 1000);
            if(pollStatus == 0) {
                continue;
            } else if (player->isStopped != 1 && fd[0].revents & POLLIN) {
                recvID = recv(clientfd, clientBuffer, bufferSize-1, 0);
                if (recvID <= 0) {
                    printf("client #%i: CLOSING THE CONNECTION\n", clientID);
                    removePlayer(clientID);
                    printf("Player #%i was removed\n", clientID);
                    close(clientfd);
                    break;
                  }
                printf("> client message (connectionInstance) #%i: %s", clientID, clientBuffer);
            }
        }
    }

}
int removePlayer(int playerfd) {
    sleep(1); // Important for synchronization with connectionInstance
    playersList* helperPlayer = headOfPlayers;
    if (headOfPlayers->playerfd == playerfd) {
        if (helperPlayer->nextPlayer == NULL) {
            free(headOfPlayers);
            headOfPlayers = NULL;
        } else {
            headOfPlayers = headOfPlayers->nextPlayer;
            free(helperPlayer);
        }
        headOfPlayersSize--;
        return 0;
    } else {
        if (headOfPlayers->nextPlayer == NULL) {
            return -1;
        }
        playersList* currentPlayer = headOfPlayers->nextPlayer;
        while (currentPlayer != NULL) {
            if (currentPlayer->playerfd == playerfd) {
                if (currentPlayer->nextPlayer != NULL) {
                    helperPlayer->nextPlayer = currentPlayer->nextPlayer;
                } else {
                    helperPlayer->nextPlayer = NULL;
                }
                headOfPlayersSize--;
                free(currentPlayer);
                return 0;
            }
            helperPlayer = helperPlayer->nextPlayer;
            currentPlayer = currentPlayer->nextPlayer;
        }
        return -1;
    }
}

int sendTheMap(playersList* player) {
    char mapSendBuffer[bufferSize] = { 0 };
    int sendID = 0;
    for(int b = 0; b<11; b++) {
            sprintf(mapSendBuffer, player->ships[b]);
            sendID = send(player->playerfd, mapSendBuffer, strlen(mapSendBuffer), 0);
            if (sendID < 0) {
                player->playerID = -1;
                removePlayer(player->playerfd);
                break;
            }
        }
    return 0;
}

void sendToAllPlayers(char* message) {
    playersList* helperPlayer = headOfPlayers;
    int sendID = 0;
    while(helperPlayer != NULL) {
        sendID = send(helperPlayer->playerfd, message, strlen(message), 0);
        if (sendID < 0) {
            int deletionFD = helperPlayer->playerfd;
            helperPlayer->playerID = -1;
            helperPlayer = helperPlayer->nextPlayer;
            removePlayer(deletionFD);
        } else {
            helperPlayer = helperPlayer->nextPlayer;
        }
    }
}

int attack(char* attackBuffer) {
    playersList* currentPlayer = headOfPlayers;
    while (currentPlayer != NULL) {
        if (currentPlayer->playerID == attackBuffer[0]-48) {
            if(currentPlayer->ships[attackBuffer[1]-47][(attackBuffer[2]-48)*2+2] == 'W') {
                    currentPlayer->ships[attackBuffer[1]-47][(attackBuffer[2]-48)*2+2] = 'X';
                    return 1;
                } else {
                    return 0;
                }
        } else {
            currentPlayer = currentPlayer->nextPlayer;
        }
    }
    return -1;
}

void* gameManager(void* args) {

    while(1) {
        printf("Waiting for players\n");
        sleep(5);
        if(headOfPlayersSize > 1) {
            char buffer[bufferSize] = { 0 };
            int recvID = 0;
            int sendID = 0;
            int round = 1;
            int localHelperRound = 1;
            int currentPlayerFD = 0;
            int currentPlayerID = 0;
            char attackBuffer[bufferSize] = { 0 };
            playersList* currentPlayer = headOfPlayers;
            playersList* helperPlayer = headOfPlayers;

            sprintf(buffer, "\\\\\\ 3...\n");
            sendToAllPlayers(buffer);;
            printf("\\\\\\ 3...\n");
            sleep(1);

            sprintf(buffer, "\\\\\\ 2...\n");
            sendToAllPlayers(buffer);
            printf("\\\\\\ 2..\n");
            sleep(1);

            sprintf(buffer, "\\\\\\ 1...\n");
            sendToAllPlayers(buffer);
            printf("\\\\\\ 1...\n");
            sleep(1);

            printf("\\\\\\ Starting the game (%i players) ///\n", headOfPlayersSize);
            sprintf(buffer, "\\\\\\ Starting the game (%i players) ///\n", headOfPlayersSize);
            sendToAllPlayers(buffer);

            gameStartedFlag = 1;

            while(TRUE) {
                currentPlayer->isStopped = 1;
                helperPlayer = headOfPlayers;
                while(helperPlayer != NULL) {
                    sendTheMap(helperPlayer);
                    helperPlayer = helperPlayer->nextPlayer;
                }
                helperPlayer = headOfPlayers;

                char tempBuffer[bufferSize] = { 0 };

                currentPlayerFD = currentPlayer->playerfd;
                currentPlayerID = currentPlayer->playerID;

                struct pollfd fd[1] = {
                    {
                        currentPlayer->playerfd,
                        POLLIN,
                        0
                    }
                };

                printf("\\\\\\ ROUND #%i: Player #%i turn ///\n", round, currentPlayerID);
                while(helperPlayer != NULL) {
                    sprintf(buffer, "\\\\\\ ROUND #%i: Player #%i turn (You are #%i) ///\n", round, currentPlayerID, helperPlayer->playerID);
                    sendID = send(helperPlayer->playerfd, buffer, sizeof(buffer)-1, 0);
                    if (sendID < 0) {
                        helperPlayer->playerfd = -1;
                        removePlayer(helperPlayer->playerfd);
                    }
                    helperPlayer = helperPlayer->nextPlayer;
                }
                helperPlayer = headOfPlayers;
                sleep(1);

                sprintf(buffer, "\\\\\\ Type the player number and the (y, x) coordinates (example:188)] \n");
                printf(buffer);
                sendID = send(currentPlayer->playerfd, buffer, sizeof(buffer)-1, 0);
                if (sendID < 0) {
                    currentPlayer->playerID = -1;
                    removePlayer(currentPlayerFD);
                } else {
                    int pollStatus = poll(fd, 1, 30000);
                    if(pollStatus == -1) {
                        perror("poll error");
                    }

                    if (fd[0].revents & POLLIN) {
                        recvID = recv(currentPlayerFD, attackBuffer, sizeof(attackBuffer), 0);
                        if (recvID == 0) {
                            currentPlayer->playerID = -1;
                            removePlayer(currentPlayerFD);
                            printf("---> Player #%i has disconnected! <---\n", currentPlayerID);
                            sprintf(buffer, "---> Player #%i has disconnected! <---\n", currentPlayerID);
                            sendToAllPlayers(buffer); 
                        } else if (recvID == -1) {
                            perror("recv error");
                        } else {
                            printf("Player %i message: %s\n", currentPlayerID, attackBuffer);
                            sprintf(buffer, "> Player #%i message: %s\n", currentPlayerID, attackBuffer);
                            sendToAllPlayers(buffer);
                            if ((attackBuffer[1] >= '0' && attackBuffer[1] <= '9') && (attackBuffer[2] >= '0' && attackBuffer[2] <= '9')) {
                                int attackID = attack(attackBuffer);
                                if (attackID == 1) {
                                    printf("---> HIT! <---\n");
                                    sprintf(buffer, "---> HIT! <---\n");
                                    sendToAllPlayers(buffer);

                                    while (helperPlayer->playerID != attackBuffer[0]-'0') {
                                        helperPlayer = helperPlayer->nextPlayer;
                                    }

                                    helperPlayer->fleetSize = helperPlayer->fleetSize-1;
                                    printf("fleetSize: %i\n", helperPlayer->fleetSize);
                                    if (round >= 15) {
                                        if (helperPlayer->fleetSize == 0) {
                                            helperPlayer->isStopped = 1;
                                            printf("---> Player #%i fleet was destroyed! <---\n", helperPlayer->playerID);
                                            sprintf(buffer, "---> Player #%i fleet was destroyed! <---\n", helperPlayer->playerID);
                                            sendToAllPlayers(buffer); 
                                            sleep(1);

                                            sprintf(buffer, "Would you like to play again? [Y/N]\n");
                                            sendID = send(helperPlayer->playerfd, buffer, sizeof(buffer), 0);
                                            if (sendID < 0) {
                                                helperPlayer->playerID = -1;
                                            } else {

                                                struct pollfd tempfd[1] = {
                                                    {
                                                        helperPlayer->playerfd,
                                                        POLLIN,
                                                        0
                                                    }
                                                };

                                                int tempPoll = poll(tempfd, 1, 15000);
                                                if (tempPoll > 0) {
                                                    if (fd[0].revents & POLLIN) {
                                                        recvID = recv(helperPlayer->playerfd, buffer, bufferSize-1, 0);
                                                        if (recvID == 0) {
                                                            helperPlayer->playerID = -1;
                                                        } else if (recvID == -1) {
                                                            perror("recv error");
                                                        } else {
                                                            if (buffer[0] == 'Y' || buffer[0] == 'y') {
                                                                helperPlayer->playerID = -2;
                                                                int* playerFD = (int*)malloc(sizeof(int));
                                                                *playerFD = helperPlayer->playerfd;
                                                                pthread_t* warshipChoosingThread = (pthread_t*)malloc(sizeof(pthread_t));
                                                                pthread_create(warshipChoosingThread, NULL, warshipChoosing, playerFD);
                                                                pthread_detach(*warshipChoosingThread);
                                                            } else {
                                                                helperPlayer->playerID = -1;
                                                            }
                                                        }
                                                    }
                                                } else {
                                                    helperPlayer->playerID = -1;
                                                }
                                            }
                                            removePlayer(helperPlayer->playerfd);
                                        }
                                    }
                                } else if(attackID == 0) {
                                    printf("---> MISS! <---\n");
                                    sprintf(buffer, "---> MISS! <---\n");
                                    sendToAllPlayers(buffer);
                                } else {
                                    printf("---> WRONG PLAYER NUMBER! <---\n");
                                    sprintf(buffer, "---> WRONG PLAYER NUMBER! <---\n");
                                    sendToAllPlayers(buffer);
                                }
                            } else {
                                printf("---> WRONG INPUT! <---\n");
                                sprintf(buffer, "---> WRONG INPUT! <---\n");
                                sendToAllPlayers(buffer);

                            }
                            currentPlayer->isStopped = 0;
                        }

                    } else {
                        printf("Timeout");
                        sprintf(buffer, "\\\\\\ Player #%i's time has expired ///\n", currentPlayerID);
                        sendToAllPlayers(buffer);
                        currentPlayer->isStopped = 0;
                    }
                }
                
                if(headOfPlayersSize <= 1) {
                    headOfPlayers->isStopped = 1;
                    printf("Winner!\n", headOfPlayers->playerID);
                    sprintf(buffer, "\\\\\\ Player #%i has won! ///\n", headOfPlayers->playerID);
                    sendID = send(headOfPlayers->playerfd, buffer, strlen(buffer), 0);
                    if (sendID < 0) {
                        headOfPlayers->playerID = -1;
                    } else {
                        sleep(1);
                        sprintf(buffer, "Would you like to play again? [Y/N]\n");
                        sendID = send(headOfPlayers->playerfd, buffer, strlen(buffer), 0);
                        if (sendID < 0) {
                            headOfPlayers->playerID = -1;
                        } else {

                            struct pollfd winnerfd[1] = {
                                {
                                    headOfPlayers->playerfd,
                                    POLLIN,
                                    0
                                }
                            };

                            int tempPoll2 = poll(winnerfd, 1, 15000);
                            if (tempPoll2 > 0) {
                                if(fd[0].revents & POLLIN) {
                                    recvID = recv(headOfPlayers->playerfd, buffer, bufferSize-1, 0);
                                    if (recvID == 0) {
                                        headOfPlayers->playerID = -1;
                                    } else if (recvID == -1) {
                                        perror("recv error");
                                    } else {
                                        if(buffer[0] == 'Y' || buffer[0] == 'y') {
                                            headOfPlayers->playerID = -2;
                                            int* playerFD2 = (int*)malloc(sizeof(int));
                                            *playerFD2 = headOfPlayers->playerfd;
                                            pthread_t* warshipChoosingThread2 = (pthread_t*)malloc(sizeof(pthread_t));
                                            pthread_create(warshipChoosingThread2, NULL, warshipChoosing, playerFD2);
                                            pthread_detach(*warshipChoosingThread2);
                                        } else {
                                            headOfPlayers->playerID = -1;
                                        }
                                    }
                                }
                            } else {
                                headOfPlayers->playerID = -1;
                            }
                        }
                    }
                    lastPlayerID = 1;
                    removePlayer(headOfPlayers->playerfd);
                    gameStartedFlag = 0;
                    break;
                }

                if (localHelperRound % headOfPlayersSize == 0) {
                    currentPlayer = headOfPlayers;
                    localHelperRound = 1;
                } else {
                    currentPlayer = currentPlayer->nextPlayer;
                    localHelperRound++;
                }
                round++;
            }
        }
    }
    pthread_create(&gameManagerThread, NULL, gameManager, NULL);
    pthread_detach(gameManagerThread);
}
