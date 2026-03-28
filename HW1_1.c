#include <time.h>
#include <stdio.h>
#include <stdlib.h> // for srand()
#include <sys/time.h> // for gettimeofday()
#include <unistd.h> // for fsync(), a function that flush data from page cache to stroage.
#define FILE_SIZE (100 * 1024 * 1024) // file size 100MB
#define CHUNK_SIZE (4 * 1024) // chunk size 4KB

// 5 steps
// 1: create the 100MB file

void create_test_file(){
    FILE *f1 = fopen("test.txt", "w");
    if(f1 == NULL){
        printf("Error: Failed to create file.");
        return;
    }

    char buffer[CHUNK_SIZE];
    for(int i=0; i<CHUNK_SIZE; i++){
        buffer[i] = 'A';
    }

    int iterations = FILE_SIZE / CHUNK_SIZE;
    for(int i=0; i < iterations; i++){
        fwrite(buffer, sizeof(char), CHUNK_SIZE, f1);
    }

    fclose(f1);

    printf("\"test_file.txt\" was created.\n");
}

void seq_read(){
    FILE *f1 = fopen("test.txt", "r");
    struct timeval start, end;
    char buffer[CHUNK_SIZE];
    int iterations = FILE_SIZE / CHUNK_SIZE;

    gettimeofday(&start, NULL);
    for(int i=0; i < iterations; i++){
        fread(buffer, sizeof(char), CHUNK_SIZE, f1);
    }
    gettimeofday(&end, NULL);

    unsigned long time_diff = (end.tv_usec - start.tv_usec) + 1000000 * (end.tv_sec - start.tv_sec);
    printf("Sequential Read Time: %ld (us)\n", time_diff);
    fclose(f1);
}

void seq_write(){
    FILE *f1 = fopen("test.txt", "w");
    struct timeval start, end;
    char buffer[2 * 1024]; // 2KB buffer
    for(int i=0; i<2*1024; i++){
        buffer[i] = 'A';
    }

    gettimeofday(&start, NULL);
    for(int i=0; i < FILE_SIZE / (2 * 1024); i++){
        fwrite(buffer, sizeof(char), (2 * 1024), f1);
    }
    fflush(f1); // flush data from user space buffer to page cache. 
    fsync(fileno(f1)); // flush data from page cache (in RAM) to stroage.
    gettimeofday(&end, NULL);

    unsigned long time_diff = (end.tv_usec - start.tv_usec) + 1000000 * (end.tv_sec - start.tv_sec);
    printf("Sequential Write Time: %ld (us)\n", time_diff);

    fclose(f1);
}

void random_read(){
    srand(time(NULL));
    char buffer[CHUNK_SIZE];
    int blocks = FILE_SIZE / CHUNK_SIZE;
    long offset;
    struct timeval start, end;
    FILE *f1 = fopen("test.txt", "r");

    gettimeofday(&start, NULL);
    for(int i=0; i<50000; i++){
        offset = (rand() % blocks) * (4 * 1024);
        fseek(f1, offset, SEEK_SET); // change the "file position indicator" to the offset defined above.
        fread(buffer, sizeof(char), CHUNK_SIZE, f1);
    }
    gettimeofday(&end, NULL);

    unsigned long diff = 1000000 * (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec);
    printf("Random Read Time: %ld (us)\n", diff);
    fclose(f1);
}

void random_buffered_write(){
    FILE *f1 = fopen("test.txt", "r+");
    struct timeval start, end;
    char buffer[2 * 1024]; // 2KB buffer
    for(int i=0; i<2*1024; i++){
        buffer[i] = 'A';
    }
    int blocks = FILE_SIZE / CHUNK_SIZE;
    long offset;

    gettimeofday(&start, NULL);
    for(int i=0; i<50000; i++){
        offset = (rand() % blocks) * (4 * 1024);
        fseek(f1, offset, SEEK_SET); // change the "file position indicator" to the offset defined above.
        fwrite(buffer, sizeof(char), 2*1024, f1);
    }
    fflush(f1);
    fsync(fileno(f1));
    gettimeofday(&end, NULL);

    unsigned long diff = 1000000 * (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec);
    printf("Random Buffered Write Time: %ld (us)\n", diff);
    fclose(f1);
}

void random_synchronous_write(){
    FILE *f1 = fopen("test.txt", "r+");
    struct timeval start, end;
    char buffer[2 * 1024]; // 2KB buffer
    for(int i=0; i<2*1024; i++){
        buffer[i] = 'A';
    }
    int blocks = FILE_SIZE / CHUNK_SIZE;
    long offset;

    gettimeofday(&start, NULL);
    for(int i=0; i<50000; i++){
        offset = (rand() % blocks) * (4 * 1024);
        fseek(f1, offset, SEEK_SET); // change the "file position indicator" to the offset defined above.
        fwrite(buffer, sizeof(char), 2*1024, f1);
        fflush(f1);
        fsync(fileno(f1));
    }
    gettimeofday(&end, NULL);

    unsigned long diff = 1000000 * (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec);
    printf("Random Synchronous Write Time: %ld (us)\n", diff);
    fclose(f1);
}

int main(int argc, char *argv[]) {
    // 檢查使用者有沒有在終端機輸入參數
    if (argc != 2) {
        printf("使用方式: %s <測試編號 1-5>\n", argv[0]);
        printf("1: Seq Read, 2: Seq Write, 3: Rand Read, 4: Rand Buf Write, 5: Rand Sync Write\n");
        return 1;
    }

    // 將字串參數轉換為整數
    int test_id = atoi(argv[1]);

    // 根據輸入的編號，決定這次執行哪一個 function
    switch (test_id) {
        case 1:
            //printf("執行 1: Sequential Read...\n");
            seq_read();
            break;
        case 2:
            //printf("執行 2: Sequential Write...\n");
            seq_write();
            break;
        case 3:
            //printf("執行 3: Random Read...\n");
            random_read();
            break;
        case 4:
            //printf("執行 4: Random Buffered Write...\n");
            random_buffered_write();
            break;
        case 5:
            //printf("執行 5: Random Synchronous Write...\n");
            random_synchronous_write();
            break;
        default:
            printf("錯誤：無效的測試編號。\n");
            break;
    }

    return 0;
}