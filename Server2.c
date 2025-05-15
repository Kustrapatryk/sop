#include "l8_common.h"                             // Nagłówek z funkcjami sieciowymi, I/O i makrami

#define BACKLOG 3                                  // Dla ewentualnego TCP; tu nieużywane, ale pozostawione
#define MAXBUF 576                                 // Maksymalny rozmiar datagramu identyczny z klientem
#define MAXADDR 5                                  // Maksymalnie obsługujemy pięć jednoczesnych transmisji

/*------------------------------------------------------------------*/
/* Struktura stanu dla pojedynczego nadawcy (zident. po sockaddr)    */
/*------------------------------------------------------------------*/
struct connections
{
    int free;                                      // Czy slot jest wolny (1) czy zajęty (0)
    int32_t chunkNo;                               // Ostatni numer fragmentu poprawnie odebranego
    struct sockaddr_in addr;                       // Adres/port klienta
};

/*------------------------------------------------------------------*/
/* Utwórz gniazdo dowolnego typu/rodziny                             */
/*------------------------------------------------------------------*/
int make_socket(int domain, int type)
{
    int sock = socket(domain, type, 0);            // Wywołanie socket()
    if (sock < 0)                                  // Obsługa błędu
        ERR("socket");
    return sock;                                   // Poprawny deskryptor
}

/*------------------------------------------------------------------*/
/* Powiąż gniazdo INET z danym portem (UDP lub TCP)                  */
/*------------------------------------------------------------------*/
int bind_inet_socket(uint16_t port, int type)
{
    struct sockaddr_in addr;                       // Lokalny adres
    int socketfd, t = 1;                           // Deskryptor + flaga SO_REUSEADDR
    socketfd = make_socket(PF_INET, type);         // Utwórz gniazdo IPv4
    memset(&addr, 0, sizeof(struct sockaddr_in));  // Wyzeruj strukturę
    addr.sin_family = AF_INET;                     // Rodzina
    addr.sin_port = htons(port);                   // Numer portu sieciowo
    addr.sin_addr.s_addr = htonl(INADDR_ANY);      // Nasłuchuj na wszystkich interfejsach
    if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &t, sizeof(t))) // Pozwól na szybkie ponowne bindowanie
        ERR("setsockopt");
    if (bind(socketfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) // Powiąż z adresem
        ERR("bind");
    if (SOCK_STREAM == type)                       // Jeśli to TCP (tu nie) – listen()
        if (listen(socketfd, BACKLOG) < 0)
            ERR("listen");
    return socketfd;                               // Zwróć deskryptor
}

/*------------------------------------------------------------------*/
/* Znajdź (lub załóż) slot dla nadawcy – zwróć indeks w tablicy      */
/*------------------------------------------------------------------*/
int findIndex(struct sockaddr_in addr, struct connections con[MAXADDR])
{
    int i, empty = -1, pos = -1;                   // i = iterator; empty = pierwszy wolny slot; pos = znaleziony
    for (i = 0; i < MAXADDR; i++)                  // Przeszukaj tablicę slotów
    {
        if (con[i].free)                           // Zapamiętaj wolny slot (gdyby potrzebny)
            empty = i;
        else if (0 == memcmp(&addr, &(con[i].addr), sizeof(struct sockaddr_in))) // Adres już obecny?
        {
            pos = i;                               // Tak – zwróć jego indeks
            break;
        }
    }
    if (-1 == pos && empty != -1)                  // Jeśli nie znaleziono, a jest wolne miejsce…
    {
        con[empty].free = 0;                       // … zajmij slot
        con[empty].chunkNo = 0;                    // … zeruj licznik fragmentów
        con[empty].addr = addr;                    // … zapisz adres klienta
        pos = empty;                               // … i używaj tego indeksu
    }
    return pos;                                   // -1 jeśli brak miejsca (6-ta transmisja)
}

/*------------------------------------------------------------------*/
/* Pętla główna serwera UDP                                         */
/*------------------------------------------------------------------*/
void doServer(int fd)
{
    struct sockaddr_in addr;                       // Adres nadawcy
    struct connections con[MAXADDR];               // Tablica slotów
    char buf[MAXBUF];                              // Bufor odbiorczy
    socklen_t size = sizeof(addr);                 // Rozmiar struct sockaddr_in
    int i;                                         // Uniwersalny iterator
    int32_t chunkNo, last;                         // Pola nagłówka
    for (i = 0; i < MAXADDR; i++)                  // Na start – oznacz wszystkie sloty jako wolne
        con[i].free = 1;
    for (;;)                                      // Nieskończona pętla obsługi
    {
        if (TEMP_FAILURE_RETRY(recvfrom(fd, buf, MAXBUF, 0, &addr, &size) < 0)) // Odbierz datagram
            ERR("read:");                          // recvfrom zwrócił błąd
        if ((i = findIndex(addr, con)) >= 0)       // Jeśli mamy (lub założyliśmy) slot dla tego nadawcy…
        {
            chunkNo = ntohl(*((int32_t *)buf));    // Numer fragmentu w nagłówku
            last = ntohl(*(((int32_t *)buf) + 1)); // Flaga „ostatni”
            if (chunkNo > con[i].chunkNo + 1)      // Przeskoczył numer → pomiń (czekamy na brakujące)
                continue;
            else if (chunkNo == con[i].chunkNo + 1) // Otrzymaliśmy „następny z kolei” fragment
            {
                if (last)                          // Jeśli to ostatni…
                {
                    printf("Last Part %d\n%s\n", chunkNo, buf + 2 * sizeof(int32_t)); // Wypisz i oznacz koniec
                    con[i].free = 1;               // Zwolnij slot (cały plik odebrany)
                }
                else                               // W przeciwnym razie zwykły fragment
                    printf("Part %d\n%s\n", chunkNo, buf + 2 * sizeof(int32_t));
                con[i].chunkNo++;                  // Zaktualizuj ostatni odebrany numer
            }
            if (TEMP_FAILURE_RETRY(sendto(fd, buf, MAXBUF, 0, &addr, size)) < 0) // Odeślij ACK (echo nagłówka)
            {
                if (EPIPE == errno)                // Klient zamknięty – zwolnij slot
                    con[i].free = 1;
                else
                    ERR("send:");
            }
        }                                          // Jeśli brak slotu (6-ty klient) – ignorujemy pakiet
    }
}

/*------------------------------------------------------------------*/
/* Szybka pomoc dla użytkownika                                     */
/*------------------------------------------------------------------*/
void usage(char *name) { fprintf(stderr, "USAGE: %s port\n", name); }

/*------------------------------------------------------------------*/
/* main() – parsowanie argumentów i start serwera                    */
/*------------------------------------------------------------------*/
int main(int argc, char **argv)
{
    int fd;                                        // Deskryptor gniazda UDP
    if (argc != 2)                                 // Wymagamy tylko numeru portu
    {
        usage(argv[0]);                            // Help
        return EXIT_FAILURE;
    }
    if (sethandler(SIG_IGN, SIGPIPE))              // Ignoruj SIGPIPE (brak w UDP, ale „standardowo”)
        ERR("Seting SIGPIPE:");
    fd = bind_inet_socket(atoi(argv[1]), SOCK_DGRAM); // Powiąż gniazdo UDP z podanym portem
    doServer(fd);                                  // Wejdź w pętlę główną (funkcja nie wraca)
    if (TEMP_FAILURE_RETRY(close(fd)) < 0)         // Zamknij socket (wykonane dopiero po przerwaniu pętli)
        ERR("close");
    fprintf(stderr, "Server has terminated.\n");   // Kontrolny log
    return EXIT_SUCCESS;                           // Koniec programu
}
