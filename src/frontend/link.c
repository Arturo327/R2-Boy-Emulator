#include "frontend/link.h"

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

static void ring_push (LinkRing *r, uint8_t byte)
{
	uint32_t wp = atomic_load_explicit(&r->write_pos, memory_order_relaxed);
	uint32_t rp = atomic_load_explicit(&r->read_pos, memory_order_acquire);
	if ((wp - rp) >= LINK_RING_SIZE) return;
	r->buffer[wp & (LINK_RING_SIZE - 1)] = byte;
	atomic_store_explicit(&r->write_pos, wp + 1, memory_order_release);
}

static int ring_pop (LinkRing *r, uint8_t *out)
{
	uint32_t wp = atomic_load_explicit(&r->write_pos, memory_order_acquire);
	uint32_t rp = atomic_load_explicit(&r->read_pos, memory_order_relaxed);
	if (rp == wp) return 0;
	*out = r->buffer[rp & (LINK_RING_SIZE - 1)];
	atomic_store_explicit(&r->read_pos, rp + 1, memory_order_release);
	return 1;
}

static void *link_thread_fn (void *arg)
{
	Link *link = (Link *) arg;

	if (link->listen_socket >= 0) {
		int client = accept(link->listen_socket, NULL, NULL);
		close(link->listen_socket);
		link->listen_socket = -1;
		if (client < 0) {
			perror("accept");
			return NULL;
		}
		link->socket = client;
	}

	int one = 1;
	setsockopt(link->socket, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
	atomic_store_explicit(&link->connected, 1, memory_order_release);

	printf("Link cable: connected\n");

	struct pollfd pfd = { .fd = link->socket, .events = POLLIN };

	while (atomic_load_explicit(&link->running, memory_order_acquire)) {

		int r = poll(&pfd, 1, 0);

		if (r > 0 && (pfd.revents & (POLLIN | POLLHUP | POLLERR))) {
			uint8_t buf[64];
			ssize_t n = recv(link->socket, buf, sizeof(buf), 0);
			if (n <= 0) {
				printf("Link cable: disconnected\n");
				atomic_store_explicit(&link->connected, 0, memory_order_release);
				break;
			}
			for (ssize_t i = 0; i < n; i++)
				ring_push(&link->rx, buf[i]);
		}

		uint8_t out;
		while (ring_pop(&link->tx, &out)) {
			if (send(link->socket, &out, 1, 0) < 0) {
				printf("Link cable: error sending\n");
				break;
			}
		}
	}
	return NULL;
}

static void link_init_common (Link *link)
{
	memset(link, 0, sizeof(Link));
	link->socket = -1;
	link->listen_socket = -1;
	atomic_init(&link->running, 1);
	atomic_init(&link->connected, 0);
	signal(SIGPIPE, SIG_IGN);
}

int link_host (Link *link, uint16_t port)
{
	link_init_common(link);

	int s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0) {
		perror("socket");
		return 0;
	}

	int one = 1;
	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

	struct sockaddr_in addr = {0};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(port);

	if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("bind");
		close(s);
		return 0;
	}
	if (listen(s, 1) < 0) {
		perror("listen");
		close(s);
		return 0;
	}

	link->listen_socket = s;
	printf("Link cable: waiting for connection at port %u...\n", port);

	if (pthread_create(&link->thread, NULL, link_thread_fn, link) != 0) {
		perror("pthread_create");
		close(s);
		return 0;
	}

	link->active = 1;
	return 1;
}

int link_connect (Link *link, const char *ip, uint16_t port)
{
	link_init_common(link);

	int s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0) {
		perror("socket");
		return 0;
	}

	struct sockaddr_in addr = {0};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
		fprintf(stderr, "Link cable: IP '%s' is not valid\n", ip);
		close(s);
		return 0;
	}

	printf("Link cable: connecting to %s:%u...\n", ip, port);
	if (connect(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("connect");
		close(s);
		return 0;
	}

	link->socket = s;

	if (pthread_create(&link->thread, NULL, link_thread_fn, link) != 0) {
		perror("pthread_create");
		close(s);
		return 0;
	}

	link->active = 1;
	return 1;
}

void link_close (Link *link)
{
	if (!link || !link->active) return;

	atomic_store_explicit(&link->running, 0, memory_order_release);
	pthread_join(link->thread, NULL);

	if (link->socket >= 0) close(link->socket);
	if (link->listen_socket >= 0) close(link->listen_socket);

	atomic_store_explicit(&link->connected, 0, memory_order_release);

	link->active = 0;
}

int link_is_connected (Link *link) {
	return link && atomic_load_explicit(&link->connected, memory_order_acquire);
}

int link_get_byte (Link *link, uint8_t *out)
{
	if (!link) return 0;
	return ring_pop(&link->rx, out);
}

void link_send_byte (Link *link, uint8_t val)
{
	if (!link) return;
	ring_push(&link->tx, val);
}
