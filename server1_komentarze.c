#include "l8_common.h"                    // Wspólne narzędzia/definicje

#define BACKLOG 3                         // Kolejka oczekujących połączeń
#define MAX_EVENTS 16                     // Maks. zdarzeń odczytanych z epolla

volatile sig_atomic_t do_work = 1;        // Flaga sterująca główną pętlą

void sigint_handler(int sig) { do_work = 0; }  // SIGINT → przerwij serwer

void usage(char *name) { fprintf(stderr, "USAGE: %s socket port\n", name); } // Info o argumentach

void calculate(int32_t data[5])           // Oblicz wynik + ustaw pola odpowiedzi
{
    int32_t op1, op2, result = -1, status = 1; // Lokalne zmienne
    op1 = ntohl(data[0]);                // Konwertuj operand1 do host-byte-order
    op2 = ntohl(data[1]);                // Konwertuj operand2
    switch ((char)ntohl(data[3]))        // Rozpoznaj operator
    {
        case '+':
            result = op1 + op2;          // Dodawanie
            break;
        case '-':
            result = op1 - op2;          // Odejmowanie
            break;
        case '*':
            result = op1 * op2;          // Mnożenie
            break;
        case '/':
            if (!op2)                    // Dzielenie przez 0 → błąd
                status = 0;
            else
                result = op1 / op2;      // Dzielenie
            break;
        default:
            status = 0;                  // Nieznany operator
    }
    data[4] = htonl(status);             // Zapisz status w sieciowym byte-order
    data[2] = htonl(result);             // Zapisz wynik
}

void doServer(int local_listen_socket, int tcp_listen_socket) // Pętla obsługi serwera
{
    int epoll_descriptor;
    if ((epoll_descriptor = epoll_create1(0)) < 0) // Utwórz epoll
    {
        ERR("epoll_create:");
    }
    struct epoll_event event, events[MAX_EVENTS];   // Pojedyncze + tablica zdarzeń
    event.events = EPOLLIN;                         // Interesuje nas gotowość do czytania
    event.data.fd = local_listen_socket;            // Klucz = fd gniazda UNIX
    if (epoll_ctl(epoll_descriptor, EPOLL_CTL_ADD, local_listen_socket, &event) == -1)
    {
        perror("epoll_ctl: listen_sock");
        exit(EXIT_FAILURE);
    }

    event.data.fd = tcp_listen_socket;              // Dodaj gniazdo TCP
    if (epoll_ctl(epoll_descriptor, EPOLL_CTL_ADD, tcp_listen_socket, &event) == -1)
    {
        perror("epoll_ctl: listen_sock");
        exit(EXIT_FAILURE);
    }

    int nfds;                                       // Liczba gotowych zdarzeń
    int32_t data[5];                                // Bufor na pakiet kalkulatora
    ssize_t size;
    sigset_t mask, oldmask;                         // Maska blokowanych sygnałów
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);                       // Zablokuj SIGINT w czasie epolla
    sigprocmask(SIG_BLOCK, &mask, &oldmask);

    while (do_work)                                 // Główna pętla
    {
        if ((nfds = epoll_pwait(epoll_descriptor, events, MAX_EVENTS, -1, &oldmask)) > 0)
        {                                           // Czekaj bez timeoutu, odblokuj sygnały
            for (int n = 0; n < nfds; n++)          // Iteruj po gotowych zdarzeniach
            {
                int client_socket = add_new_client(events[n].data.fd); // accept()
                if ((size = bulk_read(client_socket, (char *)data, sizeof(int32_t[5]))) < 0)
                    ERR("read:");
                if (size == (int)sizeof(int32_t[5])) // Odebrano pełny blok?
                {
                    calculate(data);                // Przetwarzaj
                    if (bulk_write(client_socket, (char *)data, sizeof(int32_t[5])) < 0 && errno != EPIPE)
                        ERR("write:");
                }
                if (TEMP_FAILURE_RETRY(close(client_socket)) < 0) // Zamknij klienta
                    ERR("close");
            }
        }
        else
        {
            if (errno == EINTR)                     // Przerwano sygnałem → spróbuj ponownie
                continue;
            ERR("epoll_pwait");                     // Inne błędy
        }
    }
    if (TEMP_FAILURE_RETRY(close(epoll_descriptor)) < 0) // Posprzątaj epoll
        ERR("close");
    sigprocmask(SIG_UNBLOCK, &mask, NULL);          // Przywróć maskę sygnałów
}

int main(int argc, char **argv)
{
    int local_listen_socket, tcp_listen_socket;     // Deskryptory nasłuchu
    int new_flags;
    if (argc != 3)                                 // Wymagamy 2 parametrów
    {
        usage(argv[0]);
        return EXIT_FAILURE;
    }
    if (sethandler(SIG_IGN, SIGPIPE))              // Ignoruj SIGPIPE (zapis w zamknięte gniazdo)
        ERR("Seting SIGPIPE:");
    if (sethandler(sigint_handler, SIGINT))        // Obsłuż SIGINT
        ERR("Seting SIGINT:");

    local_listen_socket = bind_local_socket(argv[1], BACKLOG); // Gniazdo UNIX
    new_flags = fcntl(local_listen_socket, F_GETFL) | O_NONBLOCK; // Ustaw O_NONBLOCK
    fcntl(local_listen_socket, F_SETFL, new_flags);

    tcp_listen_socket = bind_tcp_socket(atoi(argv[2]), BACKLOG); // Gniazdo TCP
    new_flags = fcntl(tcp_listen_socket, F_GETFL) | O_NONBLOCK;  // Także nie-blokujące
    fcntl(tcp_listen_socket, F_SETFL, new_flags);

    doServer(local_listen_socket, tcp_listen_socket);   // Start pętli serwera

    if (TEMP_FAILURE_RETRY(close(local_listen_socket)) < 0) // Zamknij oba gniazda
        ERR("close");
    if (unlink(argv[1]) < 0)                         // Usuń plik gniazda UNIX
        ERR("unlink");
    if (TEMP_FAILURE_RETRY(close(tcp_listen_socket)) < 0)
        ERR("close");
    fprintf(stderr, "Server has terminated.\n");     // Log kontrolny
    return EXIT_SUCCESS;
}
