#include <packet.h>

// you need to do packing for params from caller before call this func
packet* packing(enum operations func, char* param) {
    packet* pkt;
    switch (func) {
        case OPEN:
            pkt->opcode = 1;
            pkt->param = param;
            break;
        case CLOSE:
            pkt->opcode = 2;
            pkt->param = param;
            break;
        case WRITE:
            pkt->opcode = 3;
            pkt->param = param;
            break;
    }
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
