#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sr.h"
#include "emulator.h"  // Ensure the struct pkt is included from the emulator.h

/* Statistics counters (declared in emulator.h, defined here) */
int total_ACKs_received = 0;
int packets_resent = 0;
int new_ACKs = 0;
int packets_received = 0;
int window_full = 0;

/* Helper function to calculate checksum (same as in gbn.c) */
int calculate_checksum(struct pkt packet) {
    int checksum = 0;
    int i;
    for (i = 0; i < 20; i++) {
        checksum += (int)packet.payload[i];
    }
    checksum += packet.seqnum;
    checksum += packet.acknum;
    return checksum;
}

/* Helper function to check if a packet is corrupted */
int is_corrupted(struct pkt packet) {
    return (packet.checksum != calculate_checksum(packet));
}

/* Helper function to send a packet */
void send_packet(int AorB, struct pkt packet) {
    if (TRACE > 2) {
        printf("Sender %d sending packet with seqnum %d and payload: %s\n", AorB, packet.seqnum, packet.payload);
    }
    tolayer3(AorB, packet);
}

/* Helper function to send an ACK */
void send_ack(int AorB, int acknum) {
    struct pkt ack_pkt;
    ack_pkt.seqnum = -1; /* Indicate it's an ACK */
    ack_pkt.acknum = acknum;
    ack_pkt.checksum = calculate_checksum(ack_pkt);
    if (TRACE > 2) {
        printf("Sender %d sending ACK with acknum %d\n", AorB, acknum);
    }
    tolayer3(AorB, ack_pkt);
}

/* Helper function to buffer a received packet */
void buffer_packet(struct pkt packet) {
    int index = packet.seqnum % WINDOW_SIZE;
    receiver_buffer[index].packet = packet;
    receiver_buffer[index].received = 1;
    if (TRACE > 2) {
        printf("Receiver buffered packet with seqnum %d\n", packet.seqnum);
    }
}

/* Helper function to deliver available packets */
void deliver_available_packets() {
    while (receiver_buffer[receiver_expected_seq_num % WINDOW_SIZE].received) {
        struct pkt *pkt_to_deliver = &receiver_buffer[receiver_expected_seq_num % WINDOW_SIZE].packet;
        tolayer5(B, pkt_to_deliver->payload);
        receiver_buffer[receiver_expected_seq_num % WINDOW_SIZE].received = 0;
        if (TRACE > 2) {
            printf("Receiver delivered packet with seqnum %d\n", pkt_to_deliver->seqnum);
        }
        receiver_expected_seq_num = (receiver_expected_seq_num + 1) % SEQ_NUM_MODULO;
    }
}

/* Side A (Sender) */

void A_init(void) {
    printf("Initializing Sender A...\n");
    sender_base = 0;
    sender_next_seq_num = 0;
    int i;
    for (i = 0; i < WINDOW_SIZE; i++) {
        sender_window[i].acked = 1; /* Initially all slots are empty */
        sender_window[i].timer_running = 0;
    }
}

void A_output(struct msg message) {
    int window_index;
    window_index = sender_next_seq_num % WINDOW_SIZE;
    if (((sender_next_seq_num + 1) % SEQ_NUM_MODULO) == sender_base) {
        printf("Sender A window is full. Message discarded.\n");
        window_full++;
        return;
    }

    if (sender_window[window_index].acked) {
        /* Create packet */
        struct pkt new_packet;
        new_packet.seqnum = sender_next_seq_num;
        new_packet.acknum = -1; /* Not an ACK */
        strncpy(new_packet.payload, message.data, 20);
        new_packet.checksum = calculate_checksum(new_packet);

        /* Store in sender window */
        sender_window[window_index].packet = new_packet;
        sender_window[window_index].acked = 0;
        sender_window[window_index].time_sent = get_sim_time();

        /* Send packet */
        send_packet(A, new_packet);

        /* Start timer for this packet if it's the first unacknowledged packet */
        if (sender_base == sender_next_seq_num) {
            starttimer(A, RTT);
            sender_window[window_index].timer_running = 1;
        }

        /* Move to the next sequence number */
        sender_next_seq_num = (sender_next_seq_num + 1) % SEQ_NUM_MODULO;
    } else {
        printf("Error: Trying to send when window slot is not available.\n");
    }
}

void A_input(struct pkt packet) {
    int ack_seqnum;
    int index;
    if (is_corrupted(packet)) {
        if (TRACE > 0) {
            printf("Sender A received corrupted ACK. Discarding.\n");
        }
        return;
    }

    if (packet.acknum != -1) { /* It's an ACK */
        if (TRACE > 1) {
            printf("Sender A received ACK for packet with seqnum %d\n", packet.acknum);
        }

        ack_seqnum = packet.acknum;

        /* Check if the ACK is within the sender's window and for a packet that hasn't been ACKed */
        if (ack_seqnum >= sender_base && ack_seqnum < (sender_base + WINDOW_SIZE) % SEQ_NUM_MODULO) {
            index = ack_seqnum % WINDOW_SIZE;
            if (!sender_window[index].acked) {
                sender_window[index].acked = 1;
                stoptimer(A); /* Stop timer for the acknowledged packet */
                sender_window[index].timer_running = 0;
                new_ACKs++;

                /* Move sender_base forward if the acknowledged packet was the current base */
                while (sender_window[sender_base % WINDOW_SIZE].acked && sender_base != sender_next_seq_num) {
                    sender_base = (sender_base + 1) % SEQ_NUM_MODULO;
                    /* If there are more unacknowledged packets, restart the timer */
                    if (sender_base != sender_next_seq_num && !sender_window[sender_base % WINDOW_SIZE].timer_running && !sender_window[sender_base % WINDOW_SIZE].acked) {
                        starttimer(A, RTT);
                        sender_window[sender_base % WINDOW_SIZE].timer_running = 1;
                    }
                }
                /* If there are still unacknowledged packets, restart the timer for the new base */
                if (sender_base != sender_next_seq_num && !sender_window[sender_base % WINDOW_SIZE].timer_running && !sender_window[sender_base % WINDOW_SIZE].acked) {
                    starttimer(A, RTT);
                    sender_window[sender_base % WINDOW_SIZE].timer_running = 1;
                }
            }
        } else {
            if (TRACE > 1) {
                printf("Sender A received out-of-window or duplicate ACK for seqnum %d. Ignoring.\n", ack_seqnum);
            }
        }
    }
}

void A_timerinterrupt(void) {
    printf("Sender A timer interrupt occurred.\n");
    int i;
    int j;
    for (i = 0; i < WINDOW_SIZE; i++) {
        if (!sender_window[i].acked) {
            if (sender_window[i].timer_running) {
                if (TRACE > 1) {
                    printf("Sender A retransmitting packet with seqnum %d\n", sender_window[i].packet.seqnum);
                }
                send_packet(A, sender_window[i].packet);
                starttimer(A, RTT); /* Restart the timer for all unacknowledged packets */
                packets_resent++;
                /* Mark all unacked packets as timer running to avoid multiple restarts */
                for (j = 0; j < WINDOW_SIZE; j++) {
                    if (!sender_window[j].acked) {
                        sender_window[j].timer_running = 1;
                    }
                }
                return; /* Only one timer is active at a time for the base */
            }
        }
    }
    /* If no unacked packets, no timer should be running (shouldn't reach here if logic is correct) */
}

/* Side B (Receiver) */

void B_init(void) {
    printf("Initializing Receiver B...\n");
    receiver_expected_seq_num = 0;
    int i;
    for (i = 0; i < WINDOW_SIZE; i++) {
        receiver_buffer[i].received = 0;
    }
}

void B_input(struct pkt packet) {
    if (is_corrupted(packet)) {
        if (TRACE > 0) {
            printf("Receiver B received corrupted packet. Sending ACK for expected seqnum %d.\n", receiver_expected_seq_num);
        }
        send_ack(B, receiver_expected_seq_num);
        return;
    }

    if (TRACE > 1) {
        printf("Receiver B received packet with seqnum %d (expected %d)\n", packet.seqnum, receiver_expected_seq_num);
    }

    if (packet.seqnum >= receiver_expected_seq_num && packet.seqnum < (receiver_expected_seq_num + WINDOW_SIZE) % SEQ_NUM_MODULO) {
        /* Packet is within the receiver's window */
        buffer_packet(packet);
        send_ack(B, packet.seqnum); /* Send ACK for the received packet */
        deliver_available_packets();
        packets_received++;
    } else if (packet.seqnum < receiver_expected_seq_num) {
        /* Packet is a retransmission of an already ACKed packet */
        if (TRACE > 1) {
            printf("Receiver B received old packet with seqnum %d. Resending ACK for %d.\n", packet.seqnum, packet.seqnum);
        }
        send_ack(B, packet.seqnum);
    } else {
        /* Packet is outside the receiver's window (shouldn't happen in a correctly functioning SR) */
        if (TRACE > 0) {
            printf("Receiver B received out-of-window packet with seqnum %d. Ignoring.\n", packet.seqnum);
        }
    }
}
void B_output(struct msg message) {
    // Dummy implementation
}

void B_timerinterrupt(void) {
    // Dummy implementation
}

float get_sim_time(void) {
    return 0.0;  // Dummy return value
}
