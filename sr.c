#include "sr.h"
#include <stdio.h>
#include <string.h>

/* Sender side */
static int base = 0;
static int nextseqnum = 0;
static struct pkt snd_buffer[SEQSPACE];
static int acked[SEQSPACE];

/* Receiver side */
static int expectedseqnum = 0;
static struct pkt rcv_buffer[SEQSPACE];
static int received[SEQSPACE];

static int compute_checksum(struct pkt packet)
{
    int checksum = packet.seqnum + packet.acknum;
    int i;
    for (i = 0; i < 20; i++) {
        checksum += packet.payload[i];
    }
    return checksum;
}

static int is_corrupted(struct pkt packet)
{
    return packet.checksum != compute_checksum(packet);
}

static int in_window(int start, int seqnum)
{
    if (start <= (start + WINDOW_SIZE - 1) % SEQSPACE) {
        return seqnum >= start && seqnum <= (start + WINDOW_SIZE - 1) % SEQSPACE;
    }
    return seqnum >= start || seqnum <= (start + WINDOW_SIZE - 1) % SEQSPACE;
}

void A_output(struct msg message)
{
    if ((nextseqnum - base) % SEQSPACE < WINDOW_SIZE) {
        struct pkt packet;
        int i;
        
        packet.seqnum = nextseqnum;
        packet.acknum = 0;
        for (i = 0; i < 20; i++) {
            packet.payload[i] = message.data[i];
        }
        packet.checksum = compute_checksum(packet);

        snd_buffer[nextseqnum % SEQSPACE] = packet;
        acked[nextseqnum % SEQSPACE] = 0;

        // Add debug log here to track what packet is being sent
        printf("A: Sending packet %d\n", nextseqnum); 

        if (base == nextseqnum) {
            starttimer(0, TIMEOUT);
        }
        
        tolayer3(0, packet);
        nextseqnum = (nextseqnum + 1) % SEQSPACE;
    }
}


void A_input(struct pkt packet)
{
    int acknum;
    
    // If the packet is corrupted, discard it
    if (is_corrupted(packet)) return;

    acknum = packet.acknum;

    // If the acknowledgment is within the window, process it
    if (in_window(base, acknum)) {
        acked[acknum % SEQSPACE] = 1;

        // Add debug log here to track the acknowledgment number
        printf("A: Acknowledgment received: %d\n", acknum); 

        // Move the base forward to the next unacknowledged packet
        while (acked[base % SEQSPACE]) {
            acked[base % SEQSPACE] = 0;
            base = (base + 1) % SEQSPACE;
        }

        // If the base has changed, restart the timer
        stoptimer(0);
        if (base != nextseqnum) {
            starttimer(0, TIMEOUT);
        }
    }
}



void A_timerinterrupt(void)
{
    int i;
    for (i = base; i != nextseqnum; i = (i + 1) % SEQSPACE) {
        if (!acked[i % SEQSPACE]) {
            tolayer3(0, snd_buffer[i % SEQSPACE]);
        }
    }
    starttimer(0, TIMEOUT);
}

void A_init(void)
{
    int i;
    base = 0;
    nextseqnum = 0;
    for (i = 0; i < SEQSPACE; i++) {
        acked[i] = 0;
    }
}

void B_input(struct pkt packet)
{
    int seq;
    struct pkt ack_pkt;
    int i;

    // If the packet is corrupted, discard it
    if (is_corrupted(packet)) return;

    seq = packet.seqnum;

    // If the sequence number is in the expected window, process it
    if (in_window(expectedseqnum, seq)) {
        if (!received[seq % SEQSPACE]) {
            rcv_buffer[seq % SEQSPACE] = packet;
            received[seq % SEQSPACE] = 1;
        }

        // Create the acknowledgment packet for the received packet
        ack_pkt.seqnum = 0;
        ack_pkt.acknum = seq;  // Acknowledge the received sequence number
        for (i = 0; i < 20; i++) {
            ack_pkt.payload[i] = 0;
        }
        ack_pkt.checksum = compute_checksum(ack_pkt);

        // Send the acknowledgment packet back to A
        tolayer3(1, ack_pkt);

        // Add debug log here to track received packet and acknowledgment sent
        printf("B: Received packet with seqnum %d, sending ack %d\n", seq, seq); 

        // Deliver packets to the application if they are in the correct order
        while (received[expectedseqnum % SEQSPACE]) {
            tolayer5(1, rcv_buffer[expectedseqnum % SEQSPACE].payload);
            received[expectedseqnum % SEQSPACE] = 0;
            expectedseqnum = (expectedseqnum + 1) % SEQSPACE;
        }
    }
}

void B_init(void)
{
    int i;
    expectedseqnum = 0;
    for (i = 0; i < SEQSPACE; i++) {
        received[i] = 0;
    }
}

/* Dummy implementations to satisfy linker */
void B_output(struct msg message) {
    /* Unused in SR implementation */
}

void B_timerinterrupt(void) {
    /* Unused in SR implementation */
}