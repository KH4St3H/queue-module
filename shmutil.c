#include <fcntl.h>
#include<stdio.h>
#include<sys/ipc.h>
#include<sys/shm.h>
#include<sys/types.h>
#include<string.h>
#include<stdlib.h>
#include <sys/wait.h>
#include<unistd.h>
#include<string.h>

#include "shmutil.h"

#define SHM_KEY 0x1234

struct shmseg {
   int cnt;
   int complete;
   char buf[BUF_SIZE];
};

int create_shm(){
    int shmid;
    struct shmseg *shmp;
    shmid = shmget(SHM_KEY, sizeof(struct shmseg), 0644|IPC_CREAT);
    if (shmid == -1) {
        perror("Shared memory");
        return -1;
    }

    return shmid;
}

int remove_shm(int shmid){
    if (shmctl(shmid, IPC_RMID, 0) == -1) {
        perror("shmctl");
        return 1;
    }
    return 0;
}

int write_data(int shmid, char *data){
    // Attach SHM
    struct shmseg *shmp;
    shmp = shmat(shmid, NULL, 0);
    if (shmp == (void *) -1) {
        perror("Shared memory attach");
        return 1;
    }
    char *bufptr;
    bufptr = shmp->buf;
    shmp->complete = 0;
    memcpy(bufptr, data, strlen(data));
    shmp->cnt = strlen(data);
    shmp->complete = 1;

    // Detach SHM
    if (shmdt(shmp) == -1) {
        perror("shmdt");
        return 1;
    }
    printf("Writing Process: Complete\n");
    return 0;
}

int read_data(char *data, int shmid){
    // Attach SHM
    struct shmseg *shmp;
    shmp = shmat(shmid, NULL, 0);
    if (shmp == (void *) -1) {
        perror("Shared memory attach\n");
        return -1;
    }
    char *bufptr;
    bufptr = shmp->buf;
    if(shmp->complete != 1)
        return -1;

    memcpy(data, shmp->buf, shmp->cnt);
    data[shmp->cnt] = '\0';

    // Detach SHM
    if (shmdt(shmp) == -1) {
        perror("shmdt");
        return -1;
    }
    printf("Reading Process: Complete\n");
    return 0;
}
