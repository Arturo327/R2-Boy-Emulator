#include "frontend/link.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>

#define LINK_CONNECT_TIMEOUT_MS 3000
#define LINK_RECONNECT_DELAY_MS 1000
#define LINK_POLL_TIMEOUT_MS 1000

static void ring_push_bulk (LinkRing *r, const uint8_t *src, uint32_t n)
{
	uint32_t wp = atomic_load_explicit(&r->write_pos, memory_order_relaxed);
	uint32_t rp = atomic_load_explicit(&r->read_pos, memory_order_acquire);
	uint32_t free_space = LINK_RING_SIZE - (wp - rp);

	if (n > free_space) n = free_space;
	if (n == 0) return;

	uint32_t idx = wp & (LINK_RING_SIZE - 1);
	uint32_t first = LINK_RING_SIZE - idx;
	if (first > n) first = n;

	memcpy(r->buffer + idx, src, first);
	if (n > first)
		memcpy(r->buffer, src + first, n - first);

	atomic_store_explicit(&r->write_pos, wp + n, memory_order_release);
}

static uint32_t ring_pop_bulk (LinkRing *r, uint8_t *out, uint32_t max)
{
	uint32_t wp = atomic_load_explicit(&r->write_pos, memory_order_acquire);
	uint32_t rp = atomic_load_explicit(&r->read_pos, memory_order_relaxed);
	uint32_t avail = wp - rp;

	uint32_t n = avail < max ? avail : max;
	if (n == 0) return 0;

	uint32_t idx = rp & (LINK_RING_SIZE - 1);
	uint32_t first = LINK_RING_SIZE - idx;
	if (first > n) first = n;

	memcpy(out, r->buffer + idx, first);
	if (n > first)
		memcpy(out + first, r->buffer, n - first);

	atomic_store_explicit(&r->read_pos, rp + n, memory_order_release);
	return n;
}

static void ring_push (LinkRing *r, uint8_t byte)
{
	ring_push_bulk(r, &byte, 1);
}

static int ring_pop (LinkRing *r, uint8_t *out)
{
	return ring_pop_bulk(r, out, 1) == 1;
}

static int ring_has_data (LinkRing *r)
{
	uint32_t wp = atomic_load_explicit(&r->write_pos, memory_order_acquire);
	uint32_t rp = atomic_load_explicit(&r->read_pos, memory_order_relaxed);
	return wp != rp;
}

static void ring_reset (LinkRing *r)
{
	uint32_t wp = atomic_load_explicit(&r->write_pos, memory_order_acquire);
	atomic_store_explicit(&r->read_pos, wp, memory_order_release);
}

static void wake_thread (Link *link)
{
	uint8_t b = 1;
	ssize_t r = write(link->wake_fds[1], &b, 1);
	(void) r;
}

static void drain_wake (Link *link)
{
	uint8_t buf[32];
	while (read(link->wake_fds[0], buf, sizeof(buf)) > 0) {}
}

static int set_nonblocking (int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0) return 0;
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

static void wait_or_wake (Link *link, int ms)
{
	struct pollfd pfd = { .fd = link->wake_fds[0], .events = POLLIN };
	int r = poll(&pfd, 1, ms);
	if (r > 0 && (pfd.revents & POLLIN))
		drain_wake(link);
}

static int try_accept (Link *link)
{
	struct pollfd pfds[2];
	pfds[0].fd = link->listen_socket;
	pfds[0].events = POLLIN;
	pfds[1].fd = link->wake_fds[0];
	pfds[1].events = POLLIN;

	while (atomic_load_explicit(&link->running, memory_order_acquire)) {

		int r = poll(pfds, 2, LINK_POLL_TIMEOUT_MS);
		if (r <= 0) continue;

		if (pfds[1].revents & POLLIN)
			drain_wake(link);

		if (pfds[0].revents & POLLIN) {
			int client = accept(link->listen_socket, NULL, NULL);
			if (client < 0) {
				if (errno != EAGAIN && errno != EWOULDBLOCK)
					perror("accept");
				continue;
			}
			return client;
		}
	}
	return -1;
}

static int try_connect_once (Link *link)
{
	int s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0) { perror("socket"); return -1; }

	struct sockaddr_in addr = {0};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(link->port);
	set_nonblocking(s);

	int r = connect(s, (struct sockaddr *)&addr, sizeof(addr));
	if (r < 0 && errno != EINPROGRESS) {
		close(s);
		return -1;
	}

	if (r < 0) {
		struct pollfd pfds[2];
		pfds[0].fd = s;
		pfds[0].events = POLLOUT;
		pfds[1].fd = link->wake_fds[0];
		pfds[1].events = POLLIN;

		int pr = poll(pfds, 2, LINK_CONNECT_TIMEOUT_MS);
		if (pr <= 0) {
			close(s);
			return -1;
		}

		if (pfds[1].revents & POLLIN) {
			drain_wake(link);
			close(s);
			return -1;
		}

		int sock_err = 0;
		socklen_t len = sizeof(sock_err);
		if (getsockopt(s, SOL_SOCKET, SO_ERROR, &sock_err, &len) < 0 || sock_err != 0) {
			close(s);
			return -1;
		}
	}

	return s;
}

static void run_connection_loop (Link *link)
{
	struct pollfd pfds[2];
	pfds[1].fd = link->wake_fds[0];
	pfds[1].events = POLLIN;

	while (atomic_load_explicit(&link->running, memory_order_acquire)) {

		int pending_tx = ring_has_data(&link->tx);

		pfds[0].fd = link->socket;
		pfds[0].events = POLLIN | (pending_tx ? POLLOUT : 0);

		int r = poll(pfds, 2, LINK_POLL_TIMEOUT_MS);
		if (r <= 0) continue;

		if (pfds[1].revents & POLLIN)
			drain_wake(link);

		if (pfds[0].revents & POLLIN) {
			uint8_t buf[256];
			ssize_t n = recv(link->socket, buf, sizeof(buf), 0);
			if (n <= 0) {
				printf("Link cable: disconnected\n");
				return;
			}
			ring_push_bulk(&link->rx, buf, (uint32_t) n);

		} else if (pfds[0].revents & (POLLHUP | POLLERR)) {
			printf("Link cable: disconnected\n");
			return;
		}


		if (pfds[0].revents & POLLOUT) {
			uint8_t buf[256];
			uint32_t n = ring_pop_bulk(&link->tx, buf, sizeof(buf));
			uint32_t off = 0;
			while (off < n) {
				ssize_t sent = send(link->socket, buf + off, n - off, 0);
				if (sent < 0) {
					if (errno == EAGAIN || errno == EWOULDBLOCK) break;
					printf("Link cable: error while sending\n");
					return;
				}
				off += (uint32_t) sent;
			}
		}
	}
}

static void *link_thread_fn (void *arg)
{
	Link *link = (Link *) arg;

	while (atomic_load_explicit(&link->running, memory_order_acquire)) {

		int fd;
		if (link->mode == LINK_HOST) {
			fd = try_accept(link);
		} else {
			fd = try_connect_once(link);
			if (fd < 0) {
				if (!atomic_load_explicit(&link->running, memory_order_acquire))
					break;
				wait_or_wake(link, LINK_RECONNECT_DELAY_MS);
				continue;
			}
		}

		if (fd < 0) break;

		int one = 1;
		setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
		link->socket = fd;

		ring_reset(&link->tx);
		ring_reset(&link->rx);

		atomic_store_explicit(&link->connected, 1, memory_order_release);
		printf("Link cable: connected\n");

		run_connection_loop(link);

		atomic_store_explicit(&link->connected, 0, memory_order_release);
		close(link->socket);
		link->socket = -1;

		if (atomic_load_explicit(&link->running, memory_order_acquire))
			printf("Link cable: waiting for reconnection...\n");
	}

	if (link->listen_socket >= 0) {
		close(link->listen_socket);
		link->listen_socket = -1;
	}

	return NULL;
}

static int link_init_common (Link *link)
{
	memset(link, 0, sizeof(Link));
	link->socket = -1;
	link->listen_socket = -1;
	atomic_init(&link->running, 1);
	atomic_init(&link->connected, 0);

	if (pipe(link->wake_fds) == -1) {
		fprintf(stderr, "Link cable: error creating pipe\n");
		link->wake_fds[0] = link->wake_fds[1] = -1;
		return 0;
	}
	set_nonblocking(link->wake_fds[0]);
	set_nonblocking(link->wake_fds[1]);
	return 1;
}

int link_host (Link *link, uint16_t port)
{
	if (!link_init_common(link)) return 0;
	link->mode = LINK_HOST;
	link->port = port;

	int s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0) { perror("socket"); return 0; }

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
	set_nonblocking(s);

	link->listen_socket = s;
	printf("Link cable: waiting for connection at port %u...\n", port);

	if (pthread_create(&link->thread, NULL, link_thread_fn, link) != 0) {
		perror("pthread_create");
		close(s);
		close(link->wake_fds[0]);
		close(link->wake_fds[1]);
		link->listen_socket = -1;
		return 0;
	}

	link->active = 1;
	return 1;
}

int link_connect (Link *link, const char *ip, uint16_t port)
{
	struct in_addr tmp;
	if (inet_pton(AF_INET, ip, &tmp) <= 0) {
		fprintf(stderr, "Link cable: IP '%s' is not valid\n", ip);
		return 0;
	}

	if (!link_init_common(link)) return 0;
	link->mode = LINK_CLIENT;
	link->port = port;
	strncpy(link->ip, ip, sizeof(link->ip) - 1);

	printf("Link cable: connecting to %s:%u...\n", ip, port);

	if (pthread_create(&link->thread, NULL, link_thread_fn, link) != 0) {
		perror("pthread_create");
		close(link->wake_fds[0]);
		close(link->wake_fds[1]);
		return 0;
	}

	link->active = 1;
	return 1;
}

void link_close (Link *link)
{
	if (!link || !link->active) return;

	atomic_store_explicit(&link->running, 0, memory_order_release);
	wake_thread(link);
	pthread_join(link->thread, NULL);

	if (link->socket >= 0) close(link->socket);
	if (link->listen_socket >= 0) close(link->listen_socket);
	close(link->wake_fds[0]);
	close(link->wake_fds[1]);

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
	wake_thread(link);
}
