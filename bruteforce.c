#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rlib.h"

unsigned long gen(char *str, int position, int length, char * expect, unsigned long attempt) {
    if (position == length) {
        str[position] = '\0'; 
        return attempt;
    }

    for (char c = 'a'; c <= 'z'; c++) {
        str[position] = c; 
        attempt = gen(str, position + 1, length,expect, attempt + 1);  
        if(!strcmp(str,expect))
        { 
            return attempt;
        }
    }
    return attempt;
}

int main(int argc, char *argv[]) {

    char *str = (char *)malloc((1337) * sizeof(char));

    if (str == NULL) {
        printf("Memory allocation failed!\n");
        return 1;
    }

    char * needle = argc == 2 ? argv[1] :  "reataoaor";
    size_t length = strlen(needle);
    RBENCH(1, {
        unsigned long attempt = gen(str, 0, length, needle, 0);
        printf("Resolved \"%s\" within %zd combinations.\n",needle, attempt);
    })

    free(str);

    return 0;
}
