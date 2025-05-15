/* ======= client_local.c ======= */
/* Klient komunikujący się z serwerem przez socket lokalny */

#include "l8_common.h"

// Tworzy socket lokalny
int make_socket(char *name, struct sockaddr_un *addr) { ... }

// Łączy się z lokalnym socketem
int connect_socket(char *name) { ... }

void usage(char *name) { fprintf(stderr, "USAGE: %s socket operand1 operand2 operation \n", name); }

// Przygotowuje tablicę 5 × int32_t z danymi operacji
void prepare_request(char **argv, int32_t data[5]) { ... }

// Wypisuje odpowiedź z serwera
void print_answer(int32_t data[5]) { ... }

int main(int argc, char **argv) {
    // Sprawdzenie argumentów
    // Połączenie z socketem lokalnym
    // Wysłanie danych
    // Odczyt odpowiedzi
    // Wypisanie wyniku
    // Zamknięcie połączenia
}
