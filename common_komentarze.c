/* ======= common.h ======= */
/* Nagłówkowa biblioteka wspólna dla klienta i serwera */

#define _GNU_SOURCE // Włącza rozszerzenia GNU

#include <errno.h>       // Obsługa błędów
#include <fcntl.h>       // Manipulacja deskryptorami plików
#include <netdb.h>       // getaddrinfo itd.
#include <netinet/in.h>  // Struktury sieciowe IPv4
#include <signal.h>      // Obsługa sygnałów
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>   // Obsługa epolla
#include <sys/socket.h>  // socket, bind, listen
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>      // Gniazda UNIX
#include <unistd.h>      // close, read, write

#ifndef TEMP_FAILURE_RETRY
#define TEMP_FAILURE_RETRY(expression) \
    (__extension__({ long int __result; do __result = (long int)(expression); \
    while (__result == -1L && errno == EINTR); __result; }))
#endif

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

// Obsługa sygnałów
int sethandler(void (*f)(int), int sigNo) { ... }

// Tworzenie socketu lokalnego
int make_local_socket(char *name, struct sockaddr_un *addr) { ... }

// Połączenie do lokalnego socketu
int connect_local_socket(char *name) { ... }

// Bindowanie lokalnego socketu i ustawienie go w tryb nasłuchu
int bind_local_socket(char *name, int backlog_size) { ... }

// Tworzy socket TCP
int make_tcp_socket(void) { ... }

// Tworzy strukturę adresową TCP
struct sockaddr_in make_address(char *address, char *port) { ... }

// Łączy się z serwerem TCP
int connect_tcp_socket(char *name, char *port) { ... }

// Bindowanie TCP socketu
int bind_tcp_socket(uint16_t port, int backlog_size) { ... }

// Akceptacja połączenia
int add_new_client(int sfd) { ... }

// Czytanie dokładnie 'count' bajtów
ssize_t bulk_read(int fd, char *buf, size_t count) { ... }

// Zapis dokładnie 'count' bajtów
ssize_t bulk_write(int fd, char *buf, size_t count) { ... }
