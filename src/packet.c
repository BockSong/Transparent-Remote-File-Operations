#include "packet.h"

// you need to do packing for params from caller before call this func
packet* packing(enum operations func, char* param) {
    packet* pkt;
	pkt = malloc(sizeof(packet));
    switch (func) {
        case OP_OPEN:
            pkt->opcode = 1;
            break;
        case OP_CLOSE:
            pkt->opcode = 2;
            break;
        case OP_WRITE:
            pkt->opcode = 3;
            break;
    }
    pkt->param = malloc(strlen(param));
    memcpy(pkt->param, param, strlen(param));
    return pkt;
}

/*
packet* pack_open(const char *pathname, int flags, ...) {
    packet* pkt;
    int param_len = sizeof(int);
    pkt->opcode = 1;
    pkt->param = malloc();
}

packet* pack_close(int fildes) {
    packet* pkt;
    pkt->opcode = 2;
    pkt->param = malloc(sizeof(int));
    memcpy(pkt->param, &fildes, sizeof(int));
    return pkt;
}

packet* pack_write(int fildes, const void *buf, size_t nbyte) {
    packet* pkt;
    pkt->opcode = 3;
    ;
}
*/
