#define _GNU_SOURCE                             // Wymuś rozszerzenia GNU (przydatne np. dla POLL/EPOLL)
#include <errno.h>                              // Stałe i zmienne błędów `errno`
#include <fcntl.h>                              // Operacje na deskryptorach (flagi O_NONBLOCK itp.)
#include <netdb.h>                              // getaddrinfo / gai_strerror
#include <netinet/in.h>                         // Struktury IPv4/IPv6, htons/htonl
#include <signal.h>                             // Obsługa sygnałów POSIX
#include <stdio.h>                              // Standard I/O: printf, perror …
#include <stdlib.h>                             // exit, malloc, atoi …
#include <string.h>                             // memset, strncpy, memset
#include <sys/epoll.h>                          // Mechanizm epoll
#include <sys/socket.h>                         // socket, bind, listen, connect …
#include <sys/time.h>                           // time-related structs (opcjonalne)
#include <sys/types.h>                          // Typy POSIX, używane razem z sys/socket.h
#include <sys/un.h>                             // Gniazda domeny UNIX
#include <unistd.h>                             // close, unlink, read, write …

#ifndef TEMP_FAILURE_RETRY                      // Makro gwarantujące ponawianie przerwanej operacji
#define TEMP_FAILURE_RETRY(expression)          \
    (__extension__({                            \
        long int __result;                      \
        do                                      \
            __result = (long int)(expression);  /* próbuj wykonać wyrażenie */ \
        while (__result == -1L && errno == EINTR); /* powtarzaj, jeśli przerwał sygnał */ \
        __result;                               /* zwróć rezultat */ \
    }))
#endif

#define ERR(source) (perror(source),            /* Wypisz błąd na stderr … */ \
                     fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), /* …z plikiem i linią */ \
                     exit(EXIT_FAILURE))        /* i zakończ proces kodem 1 */

int sethandler(void (*f)(int), int sigNo)       // Ustaw własny handler sygnału
{
    struct sigaction act;                       // Struktura definicji akcji
    memset(&act, 0, sizeof(struct sigaction));  // Wyzeruj
    act.sa_handler = f;                         // Ustaw funkcję obsługi
    if (-1 == sigaction(sigNo, &act, NULL))     // Zarejestruj handler
        return -1;                              // Błąd → zwróć -1
    return 0;                                   // Sukces
}

int make_local_socket(char *name, struct sockaddr_un *addr)    // Utwórz deskryptor gniazda UNIX
{
    int socketfd;
    if ((socketfd = socket(PF_UNIX, SOCK_STREAM, 0)) < 0)      // PF_UNIX + strumieniowe
        ERR("socket");
    memset(addr, 0, sizeof(struct sockaddr_un));               // Wyzeruj strukturę adresu
    addr->sun_family = AF_UNIX;                                // Rodzina UNIX
    strncpy(addr->sun_path, name, sizeof(addr->sun_path) - 1); // Ścieżka pliku gniazda
    return socketfd;                                           // Zwróć deskryptor
}

int connect_local_socket(char *name)           // Połącz się z gniazdem lokalnym serwera
{
    struct sockaddr_un addr;
    int socketfd;
    socketfd = make_local_socket(name, &addr); // Utwórz deskryptor i strukturę adresu
    if (connect(socketfd, (struct sockaddr *)&addr, SUN_LEN(&addr)) < 0) // Podłącz
    {
        ERR("connect");
    }
    return socketfd;                           // Zwróć już połączony deskryptor
}

int bind_local_socket(char *name, int backlog_size)            // Przygotuj gniazdo serwerowe UNIX
{
    struct sockaddr_un addr;
    int socketfd;
    if (unlink(name) < 0 && errno != ENOENT)    // Usuń stary plik, jeśli istnieje
        ERR("unlink");
    socketfd = make_local_socket(name, &addr);  // Utwórz deskryptor
    if (bind(socketfd, (struct sockaddr *)&addr, SUN_LEN(&addr)) < 0) // Powiąż z adresem
        ERR("bind");
    if (listen(socketfd, backlog_size) < 0)     // Zamień w gniazdo nasłuchujące
        ERR("listen");
    return socketfd;                            // Zwróć deskryptor nasłuchu
}

int make_tcp_socket(void)                      // Utwórz surowe gniazdo IPv4/TCP
{
    int sock;
    sock = socket(PF_INET, SOCK_STREAM, 0);    // PF_INET + SOCK_STREAM => TCP
    if (sock < 0)
        ERR("socket");
    return sock;
}

struct sockaddr_in make_address(char *address, char *port) // Zbuduj sockaddr_in z napisu host+port
{
    int ret;
    struct sockaddr_in addr;
    struct addrinfo *result;
    struct addrinfo hints = {};               // Wypełnij „podpowiedzi” dla getaddrinfo
    hints.ai_family = AF_INET;                // Tylko IPv4
    if ((ret = getaddrinfo(address, port, &hints, &result)))   // Rozwiąż host/port
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret)); // Błąd → log
        exit(EXIT_FAILURE);
    }
    addr = *(struct sockaddr_in *)(result->ai_addr);           // Skopiuj wynik
    freeaddrinfo(result);                       // Zwolnij listę wyników
    return addr;                                // Zwróć gotową strukturę
}

int connect_tcp_socket(char *name, char *port) // Połącz z serwerem TCP (domena + port)
{
    struct sockaddr_in addr;
    int socketfd;
    socketfd = make_tcp_socket();               // Deskryptor
    addr = make_address(name, port);            // Adres docelowy
    if (connect(socketfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) < 0) // 3-way handshake
    {
        ERR("connect");
    }
    return socketfd;                            // Zwróć deskryptor klienta
}

int bind_tcp_socket(uint16_t port, int backlog_size) // Powiąż gniazdo serwerowe z portem
{
    struct sockaddr_in addr;
    int socketfd, t = 1;
    socketfd = make_tcp_socket();               // Utwórz deskryptor
    memset(&addr, 0, sizeof(struct sockaddr_in)); // Wyzeruj strukturę
    addr.sin_family = AF_INET;                  // IPv4
    addr.sin_port = htons(port);                // Port w big-endian
    addr.sin_addr.s_addr = htonl(INADDR_ANY);   // Nasłuchuj na wszystkich interfejsach
    if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &t, sizeof(t))) // Pozwól na szybkie ponowne bind
        ERR("setsockopt");
    if (bind(socketfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) // Powiąż z adresem
        ERR("bind");
    if (listen(socketfd, backlog_size) < 0)     // Nasłuchuj
        ERR("listen");
    return socketfd;                            // Zwróć deskryptor nasłuchu
}

int add_new_client(int sfd)                     // Przyjmij nowe połączenie
{
    int nfd;
    if ((nfd = TEMP_FAILURE_RETRY(accept(sfd, NULL, NULL))) < 0) // accept może przerwać sygnał
    {
        if (EAGAIN == errno || EWOULDBLOCK == errno)             // Brak queued connections
            return -1;
        ERR("accept");
    }
    return nfd;                                // Zwróć deskryptor klienta
}

ssize_t bulk_read(int fd, char *buf, size_t count) // Gwarantowane odczytanie „count” bajtów
{
    int c;
    size_t len = 0;
    do
    {
        c = TEMP_FAILURE_RETRY(read(fd, buf, count)); // Czytaj z ponawianiem
        if (c < 0)
            return c;                                // Błąd
        if (0 == c)
            return len;                              // EOF
        buf += c;                                    // Przesuń wskaźnik bufora
        len += c;                                    // Zlicz już przeczytane
        count -= c;                                  // Pozostało do odczytu
    } while (count > 0);
    return len;                                      // Zwróć liczbę bajtów
}

ssize_t bulk_write(int fd, char *buf, size_t count) // Zapisz dokładnie „count” bajtów
{
    int c;
    size_t len = 0;
    do
    {
        c = TEMP_FAILURE_RETRY(write(fd, buf, count)); // Pisz z ponawianiem
        if (c < 0)
            return c;                                  // Błąd
        buf += c;                                      // Przesuń bufor
        len += c;                                      // Zlicz zapisane
        count -= c;                                    // Zmniejsz pozostałe
    } while (count > 0);
    return len;                                        // Zwróć liczbę bajtów
}
