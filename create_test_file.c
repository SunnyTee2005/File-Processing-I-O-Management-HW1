#include <time.h>
#include <stdio.h>
#include <stdlib.h> // for srand()
#include <sys/time.h> // for gettimeofday()
#include <unistd.h> // for fsync(), a function that flush data from page cache to stroage.
#define FILE_SIZE (100 * 1024 * 1024) // file size 100MB
#define CHUNK_SIZE (4 * 1024) // chunk size 4KB

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

int main(void){
    create_test_file();
    return 0;
}