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

typedef struct helperStruct {
    int playerID;
    int socketfd;
    pthread_t* connectionThread;
} helperStruct;

typedef struct threadsList {
    pthread_t* connectionThread;
    struct threadsList* nextThread;
} threadsList;

typedef struct playersList {
    int playerID;
    int playerfd;
    int isStopped;
    char fleetSize;
    char ships[11][24];
    struct playersList* nextPlayer;
} playersList;

int playerTurnFlagID = -1;
threadsList* threads = NULL;
playersList* players = NULL;
int playersListSize = 0;
int socketfd = -1;
short terminationFlag = 0;
char gameStartedFlag = 0;

int removeThread(pthread_t* connectionThread) {
    threadsList* headThread = threads;
    if((*headThread).connectionThread == connectionThread) {
        headThread = (*headThread).nextThread;
        free(headThread);
        return 0;
    } else {
        if((*headThread).nextThread == NULL) {
            return -1;
        }
        threadsList* currentThread = (*headThread).nextThread;
        threadsList* helperThread = headThread;
        for(int i = 0; i<playersListSize-1; i++) {
            if((*currentThread).connectionThread == connectionThread) {
                if((*currentThread).nextThread != NULL) {
                    (*currentThread).nextThread = (*currentThread).nextThread;
                    return 0;
                } else {
                    (*helperThread).nextThread = NULL;
                    return 0;
                }
            }
        }
        return -1;
    }
}

playersList* headPlayer = NULL;
threadsList* headThread = NULL;

int removePlayer(int playerID) {
    playersList* headPlayerLocal = players;
    if(playersListSize == 1) {
        headPlayer = NULL;
        headThread = NULL;
        playersListSize--;
        return 0;
    }
    if(players->playerID == playerID) {
        players = players->nextPlayer;
        playersListSize--;
        return 0;
    } else {
        if((*headPlayerLocal).nextPlayer == NULL) {
            return -1;
        }
        playersList* currentPlayer = (*headPlayerLocal).nextPlayer;
        playersList* helperPlayer = headPlayerLocal;
        while (currentPlayer != NULL) {
            if((*currentPlayer).playerID = playerID) {
                if((*currentPlayer).nextPlayer != NULL) {
                    (*helperPlayer).nextPlayer = (*currentPlayer).nextPlayer;
                    playersListSize--;
                } else {
                    (*helperPlayer).nextPlayer = NULL;
                    playersListSize--;
                }
                return 0;
            }
            helperPlayer = helperPlayer->nextPlayer;
            currentPlayer = currentPlayer->nextPlayer;
        }
        return -1;
    }
}


void* connectionInstance(void* arg) {
    helperStruct* helpStruct = (helperStruct*)arg;
    int clientID = (*helpStruct).playerID;
    int clientfd = (*helpStruct).socketfd;
    pthread_t* connectionThread = (*helpStruct).connectionThread;
    free(helpStruct);
    playersList* helperPlayer = players;
    while(helperPlayer->playerfd != clientfd) {
        helperPlayer = helperPlayer -> nextPlayer;
    }

    struct pollfd fds[2] = {
        {
            STDIN_FILENO,
            POLLIN,
            0
        },
        {
            clientfd,
            POLLIN,
            0
        }
    };

    char clientBuffer[bufferSize] = { 0 };
    for (;;) {
        if(helperPlayer != NULL && helperPlayer->fleetSize != 0){
            if(helperPlayer->isStopped != 1) {
                if(playerTurnFlagID != clientfd) {
                    int pollStatus = poll(fds, 2, 1000);
                    if(pollStatus == 0) {
                        continue;
                    } else if (fds[0].revents & POLLIN) {
                        read(0, clientBuffer, bufferSize-1);
                        send(clientfd, clientBuffer, bufferSize, 0);
                    } else if (fds[1].revents & POLLIN) {
                        if (recv(clientfd, clientBuffer, bufferSize-1, 0) == 0 ) {
                            printf("client #%i: CLOSING THE CONNECTION\n", clientID);
                            send(clientfd, clientBuffer, 0, 0);
                            removePlayer(clientID);
                            printf("Player #%i was removed\n", clientID);
                            close(clientfd);
                            break;
                        }
                        printf("> client message (instance) #%i: %s", clientID, clientBuffer);
                    } else {
                        if(read(0, clientBuffer, bufferSize-1) < 0) {
                            break;
                        }
                        printf("SOME ERROR\n");
                    }
                }
            }
        } else {
            break;
        }
    }
    removeThread(connectionThread);
}

int tempPlayerID = 0;


void* warshipChoosing(void* arg) {
    int* args = (int*)arg;
    char tempBuffer[512] = { 0 };
    int clientfd = *args;
    free(args);

    threadsList* newThread = (threadsList*)malloc(sizeof(threadsList));
    playersList* newPlayer = (playersList*)malloc(sizeof(playersList));
    newPlayer->playerfd = clientfd;
    newPlayer->isStopped = 0;

    sprintf(tempBuffer, "-Next you will choose your warships...\n\n");
    send(clientfd, tempBuffer, sizeof(tempBuffer), 0);
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
    
    char shipBuildBuffer[bufferSize] = { 0 };
    //char Choose[bufferSize] = { 0 };
    int response = -1;
    //memset(tempBuffer, 0, sizeof(tempBuffer));

    for(int i = 0; i<15; i++) {
        //memset(tempBuffer, 0, sizeof(tempBuffer));
        for(int b = 0; b<11; b++) {
            snprintf(tempBuffer, 24, "%s", map[b]);
            send(clientfd, tempBuffer, strlen(tempBuffer), 0);
        }
        //memset(tempBuffer, 0, sizeof(tempBuffer));

        memset(tempBuffer, 0, sizeof(tempBuffer));
        sprintf(tempBuffer, "%i/15: Choose your next point [Example: 57 for (7, 5)]\n", i+1);
        send(clientfd, tempBuffer, strlen(tempBuffer), 0);
        
        response = recv(clientfd, shipBuildBuffer, sizeof(shipBuildBuffer), 0);

        if(response == -1) {
            perror("response error line ~200");
            break;
        }

        if((shipBuildBuffer[0] >= '0' && shipBuildBuffer[0] <= '9') && (shipBuildBuffer[1] >= '0' && shipBuildBuffer[1] <= '9')) {
            if(map[shipBuildBuffer[0]-47][(shipBuildBuffer[1]-48)*2+2] != 'W') {
                map[shipBuildBuffer[0]-47][(shipBuildBuffer[1]-48)*2+2] = 'W';
            } else {
                sprintf(tempBuffer, "%s", "This point is already chosen!\n");
                send(clientfd, tempBuffer, strlen(tempBuffer), 0);
                i = i-1;
            }
        } else {
            sprintf(tempBuffer, "%s", "Wrong input!\n");
            send(clientfd, tempBuffer, strlen(tempBuffer), 0);
            i = i-1;
        }
    }

    for(int b = 0; b<11; b++) {
            //memset(tempBuffer, 0, sizeof(tempBuffer));
            snprintf(tempBuffer, 24, "%s", map[b]);
            send(clientfd, tempBuffer, strlen(tempBuffer), 0);
        }
        
    for(int i = 0; i<11; i++) {
        for(int j = 0; j<24; j++) {
            newPlayer->ships[i][j] = map[i][j];
        }
    }

    newPlayer->fleetSize = 15;
    

    //memset(Choose, 0, sizeof(Choose));
    sprintf(tempBuffer, "\nGreat! now you can wait for other players...\n");
    send(clientfd, tempBuffer, sizeof(tempBuffer), 0);
    //memset(Choose, 0, sizeof(Choose));
    if(gameStartedFlag == 1) {
        sprintf(tempBuffer, "\nThe game has already started, wait for its end...\n");
        send(clientfd, tempBuffer, sizeof(tempBuffer), 0);
    }
    while(gameStartedFlag == 1) {
        sleep(1);
    }

    newPlayer->playerID = tempPlayerID;
    newPlayer->nextPlayer = NULL;
    tempPlayerID++;
    if(headPlayer == NULL) {
        headPlayer = newPlayer;
        players = headPlayer;
    } else {
        headPlayer->nextPlayer = newPlayer;
        headPlayer = headPlayer->nextPlayer;
    }

    printf("A new player has connected: #%i\n", (*newPlayer).playerID);
    sleep(1);
    sprintf(tempBuffer, "{You are player #%i}\n", (*newPlayer).playerID);
    send(clientfd, tempBuffer, sizeof(tempBuffer), 0);

    playersListSize++;
        
    pthread_t* newConnectionThread = (pthread_t*)malloc(sizeof(pthread_t));
    helperStruct* helpStruct = (helperStruct*)malloc(sizeof(helperStruct));
    helpStruct->playerID = newPlayer->playerID; helpStruct->socketfd = clientfd; helpStruct->connectionThread = newConnectionThread;

    pthread_create(newConnectionThread, NULL, connectionInstance, helpStruct);
    newThread->connectionThread = newConnectionThread;
    pthread_detach(*newConnectionThread);


    if(headThread == NULL) {
        headThread = newThread;
        threads = headThread;
    } else {
        headThread->nextThread = newThread;
        headThread = headThread->nextThread;
    }
;
}


void* instancesManager(void* arg) {
    int joined = 0;
    int* clientfd = NULL;
    pthread_t* warshipChoosingThread = NULL;
    for(;;) {
        if(joined == 4) {
            printf("4 joined\n");
            sleep(15);
            if(gameStartedFlag == 0) {
                joined = 0;
            }
        }
        clientfd = (int*)malloc(sizeof(int));
        *clientfd = accept(socketfd, 0, 0);
        if(*clientfd == -1) {
            perror("accept error");
        }
        joined++;

        warshipChoosingThread = (pthread_t*)malloc(sizeof(pthread_t));
        pthread_create(warshipChoosingThread, NULL, warshipChoosing, clientfd);
        pthread_detach(*warshipChoosingThread);
        
        if(playersListSize == 4) {
            printf("The server is full\n");
            while(playersListSize == 4) {
                sleep(1);
            }
        }
    }
}

char attackBuffer[512] = { 0 }; // 0 - playerNumber, 1 - y, 2 - x.

int attack() {
    playersList* currentPlayer = players;
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

int sendTheMap(playersList* player) {
    char mapSendBuffer[bufferSize] = { 0 };
    for(int b = 0; b<11; b++) {
            sprintf(mapSendBuffer, player->ships[b]);
            send(player->playerfd, mapSendBuffer, strlen(mapSendBuffer), 0);
        }
    memset(mapSendBuffer, 0, sizeof(mapSendBuffer));
    return 0;
}

void* gameManager(void* args) {

    while(1) {
        printf("Waiting for players\n");
        sleep(5);
        if(playersListSize > 1) {
            char buffer[bufferSize] = { 0 };
            int round = 0;
            int playerNumber = 0;
            int playerID = 0;
            playersList* currentPlayer = 0;
            playersList* helperPlayer = 0;
            currentPlayer = players;
            helperPlayer = players;

            printf("\\\\\\ Starting the game (%i players) ///\n", playersListSize);
            sprintf(buffer, "\\\\\\ Starting the game (%i players) ///\n", playersListSize);
            while(helperPlayer != NULL) {
                send(helperPlayer->playerfd, buffer, sizeof(buffer)-1, 0);
                helperPlayer = helperPlayer->nextPlayer;
            }
            helperPlayer = players;
  
            sleep(1);
            sprintf(buffer, "\\\\\\ 3...\n");
            while(helperPlayer != NULL) {
                send(helperPlayer->playerfd, buffer, sizeof(buffer)-1, 0);
                helperPlayer = helperPlayer->nextPlayer;
            }
            helperPlayer = players;
            printf("\\\\\\ 3...\n");

            sleep(1);
            sprintf(buffer, "\\\\\\ 2...\n");
            while(helperPlayer != NULL) {
                send(helperPlayer->playerfd, buffer, sizeof(buffer)-1, 0);
                helperPlayer = helperPlayer->nextPlayer;
            }
            helperPlayer = players;
            printf("\\\\\\ 2..\n");
            
            sleep(1);
            sprintf(buffer, "\\\\\\ 1...\n");
            while(helperPlayer != NULL) {
                send(helperPlayer->playerfd, buffer, sizeof(buffer)-1, 0);
                helperPlayer = helperPlayer->nextPlayer;
            }
            helperPlayer = players;
            printf("\\\\\\ 1...\n");
            
            sleep(1);
            gameStartedFlag = 1;

            for(;;) {
                helperPlayer = players;
                while(helperPlayer != NULL) {
                    sendTheMap(helperPlayer);
                    helperPlayer = helperPlayer->nextPlayer;
                }
                helperPlayer = players;

                char tempBuffer[bufferSize] = { 0 };

                playerNumber = (*currentPlayer).playerfd;
                playerID = (*currentPlayer).playerID;
                struct pollfd fd[1] = {
                    {
                        (*currentPlayer).playerfd,
                        POLLIN,
                        0
                    }
                };
                playerTurnFlagID = playerNumber;
                printf("\\\\\\ ROUND #%i: Player #%i turn ///\n", round, playerID);
                while(helperPlayer != NULL) {
                    sprintf(buffer, "\\\\\\ ROUND #%i: Player #%i turn (You are #%i) ///\n", round, playerID, helperPlayer->playerID);
                    send(helperPlayer->playerfd, buffer, sizeof(buffer)-1, 0);
                    helperPlayer = helperPlayer->nextPlayer;
                }
                helperPlayer = players;

                sleep(1);
                sprintf(buffer, "\\\\\\ Type the player number and the (y, x) coordinates (example:188)] \n");
                printf(buffer);
                while(helperPlayer != NULL) {
                    send(helperPlayer->playerfd, buffer, sizeof(buffer)-1, 0);
                    helperPlayer = helperPlayer->nextPlayer;
                }
                helperPlayer = players;
                
                currentPlayer->isStopped = 1;
                int pollStatus = poll(fd, 1, 30000);
                if(pollStatus == -1) {
                    perror("poll error");
                }

                if (fd[0].revents & POLLIN) {
                    int recvID = recv(playerNumber, attackBuffer, sizeof(attackBuffer), 0);
                    if(recvID == -1) {
                        perror("recv error");
                    }
                    if((attackBuffer[1] >= '0' && attackBuffer[1] <= '9') && (attackBuffer[2] >= '0' && attackBuffer[2] <= '9')) {
                        int attackID = attack();
                        if(attackID == 1) {
                            while(helperPlayer->playerID != attackBuffer[0]-48) {
                                    helperPlayer = helperPlayer->nextPlayer;
                                }
                            helperPlayer->fleetSize = helperPlayer->fleetSize-1;
                            printf("fleetSize: %i\n", helperPlayer->fleetSize);
                            if(round >= 14) {
                                if(helperPlayer->fleetSize == 0) {
                                    helperPlayer->isStopped = 1;
                                    printf("---> Player #%i fleet was destroyed! <---\n", helperPlayer->playerID);
                                    sprintf(buffer, "---> Player #%i fleet was destroyed! <---\n", helperPlayer->playerID);
                                    helperPlayer = players;
                                    while(helperPlayer != NULL) {
                                        send(helperPlayer->playerfd, buffer, sizeof(buffer)-1, 0);
                                        helperPlayer = helperPlayer->nextPlayer;
                                    }
                                    helperPlayer = players;
                                    while(helperPlayer->playerID != attackBuffer[0]-48 && helperPlayer->nextPlayer != NULL) {
                                        helperPlayer = helperPlayer->nextPlayer;
                                    }
                                    
                                    sleep(1);
                                    sprintf(buffer, "Would you like to play again? [Y/N]\n");
                                    send(helperPlayer->playerfd, buffer, sizeof(buffer), 0);
                                    struct pollfd tempfd[1] = {
                                        {
                                            helperPlayer->playerfd,
                                            POLLIN,
                                            0
                                        }
                                    };
                                    int tempPoll = poll(tempfd, 1, 15000);
                                    if (tempPoll != 0 && tempPoll != -1) {
                                        if(fd[0].revents & POLLIN) {
                                            recv(helperPlayer->playerfd, buffer, bufferSize-1, 0);
                                            if(buffer[0] == 'Y' || buffer[0] == 'y') {
                                                int* playerFD = (int*)malloc(sizeof(int));
                                                *playerFD = helperPlayer->playerfd;
                                                pthread_t* warshipChoosingThread = (pthread_t*)malloc(sizeof(pthread_t));
                                                pthread_create(warshipChoosingThread, NULL, warshipChoosing, playerFD);
                                                pthread_detach(*warshipChoosingThread);
                                            } else {
                                                helperPlayer->isStopped = 0;
                                                close(helperPlayer->playerfd);
                                            }
                                        }
                                    }
                                    
                                    helperPlayer->isStopped = 0;
                                    removePlayer(attackBuffer[0]-48);
                                    helperPlayer = players;
                                }
                            }
                            helperPlayer = players;
                            printf("---> HIT! <---\n");
                            sprintf(buffer, "---> HIT! <---\n");
                            while(helperPlayer != NULL) {
                                send(helperPlayer->playerfd, buffer, strlen(buffer), 0);
                                helperPlayer = helperPlayer->nextPlayer;
                            }
                            helperPlayer = players;

                        } else if(attackID == 0) {
                            printf("---> MISS! <---\n");
                            sprintf(buffer, "---> MISS! <---\n");
                            while(helperPlayer != NULL) {
                                send(helperPlayer->playerfd, buffer, strlen(buffer), 0);
                                helperPlayer = helperPlayer->nextPlayer;
                            }
                            helperPlayer = players;
                        } else {
                            printf("---> WRONG PLAYER NUMBER! <---\n");
                            sprintf(buffer, "---> WRONG PLAYER NUMBER! <---\n");
                            while(helperPlayer != NULL) {
                                send(helperPlayer->playerfd, buffer, strlen(buffer), 0);
                                helperPlayer = helperPlayer->nextPlayer;
                            }
                            helperPlayer = players;

                        }
                    } else {
                        printf("---> WRONG INPUT! <---\n");
                        sprintf(buffer, "---> WRONG INPUT! <---\n");
                        while(helperPlayer != NULL) {
                            send(helperPlayer->playerfd, buffer, strlen(buffer), 0);
                            helperPlayer = helperPlayer->nextPlayer;
                        }
                        helperPlayer = players;

                    }
                    printf("Player %i message: %s\n", playerID, attackBuffer);
                    sprintf(buffer, "> Player #%i message: %s\n", playerID, attackBuffer);
                    while(helperPlayer != NULL) {
                        send(helperPlayer->playerfd, buffer, strlen(buffer), 0);
                        helperPlayer = helperPlayer->nextPlayer;
                    }
                    helperPlayer = players;

                } else {
                    printf("Timeout");
                    sprintf(buffer, "\\\\\\ Player #%i's time has expired ///\n", playerID);
                    while(helperPlayer != NULL) {
                        send(helperPlayer->playerfd, buffer, sizeof(buffer)-1, 0);
                        helperPlayer = helperPlayer->nextPlayer;
                    }
                    helperPlayer = players;
                }
                currentPlayer->isStopped = 0;

                if(round>=14 && playersListSize == 1) {
                    players->isStopped = 1;
                    playerTurnFlagID = -1;
                    gameStartedFlag = 0;
                    printf("Winner!\n", players->playerID);
                    sprintf(buffer, "\\\\\\ Player #%i has won! ///\n", players->playerID);
                    send(players->playerfd, buffer, strlen(buffer), 0);
                    tempPlayerID = 0;
                    /*while(helperPlayer != NULL) {
                        send(helperPlayer->playerfd, buffer, sizeof(buffer)-1, 0);
                        helperPlayer = helperPlayer->nextPlayer;
                    }
                    helperPlayer = players; */
                    sleep(1);
                    sprintf(buffer, "Would you like to play again? [Y/N]\n");
                    send(players->playerfd, buffer, strlen(buffer), 0);
                    struct pollfd tempfd2[1] = {
                        {
                            players->playerfd,
                            POLLIN,
                            0
                        }
                    };
                    int tempPoll2 = poll(tempfd2, 1, 15000);
                    if (tempPoll2 != 0 && tempPoll2 != -1) {
                        if(fd[0].revents & POLLIN) {
                            recv(players->playerfd, buffer, bufferSize-1, 0);
                            if(buffer[0] == 'Y' || buffer[0] == 'y') {
                                int* playerFD2 = (int*)malloc(sizeof(int));
                                *playerFD2 = players->playerfd;
                                pthread_t* warshipChoosingThread2 = (pthread_t*)malloc(sizeof(pthread_t));
                                pthread_create(warshipChoosingThread2, NULL, warshipChoosing, playerFD2);
                                pthread_detach(*warshipChoosingThread2);
                            } else {
                                players->isStopped = 0;
                                close(players->playerfd);
                            }
                        }
                    }
                    removePlayer(players->playerID);
                    break;
                }


                round++;

                if(round % playersListSize == 0) {
                    currentPlayer = players;
                } else {
                    currentPlayer = (*currentPlayer).nextPlayer;
                }
            }
        }
    }
    pthread_t gameManagerThread = 0;
    pthread_create(&gameManagerThread, NULL, gameManager, NULL);
    pthread_detach(gameManagerThread);
}

int main() {
    socketfd = socket(AF_INET, SOCK_STREAM, 0);
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

    int bnd = bind(socketfd, (struct sockaddr *)&address, sizeof(address));
    if(bnd == -1) {
        perror("binding error");
        close(socketfd);
        return -1;
    }
    printf("bind: %i\n", bnd);

    int lis = listen(socketfd, 4);
    if(lis == -1) {
        perror("listening error");
        close(socketfd);
        return -1;
    }
    printf("listen: %i\n", lis);;

    pthread_t instancesManagerThread = 0;
    pthread_create(&instancesManagerThread, NULL, instancesManager, NULL);
    pthread_detach(instancesManagerThread);

    pthread_t gameManagerThread = 0;
    pthread_create(&gameManagerThread, NULL, gameManager, NULL);
    pthread_detach(gameManagerThread);

    struct pollfd fd[1] = {
        {
            STDIN_FILENO,
            POLLIN,
            0
        },
    };

    char buffer[5] = { 0 };
    
    for(;;) {
        poll(fd, 1, 2000);
        if(terminationFlag == 1) {
            close(socketfd);
            pthread_cancel(gameManagerThread);
            break;
        } else if(fd[0].revents & POLLIN) {
            read(0, buffer, 4);
            if(buffer[0] == 'S' && buffer[1] == 'T' && buffer[2] == 'O' && buffer[3] == 'P') {
                playersList* currentPlayer = players;
                for(int i = 0; i<playersListSize; i++) {
                    close(currentPlayer->playerfd);
                    currentPlayer = currentPlayer->nextPlayer;
                    printf("client #%i: CLOSING THE CONNECTION (Server shutdown)\n", currentPlayer->playerfd);
                }
                threadsList* threadHead = threads;
                for(int i = 0; i<playersListSize; i++) {
                    removeThread(threadHead->connectionThread);
                    threadHead = threadHead->nextThread;
                }

                pthread_cancel(instancesManagerThread);
                pthread_cancel(gameManagerThread);
                break;
            }
        }
    }
    close(socketfd);
    printf("THE SERVER HAS BEEN CLOSED\n");
    return 0;
}
