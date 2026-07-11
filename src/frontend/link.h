#ifndef LINK_H
#define LINK_H

#include <stdint.h>
#include <stdatomic.h>
#include <pthread.h>

#define LINK_RING_SIZE 64
_Static_assert((LINK_RING_SIZE & (LINK_RING_SIZE - 1)) == 0,
                 "LINK_RING_SIZE must be power of 2");

typedef struct LinkRing {
	uint8_t buffer[LINK_RING_SIZE];
	_Atomic uint32_t write_pos;
	_Atomic uint32_t read_pos;
} LinkRing;

typedef struct Link {
	int socket;
	int listen_socket;
	int active;
	_Atomic int running;
	_Atomic int connected;

	pthread_t thread;

	LinkRing rx;
	LinkRing tx;
} Link;

int link_host (Link *link, uint16_t port);
int link_connect (Link *link, const char *ip, uint16_t port);

void link_close (Link *link);
int link_is_connected (Link *link);

int link_get_byte (Link *link, uint8_t *out);
void link_send_byte (Link *link, uint8_t val);

#endif
