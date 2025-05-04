#ifndef SR_H
#define SR_H

#include "emulator.h"


/* Selective Repeat specific parameters */
#define RTT 16.0       /* Round trip time */
#define WINDOWSIZE 6   /* Window size (must be <= SEQSPACE/2) */
#define SEQSPACE 7     /* Sequence number space */
#define NOTINUSE (-1)  /* Marks unused header fields */

/* Packet structure (unchanged from GBN) */
struct pkt {
    int seqnum;
    int acknum;
    int checksum;
    char payload[20];
};


#endif /* SR_H */