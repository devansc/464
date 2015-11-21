#include  <stdlib.h>
#include  <stdio.h>

FILE *transferFile;

//returns number of bytes of file
int openFile(char *filename) {
    long size;
    transferFile = fopen(filename, "r");
    if (transferFile == NULL) {
        fprintf(stderr, "File %s does not exist, exitting", filename);
        exit(1);
    }
    fseek(transferFile, 0L, SEEK_END);
    size = ftell(transferFile);
    printf(" SIZE IS %ld\n", size);
    fseek(transferFile, 0L, SEEK_SET);
    return (int)size;
}

int main(void) {
    int size;
    size = openFile("testFile");
    return 0;
}


