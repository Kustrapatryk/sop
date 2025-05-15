#include "l8_common.h"                         // Wspólne funkcje/makra

int make_socket(char *name, struct sockaddr_un *addr) // Stwórz lokalne gniazdo UNIX (klient)
{
    int socketfd;
    if ((socketfd = socket(PF_UNIX, SOCK_STREAM, 0)) < 0) // Strumieniowe, domena UNIX
        ERR("socket");
    memset(addr, 0, sizeof(struct sockaddr_un));         // Wyzeruj strukturę
    addr->sun_family = AF_UNIX;                          // Rodzina
    strncpy(addr->sun_path, name, sizeof(addr->sun_path) - 1); // Ścieżka pliku
    return socketfd;
}

int connect_socket(char *name)               // Połącz klienta z serwerem UNIX
{
    struct sockaddr_un addr;
    int socketfd;
    socketfd = make_socket(name, &addr);     // Uzyskaj deskryptor + adres
    if (connect(socketfd, (struct sockaddr *)&addr, SUN_LEN(&addr)) < 0) // Connect
    {
        ERR("connect");
    }
    return socketfd;                         // Zwróć połączony deskryptor
}

void usage(char *name) { fprintf(stderr, "USAGE: %s socket operand1 operand2 operation \n", name); } // Help

void prepare_request(char **argv, int32_t data[5]) // Wypełnij pakiet żądania
{
    data[0] = htonl(atoi(argv[2]));        // operand1
    data[1] = htonl(atoi(argv[3]));        // operand2
    data[2] = htonl(0);                    // placeholder na wynik
    data[3] = htonl((int32_t)(argv[4][0])); // operator jako int32
    data[4] = htonl(1);                    // status domyślnie = 1
}

void print_answer(int32_t data[5])          // Wypisz rezultat
{
    if (ntohl(data[4]))                    // status OK?
        printf("%d %c %d = %d\n", ntohl(data[0]), (char)ntohl(data[3]), ntohl(data[1]), ntohl(data[2]));
    else
        printf("Operation impossible\n");  // Błąd (np. dzielenie przez 0)
}

int main(int argc, char **argv)
{
    int fd;
    int32_t data[5];
    if (argc != 5)                         // Sprawdź argumenty
    {
        usage(argv[0]);
        return EXIT_FAILURE;
    }
    fd = connect_socket(argv[1]);          // Nawiąż połączenie
    prepare_request(argv, data);           // Zbuduj pakiet
    if (bulk_write(fd, (char *)data, sizeof(int32_t[5])) < 0) // Wyślij
        ERR("write:");
    if (bulk_read(fd, (char *)data, sizeof(int32_t[5])) < (int)sizeof(int32_t[5])) // Odbierz odpowiedź
        ERR("read:");
    print_answer(data);                    // Wyświetl wynik
    if (TEMP_FAILURE_RETRY(close(fd)) < 0) // Zamknij deskryptor
        ERR("close");
    return EXIT_SUCCESS;
}
