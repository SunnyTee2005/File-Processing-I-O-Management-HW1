#include <fcntl.h> // for O_RDONLY, O_CREAT
#include <unistd.h> 
#include <sys/types.h> // lseek()
#include <sys/time.h> // gettimeofday()
#include <stdlib.h> // atoi()
#include <stdio.h>
#include <string.h> // memset()
#include <time.h> // time()
#define FILE_SIZE (100 * 1024 * 1024) // 100MB
#define TWO_KB (2 * 1024)
#define FOUR_KB (4 * 1024)


/*Read the file sequentially from beginning to end.
Read 4KB of data per operation.
Continue until the entire 100MB file has been read.*/
void seq_read(){
    int fd = open("test.txt", O_RDONLY);
    if(fd == -1){
        printf("Error: Failed to open \"test.txt\" \n");
        return;
    }
    char buffer[FOUR_KB];
    struct timeval start, end;

    gettimeofday(&start, NULL);
    for(int i=0; i < FILE_SIZE / FOUR_KB; i++){
        read(fd, buffer, FOUR_KB);
    }
    gettimeofday(&end, NULL);

    unsigned long diff = 1000000 * (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec);

    printf("Sequential Read Time: %ld (us)\n", diff);
    close(fd);
}

/*Overwrite the file with 100MB of new data using sequential writes.
Write 2KB of data per operation from beginning to end.
After completing all writes, call fsync() to ensure that the data is flushed to disk.*/
void seq_write(){
    int fd = open("test.txt", O_WRONLY | O_TRUNC | O_CREAT, 0644);
    if(fd == -1){
        printf("Error: Failed to open test.txt. \n");
        return;
    }
    char buffer[TWO_KB];
    memset(buffer, 'A', TWO_KB);
    struct timeval start, end;

    gettimeofday(&start, NULL);
    for(int i=0; i < FILE_SIZE / TWO_KB; i++){
        write(fd, buffer, TWO_KB);
    }
    fsync(fd);
    gettimeofday(&end, NULL);

    unsigned long diff = 1000000 * (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec);
    printf("Sequential Write Time: %ld (us)\n", diff);
    close(fd);
}

/*Repeat the following operation 50,000 times:
Select a 4KB-aligned offset uniformly at random within the file.
Seek to the selected offset using lseek().	
Read 4KB of data from that position.*/
void random_read(){
    int fd = open("test.txt", O_RDONLY);
    if(fd == -1){
        printf("Error: Failed to open file.\n");
        return;
    }
    char buffer[FOUR_KB];
    struct timeval start, end;
    srand(time(NULL));

    gettimeofday(&start, NULL);
    for(int i=0; i < 50000; i++){
        long offset = rand() % (FILE_SIZE / FOUR_KB) * FOUR_KB;
        lseek(fd, offset, SEEK_SET);
        read(fd, buffer, FOUR_KB);
    }
    gettimeofday(&end, NULL);

    unsigned long diff = 1000000 * (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec);
    printf("Random Read Time: %ld (us)\n", diff);
    close(fd);
}

/*Random Buffered Write (Single Sync)	
Repeat the following operation 50,000 times:
Select a 4KB-aligned offset uniformly at random within the file.
Seek to the selected offset using lseek().
Write 2KB of data at that position.
After all writes have been completed, call fsync() once.*/
void random_buffered_write(){
    int fd = open("test.txt", O_WRONLY);
    if(fd == -1){
        printf("Error: Failed to open file.\n");
        return;
    }
    char buffer[TWO_KB];
    memset(buffer, 'A', TWO_KB);
    struct timeval start, end;
    srand(time(NULL));

    gettimeofday(&start, NULL);
    for(int i=0; i < 50000; i++){
        long offset = rand() % (FILE_SIZE / FOUR_KB) * FOUR_KB;
        lseek(fd, offset, SEEK_SET);
        write(fd, buffer, TWO_KB);
    }
    fsync(fd);
    gettimeofday(&end, NULL);

    unsigned long diff = 1000000 * (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec);
    printf("Random Buffered Write Time: %ld (us)\n", diff);
    close(fd);
}

/*Random Synchronous Write	
Repeat the following operation 50,000 times:
Select a 4KB-aligned offset uniformly at random within the file.
Seek to the selected offset using lseek().
Write 2KB of data at that position.
Call fsync() after each write.*/
void random_synchronous_write(){
    int fd = open("test.txt", O_WRONLY);
    if(fd == -1){
        printf("Error: Failed to open file.\n");
        return;
    }
    char buffer[TWO_KB];
    memset(buffer, 'A', TWO_KB);
    struct timeval start, end;
    srand(time(NULL));

    gettimeofday(&start, NULL);
    for(int i=0; i < 50000; i++){
        long offset = rand() % (FILE_SIZE / FOUR_KB) * FOUR_KB;
        lseek(fd, offset, SEEK_SET);
        write(fd, buffer, TWO_KB);
        fsync(fd);
    }
    gettimeofday(&end, NULL);

    unsigned long diff = 1000000 * (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec);
    printf("Random Synchronous Write Time: %ld (us)\n", diff);
    close(fd);
}


int main(int argc, char *argv[]){
    if (argc != 2) {
        printf("使用方式: %s <測試編號 1-5>\n", argv[0]);
        printf("1: Seq Read, 2: Seq Write, 3: Rand Read, 4: Rand Buf Write, 5: Rand Sync Write\n");
        return 1;
    }

    int test_id = atoi(argv[1]); // atoi = ASCII to integer
    switch (test_id)
    {
    case 1:
        seq_read();
        break;
    case 2:
        seq_write();
        break;
    case 3:
        random_read();
        break;
    case 4:
        random_buffered_write();
        break;
    case 5:
        random_synchronous_write();
        break;
    default:
        break;
    }
}