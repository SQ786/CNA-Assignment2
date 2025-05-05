#ifndef SR_H
#define SR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "emulator.h"  // Include emulator.h to access the msg and pkt structures

/* Constants (adjust these based on assignment requirements) */
#define WINDOW_SIZE 6  /* Example:  The assignment specifies 6 */
#define SEQ_NUM_MODULO 16 /* Example: Sequence number space (must be > 2*WINDOW_SIZE) */
#define RTT 16.0       /* The assignment specifies 16.0 */

/* Data Structures */
/* Sender Window Entry */
struct sender_window_entry {
    struct pkt packet;  /* Uses pkt from emulator.h */
    float time_sent;
    int timer_running;
    int acked;
};

extern float get_sim_time(void);

/* Receiver Buffer Entry (if you choose to use a fixed-size buffer) */
struct receiver_buffer_entry {
    struct pkt packet;  /* Uses pkt from emulator.h */
    int received;
};

/* Function Prototypes */

/* Side A (Sender) */
void A_init(void);
void A_output(struct msg message);  /* Uses msg from emulator.h */
void A_input(struct pkt packet);    /* Uses pkt from emulator.h */
void A_timerinterrupt(void);

/* Side B (Receiver) */
void B_init(void);
void B_input(struct pkt packet);    /* Uses pkt from emulator.h */

/* Optional for Bidirectional Transfer (if needed) */
void B_output(struct msg message);  /* Uses msg from emulator.h */
void B_timerinterrupt(void);

/* Global Variables */
/* Sender */
struct sender_window_entry sender_window[WINDOW_SIZE];
int sender_base;           /* Base of the sender window */
int sender_next_seq_num;  /* Next sequence number to be used */

/* Receiver */
struct receiver_buffer_entry receiver_buffer[WINDOW_SIZE]; /* Or consider dynamic allocation */
int receiver_expected_seq_num; /* Expected sequence number */
int B_nextseqnum; /* Next sequence number to send from B (if bidirectional) */

/* Statistics counters (if needed for debugging/reporting) */
extern int total_ACKs_received;
extern int packets_resent;
extern int new_ACKs;
extern int packets_received;
extern int window_full;

#endif /* SR_H */
