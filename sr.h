#ifndef SR_H
#define SR_H

#include "emulator.h"

#define WINDOW_SIZE 6
#define SEQSPACE 12
#define TIMEOUT 20.0

void A_init(void);
void A_output(struct msg message);
void A_input(struct pkt packet);
void A_timerinterrupt(void);

void B_init(void);
void B_input(struct pkt packet);

/* Add these dummy functions to satisfy linker */
void B_output(struct msg message);
void B_timerinterrupt(void);

#endif /* SR_H */