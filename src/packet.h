#ifndef _PACKET_H
#define _PACKET_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum operations {
    OP_OPEN = 1, OP_CLOSE, OP_WRITE
};

typedef struct{
    int opcode;
    char *param;
} packet;

packet* packing(enum operations func, char* param);

#endif
