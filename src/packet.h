#ifndef _PACKET_H
#define _PACKET_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum operations {
    OPEN = 1, CLOSE, WRITE
};

typedef struct{
    int opcode;
    char *param;
} packet;

packet packaging(enum operations func, char* param);

#endif
