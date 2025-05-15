/* ======= server1.c ======= */
/* Serwer obsługuje lokalne i TCP sockety jednocześnie */

#include "l8_common.h"

#define BACKLOG 3        // Kolejka oczekujących połączeń
#define MAX_EVENTS 16    // Maksymalna liczba zdarzeń epolla

volatile sig_atomic_t do_work = 1; // Flaga działania serwera

void sigint_handler(int sig) { do_work = 0; } // Obsługa Ctrl+C

void usage(char *name) { fprintf(stderr, "USAGE: %s socket port\n", name); }

// Oblicza wynik działania i ustawia status
void calculate(int32_t data[5]) { ... }

// Główna pętla serwera – epoll + akceptowanie i obsługa klientów
void doServer(int local_listen_socket, int tcp_listen_socket) { ... }

int main(int argc, char **argv) {
    // Sprawdzenie liczby argumentów
    // Ustawienie handlerów sygnałów
    // Tworzenie socketów lokalnych i TCP
    // Ustawienie ich w tryb nieblokujący
    // Uruchomienie pętli serwera
    // Sprzątanie (zamknięcie socketów, unlink lokalnego pliku)
}
