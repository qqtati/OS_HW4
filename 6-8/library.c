#include <stdio.h>     
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>   
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#define MAXPENDING 5

pthread_mutex_t mutex;
int *books;
int books_count;
char message[32];
int multicast_sock;
struct sockaddr_in multicastAddr;

typedef struct thread_args {
    int socket;
} thread_args;

void DieWithError(char *errorMessage)
{
    perror(errorMessage);
    exit(1);
}

void *clientThread(void *args) {
    int server_socket;
    int client_length;
    pthread_t threadId;
    struct sockaddr_in client_addr;
    pthread_detach(pthread_self());
    server_socket = ((thread_args*)args)->socket;
    free(args);
    client_length = sizeof(client_addr);
    int buffer[2];
    for (;;) {
        recvfrom(server_socket, buffer, sizeof(buffer), 0, (struct sockaddr*) &client_addr, &client_length);
        pthread_mutex_lock(&mutex);
        if (buffer[0] == 1) {
            if (books[buffer[1]] == 0) {
                printf("Giving book %d to reader\n", buffer[1]);
                snprintf(message, sizeof(message), "Giving book %d to reader\n", buffer[1]);
                sendto(multicast_sock, message, sizeof(message), 0, (struct sockaddr*) &multicastAddr, sizeof(multicastAddr));
                books[buffer[1]] = 1;
                buffer[0] = 1;
            } else {
                snprintf(message, sizeof(message), "Waiting for book #%d\n", buffer[1]);
                sendto(multicast_sock, message, sizeof(message), 0, (struct sockaddr*) &multicastAddr, sizeof(multicastAddr));
                buffer[0] = 2;
            }
        } else {
            snprintf(message, sizeof(message), "Got returned book #%d\n", buffer[1]);
            sendto(multicast_sock, message, sizeof(message), 0, (struct sockaddr*) &multicastAddr, sizeof(multicastAddr));
            printf("Got returned book #%d\n", buffer[1]);
            books[buffer[1]] = 0;
            buffer[0] = 3;
        }
        pthread_mutex_unlock(&mutex);
        sendto(server_socket, buffer, sizeof(buffer), 0, (struct sockaddr*) &client_addr, sizeof(client_addr));
    }
}

int createTCPSocket(unsigned short server_port) {
    int server_socket;
    struct sockaddr_in server_addr;

    if ((server_socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) DieWithError("socket() failed");
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;              
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); 
    server_addr.sin_port = htons(server_port);

    if (bind(server_socket, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) DieWithError("bind() failed");
    printf("Open socket on %s:%d\n", inet_ntoa(server_addr.sin_addr), server_port);
    return server_socket;
}

int main(int argc, char *argv[])
{
    unsigned short server_port;
    int server_socket;
    pthread_t threadId;
    pthread_mutex_init(&mutex, NULL);
    char *multicastIP;
    unsigned short multicastPort;
    if (argc != 5)
    {
        fprintf(stderr, "Usage:  %s <Mulicast addr> <Multicast port> <Port for clients> <Books count>\n", argv[0]);
        exit(1);
    }

    multicastIP = argv[1];
    multicastPort = atoi(argv[2]);
    server_port = atoi(argv[3]);
    books_count = atoi(argv[4]);
    books = (int*) malloc(sizeof(int) * books_count);
    server_socket = createTCPSocket(server_port);

    if ((multicast_sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        DieWithError("socket() failed");

   
    int multicastTTL = 1;
    if (setsockopt(multicast_sock, IPPROTO_IP, IP_MULTICAST_TTL, (void *) &multicastTTL, 
          sizeof(multicastTTL)) < 0)
        DieWithError("setsockopt() failed");

    memset(&multicastAddr, 0, sizeof(multicastAddr));   
    multicastAddr.sin_family = AF_INET;                 
    multicastAddr.sin_addr.s_addr = inet_addr(multicastIP);
    multicastAddr.sin_port = htons(multicastPort); 


    printf("Open multicast socket on %s:%d\n", inet_ntoa(multicastAddr.sin_addr), multicastPort);

    for (int i = 0; i < MAXPENDING; i++) {
        thread_args *args = (thread_args*) malloc(sizeof(thread_args));
        args->socket = server_socket;
        if (pthread_create(&threadId, NULL, clientThread, (void*) args) != 0) DieWithError("pthread_create() failed");
    }
    for (;;) {
        sleep(1);
    }
    free(books);
    pthread_mutex_destroy(&mutex);
    return 0;
}