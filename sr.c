#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "emulator.h"

#define WINDOW_SIZE 6
#define SEQ_NUM_MODULO 12
#define RTT 16.0

/* Sender state */
static int sender_base = 0;
static int sender_next_seq_num = 0;
static struct pkt sender_window[WINDOW_SIZE];
static int acked[WINDOW_SIZE]; /* 1=ACKed, 0=not ACKed */

/* Receiver state */
static int receiver_expected_seq_num = 0;
static struct pkt receiver_buffer[WINDOW_SIZE];
static int received[WINDOW_SIZE]; /* 1=received, 0=not received */

/* Statistics */
int total_ACKs_received = 0;
int packets_resent = 0;
int new_ACKs = 0;
int packets_received = 0;
int window_full = 0;

/* Helper Functions */
int calculate_checksum(struct pkt packet) {
    int checksum = packet.seqnum + packet.acknum;
    for (int i = 0; i < 20; i++) {
        checksum += packet.payload[i];
    }
    return checksum;
}

int is_corrupted(struct pkt packet) {
    return packet.checksum != calculate_checksum(packet);
}


void send_ack(int entity, int acknum) {
    struct pkt ack_pkt;
    memset(ack_pkt.payload, 0, 20);
    ack_pkt.seqnum = -1;
    ack_pkt.acknum = acknum;
    ack_pkt.checksum = calculate_checksum(ack_pkt);
    tolayer3(entity, ack_pkt);

}

void send_packet(int entity, struct pkt packet) {
    if (TRACE > 2) {
        printf("Entity %d sending packet seqnum=%d\n", entity, packet.seqnum);
    }
    tolayer3(entity, packet);
}

/* Sender Implementation */
void A_init(void) {
    sender_base = 0;
    sender_next_seq_num = 0;
    memset(acked, 0, sizeof(acked));
}

void A_output(struct msg message) {
    /* Check if window is full */
    if ((sender_next_seq_num - sender_base) % SEQ_NUM_MODULO >= WINDOW_SIZE) {
        if (TRACE > 0) {
            printf("Window full (base=%d, next=%d). Message dropped.\n", 
                  sender_base, sender_next_seq_num);
        }
        window_full++;
        return;
    }

    /* Create and store packet */
    int window_index = sender_next_seq_num % WINDOW_SIZE;
    sender_window[window_index].seqnum = sender_next_seq_num;
    sender_window[window_index].acknum = -1;
    strncpy(sender_window[window_index].payload, message.data, 20);
    sender_window[window_index].checksum = calculate_checksum(sender_window[window_index]);

    acked[window_index] = 0;
    send_packet(A, sender_window[window_index]);

    /* Start timer if first packet in window */
    if (sender_base == sender_next_seq_num) {
        starttimer(A, RTT);
    }

    sender_next_seq_num = (sender_next_seq_num + 1) % SEQ_NUM_MODULO;
}

