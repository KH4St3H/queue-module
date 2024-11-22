#include <arpa/inet.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <netinet/in.h>
#include <stdlib.h>
#include<sys/shm.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>

#include "shmutil.h"

#define BUFFERSIZE 1024
#define DEVICE "/dev/myQueue"

int shmid;

void writeToQueue(char *data);

void process1();
void process2();
int process3();

double epoch_double(struct timespec *tv);
long int epoch_millis();

int main()
{
    int parent = getpid();
    long int *arr;
    
    int key = ftok(".", 34);
    int times_shmid = shmget(key,sizeof(long int)*6,0666|IPC_CREAT);

    fork();
    
    if (getpid() != parent){
        process1();
        arr = (long int *)shmat(times_shmid, NULL, 0);
        arr[0] = getpid();
        arr[1] = epoch_millis();
        shmdt((void *) arr);

        exit(0);
    }
    fork();
    
    int pid = getpid();
    if (pid != parent){
        process2();
        arr = (long int *)shmat(times_shmid, NULL, 0);
        arr[2] = getpid();
        arr[3] = epoch_millis();
        shmdt((void *) arr);
        exit(0);
    }

    int status;
    arr = (long int *)shmat(times_shmid, NULL, 0);

    waitpid(arr[0], &status, 0);

    printf("wait time for process 1: %ldms\n", (epoch_millis() - arr[1]));

    shmid = create_shm();
    fork();
    pid = getpid();
    if(pid != parent){
        int success = process3();

        arr = (long int *)shmat(times_shmid, NULL, 0);
        arr[4] = getpid();
        arr[5] = epoch_millis();
        shmdt((void *) arr);
        if(success == 0){
            printf("Process 3 has completed successfully\n");
            exit(0);
        } else {
            perror("Process 3 has failed!\n");
            exit(1);
        }
    }
    int donepid = wait(NULL);
    if (donepid == arr[2])
        printf("wait time for process 2: %ldms\n", (epoch_millis() - arr[3]));
    else 
        printf("wait time for process 3: %ldms\n", (epoch_millis() - arr[5]));

    donepid = wait(NULL);
    if (donepid == arr[2])
        printf("wait time for process 2: %ldms\n", (epoch_millis() - arr[3]));
    else
        printf("wait time for process 3: %ldms\n", (epoch_millis() - arr[5]));

    shmdt((void *) arr);
    shmctl(times_shmid, IPC_RMID, NULL);

    char data[BUF_SIZE];
    read_data(data, shmid);
    printf("Read \"%s\" from shared memory(SHM)\n", data);

    remove_shm(shmid);
}

char data[BUF_SIZE];

int process3(){
    int fd;
    fd = open(DEVICE, O_RDWR); 

    int success;

    int i = 0;
    while(1){
        success = read(fd, data+i, 1);
        if(success < 1)
            break;
        i++;
    }
    data[i+1] = '\n';

    printf("read '%s' from queue\n", data);

    write_data(shmid, data);
    return 0;
}

void process2(){
 
    int server_fd, new_socket;
    struct sockaddr_in address;

    int opt = 1;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_port = htons(54321);

    if (inet_pton(AF_INET, "127.0.0.1", &address.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        exit(EXIT_FAILURE);
    }


    if (connect(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        printf("\nConnection Failed \n");
        exit(EXIT_FAILURE);
    }

    printf("receiving data\n");
    char buffer[BUFFERSIZE] = {0};
    read(server_fd, buffer, BUFFERSIZE);
    printf("received data: %s\n", buffer);
    writeToQueue(buffer);
}


void process1(){
    char line[1024];
    int c = 0;

    int server_fd, new_socket;
    struct sockaddr_in address;

    int opt = 1;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // 0.0.0.0
    address.sin_port = htons(54321);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    scanf("%1023[^\n]", line);
    addrlen = sizeof(address);

    printf("accepting new connections\n");
    new_socket = accept(server_fd, (struct sockaddr*)&address, &addrlen);
    send(new_socket, line, strlen(line), 0);
}

void writeToQueue(char *data){
    int fd;
    fd = open(DEVICE, O_RDWR);
    int success;
    int count = strlen(data);
    for(int i=0; i<count; i++){
        success = write(fd, data, 1);
        // printf("data: %s\tstrlen: %ld\tsuccess: %d\n", data, strlen(data), success);
        if(success < 0){
            printf("Failed like a dog!\n");
            break;
        }
        if(success != 0)
            data++;
    }
}

double epoch_double(struct timespec *tv)
{
    clock_gettime(CLOCK_REALTIME, tv);
    char time_str[32];

    sprintf(time_str, "%ld.%.9ld", tv->tv_sec, tv->tv_nsec);

    return atof(time_str);
}

long int epoch_millis()
{
    double epoch;
    struct timespec tv;
    epoch = epoch_double(&tv);
    epoch = round(epoch*1e3);

    return (long int) epoch;
}