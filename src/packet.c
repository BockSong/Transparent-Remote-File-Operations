#include <packet.h>

packet* packaging(enum operations func, char* param) {
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

packet* pack_open(const char *pathname, int flags, ...) {
    packet* pkt;
    pkt->opcode = 1;
    ;
}

packet* pack_close(int fildes) {
    packet* pkt;
    pkt->opcode = 2;
    ;
}

packet* pack_write(int fildes, const void *buf, size_t nbyte) {
    packet* pkt;
    pkt->opcode = 3;
    ;
}

