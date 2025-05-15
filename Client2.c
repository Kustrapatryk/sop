#include "l8_common.h"                             // Wspólny nagłówek z makrami, funkcjami I/O i sieci

#define MAXBUF 576                                 // Maksymalny rozmiar jednego datagramu (tekst + nagłówki sterujące)

volatile sig_atomic_t last_signal = 0;             // Globalna zmienna zapamiętująca numer ostatniego sygnału (obsługa SIGALRM)

void sigalrm_handler(int sig) { last_signal = sig; } // Handler SIGALRM – ustawia flagę, by pętla wiedziała, że upłynęło 0,5 s

/*------------------------------------------------------------------*/
/* Utworzenie surowego gniazda IPv4/UDP                             */
/*------------------------------------------------------------------*/
int make_socket(void)
{
    int sock;                                      // Lokalne miejsce na deskryptor
    sock = socket(PF_INET, SOCK_DGRAM, 0);         // PF_INET = IPv4, SOCK_DGRAM = UDP
    if (sock < 0)                                  // Jeśli `socket()` zwrócił błąd…
        ERR("socket");                             // … wypisz komunikat i zakończ
    return sock;                                   // Zwróć poprawny deskryptor
}

/*------------------------------------------------------------------*/
/* Krótkie info o użyciu programu                                   */
/*------------------------------------------------------------------*/
void usage(char *name) { fprintf(stderr, "USAGE: %s domain port file \n", name); }

/*------------------------------------------------------------------*/
/* Wyślij buf1 i czekaj maks. 0,5 s na potwierdzenie w buf2          */
/*------------------------------------------------------------------*/
void sendAndConfirm(int fd, struct sockaddr_in addr, char *buf1, char *buf2, ssize_t size)
{
    struct itimerval ts;                            // Struktura timera do SIGALRM
    if (TEMP_FAILURE_RETRY(sendto(fd, buf1, size, 0, &addr, sizeof(addr))) < 0) // Wyślij datagram
        ERR("sendto:");                             // Błąd wysyłki
    memset(&ts, 0, sizeof(struct itimerval));       // Wyzeruj strukturę timera
    ts.it_value.tv_usec = 500000;                   // Ustaw odliczanie: 0,5 s
    setitimer(ITIMER_REAL, &ts, NULL);              // Uruchom odliczanie – po 0,5 s przyjdzie SIGALRM
    last_signal = 0;                                // Skasuj poprzedni sygnał
    /*--------------------------------------------------------------*/
    /* Oczekiwanie (recv) aż przyjdzie poprawne potwierdzenie        */
    /*--------------------------------------------------------------*/
    while (recv(fd, buf2, size, 0) < 0)             // recv() blokuje do przyjścia czegokolwiek
    {
        if (EINTR != errno)                         // Jeśli przerwał inny sygnał niż SIGALRM
            ERR("recv:");                           // to błąd
        if (SIGALRM == last_signal)                 // Jeśli przerwało SIGALRM (timeout) …
            break;                                  // … przerwij pętlę – pakiet się „zgubił”
    }
}

/*------------------------------------------------------------------*/
/* Główna logika klienta: czyta plik → dzieli → wysyła z ACK-ami     */
/*------------------------------------------------------------------*/
void doClient(int fd, struct sockaddr_in addr, int file)
{
    char buf[MAXBUF];                               // Bufor wysyłany
    char buf2[MAXBUF];                              // Bufor na potwierdzenie
    int offset = 2 * sizeof(int32_t);               // Dwa pola sterujące (chunkNo + last?)
    int32_t chunkNo = 0;                            // Numer kolejnego fragmentu
    int32_t last = 0;                               // Flaga „ostatni fragment”
    ssize_t size;                                   // Liczba bajtów rzeczywistego payloadu
    int counter;                                    // Licznik retransmisji

    do                                              // Czytaj plik aż do EOF
    {
        if ((size = bulk_read(file, buf + offset, MAXBUF - offset)) < 0) // Czytaj z pliku
            ERR("read from file:");
        *((int32_t *)buf) = htonl(++chunkNo);       // Zapisz numer fragmentu (sieciowy porządek bajtów)
        if (size < MAXBUF - offset)                 // Jeśli to ostatni kawałek pliku…
        {
            last = 1;                               // … ustaw flagę końca
            memset(buf + offset + size, 0, MAXBUF - offset - size); // Wypełnij resztę zerami
        }
        *(((int32_t *)buf) + 1) = htonl(last);      // Zapisz flagę last (0/1) tuż po chunkNo
        memset(buf2, 0, MAXBUF);                    // Wyzeruj bufor potwierdzenia
        counter = 0;                                // Zresetuj licznik prób
        do
        {
            counter++;                              // Zwiększ liczbę retransmisji
            sendAndConfirm(fd, addr, buf, buf2, MAXBUF); // Wyślij i czekaj na ACK
        } while (*((int32_t *)buf2) != (int32_t)htonl(chunkNo) && counter <= 5); // Dopóki ACK ≠ nr chunk lub <5 prób
        if (*((int32_t *)buf2) != (int32_t)htonl(chunkNo) && counter > 5) // Po 5 niepowodzeniach…
            break;                                  // … zrezygnuj z dalszego wysyłania
    } while (size == MAXBUF - offset);              // Kontynuuj dopóki pełne bufory (EOF kończy pętlę)
}

/*------------------------------------------------------------------*/
/* main() – parsowanie argumentów i start klienta                    */
/*------------------------------------------------------------------*/
int main(int argc, char **argv)
{
    int fd, file;                                   // Deskryptory: socket i plik
    struct sockaddr_in addr;                        // Adres serwera UDP

    if (argc != 4)                                  // Sprawdź liczbę argumentów
    {
        usage(argv[0]);                             // Wypisz help
        return EXIT_FAILURE;                        // Błędne użycie → exit
    }
    if (sethandler(SIG_IGN, SIGPIPE))               // Ignoruj SIGPIPE (tu raczej nie wystąpi w UDP, ale nawyk)
        ERR("Seting SIGPIPE:");
    if (sethandler(sigalrm_handler, SIGALRM))       // Obsłuż SIGALRM (timeout 0,5 s)
        ERR("Seting SIGALRM:");
    if ((file = TEMP_FAILURE_RETRY(open(argv[3], O_RDONLY))) < 0) // Otwórz plik do czytania
        ERR("open:");
    fd = make_socket();                             // Utwórz gniazdo UDP
    addr = make_address(argv[1], argv[2]);          // Rozwiąż domenę + port do sockaddr_in
    doClient(fd, addr, file);                       // Jedź z wysyłaniem
    if (TEMP_FAILURE_RETRY(close(fd)) < 0)          // Zamknij socket
        ERR("close");
    if (TEMP_FAILURE_RETRY(close(file)) < 0)        // Zamknij plik
        ERR("close");
    return EXIT_SUCCESS;                            // Sukces
}
