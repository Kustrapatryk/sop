/* ======= client_tcp.c ======= */
/* Klient TCP łączący się z serwerem przez IP i port */

#include "l8_common.h"

// Przygotowanie danych do wysyłki (5 intów)
void prepare_request(char **argv, int32_t data[5]) { ... }

// Wyświetlenie odpowiedzi z serwera
void print_answer(int32_t data[5]) { ... }

void usage(char *name) { fprintf(stderr, "USAGE: %s domain port operand1 operand2 operation \n", name); }

int main(int argc, char **argv) {
    // Sprawdzenie argumentów
    // Połączenie TCP z serwerem
    // Wysłanie danych
    // Odbiór odpowiedzi
    // Wyświetlenie wyniku
    // Zamknięcie połączenia
}
