/*
 * ej05_eco_servidor.c
 *
 * Ejercicio 5 - Servidor de eco TCP iterativo.
 *
 * Uso: ./ej05_eco_servidor <puerto> [noreuse]
 *
 * 1. Atiende a los clientes de a uno por vez.
 * 2. Devuelve todo lo que recibe hasta que el cliente cierre.
 * 3. Muestra cuándo se conecta y cuándo se desconecta cada cliente.
 * 4. Garantiza enviar todos los bytes aunque send() envíe menos (enviar_todo).
 * 5. Con "noreuse" NO activa SO_REUSEADDR (para el experimento de TIME_WAIT).
 *
 * Decisión de diseño: se intenta un socket IPv6 "dual stack" (IPV6_V6ONLY = 0)
 * ligado a "::", así acepta clientes IPv6 y también IPv4 (estos últimos
 * aparecen como direcciones mapeadas ::ffff:a.b.c.d). Esto permite que el
 * cliente pruebe cualquier dirección que devuelva getaddrinfo().
 * Si el sistema no soporta IPv6, se usa un socket IPv4 común.
 *
 * Compilar: gcc -Wall -Wextra -o ej05_eco_servidor ej05_eco_servidor.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define BACKLOG 10
#define TAM_BUF 4096

static int parsear_puerto(const char *s)
{
    char *fin;
    errno = 0;
    long v = strtol(s, &fin, 10);
    if (errno != 0 || *s == '\0' || *fin != '\0' || v < 1 || v > 65535)
        return -1;
    return (int)v;
}

static void uso(const char *prog)
{
    fprintf(stderr, "Uso: %s <puerto> [noreuse]\n", prog);
}

/*
 * Envía len bytes completos. send() puede escribir menos de lo pedido
 * (envío parcial, p. ej. si el buffer de envío del kernel está lleno),
 * por eso se repite hasta completar.
 * MSG_NOSIGNAL: si el cliente ya cerró, send() devuelve EPIPE en lugar
 * de matar al proceso con la señal SIGPIPE.
 */
static int enviar_todo(int fd, const char *buf, size_t len)
{
    size_t enviado = 0;
    while (enviado < len) {
        ssize_t n = send(fd, buf + enviado, len - enviado, MSG_NOSIGNAL);
        if (n == -1) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        enviado += (size_t)n;
    }
    return 0;
}

/* Arma "IP:puerto" de cualquier familia usando getnameinfo() numérico. */
static void dir_a_texto(const struct sockaddr *sa, socklen_t len,
                        char *out, size_t outlen)
{
    char ip[NI_MAXHOST], port[NI_MAXSERV];
    int rc = getnameinfo(sa, len, ip, sizeof ip, port, sizeof port,
                         NI_NUMERICHOST | NI_NUMERICSERV);
    if (rc != 0)
        snprintf(out, outlen, "(desconocida: %s)", gai_strerror(rc));
    else
        snprintf(out, outlen, "[%s]:%s", ip, port);
}

/* Atiende a un cliente: eco hasta que cierre. Devuelve bytes devueltos. */
static long atender(int fd)
{
    char buf[TAM_BUF];
    long total = 0;

    for (;;) {
        ssize_t n = recv(fd, buf, sizeof buf, 0);
        if (n == 0)
            break;                      /* El cliente cerró (FIN recibido) */
        if (n == -1) {
            if (errno == EINTR)
                continue;
            perror("recv");
            break;
        }
        if (enviar_todo(fd, buf, (size_t)n) == -1) {
            perror("send");
            break;
        }
        total += n;
    }
    return total;
}

int main(int argc, char *argv[])
{
    if (argc < 2 || argc > 3) {
        uso(argv[0]);
        return EXIT_FAILURE;
    }
    int puerto = parsear_puerto(argv[1]);
    if (puerto == -1) {
        fprintf(stderr, "Puerto inválido: '%s'\n", argv[1]);
        uso(argv[0]);
        return EXIT_FAILURE;
    }
    int reusar = 1;
    if (argc == 3) {
        if (strcmp(argv[2], "noreuse") != 0) {
            fprintf(stderr, "Segundo argumento desconocido: '%s'\n", argv[2]);
            uso(argv[0]);
            return EXIT_FAILURE;
        }
        reusar = 0;
    }

    /* Primero se intenta IPv6 dual stack; si el sistema no tiene IPv6
     * (EAFNOSUPPORT) se cae a un socket IPv4 normal. */
    int familia = AF_INET6;
    int fd_escucha = socket(AF_INET6, SOCK_STREAM, 0);
    if (fd_escucha == -1 && errno == EAFNOSUPPORT) {
        fprintf(stderr, "Aviso: sin soporte IPv6, se usa sólo IPv4\n");
        familia = AF_INET;
        fd_escucha = socket(AF_INET, SOCK_STREAM, 0);
    }
    if (fd_escucha == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }

    /* Dual stack: que el socket IPv6 acepte también clientes IPv4. */
    if (familia == AF_INET6) {
        int no = 0;
        if (setsockopt(fd_escucha, IPPROTO_IPV6, IPV6_V6ONLY, &no, sizeof no) == -1) {
            perror("setsockopt(IPV6_V6ONLY)");
            close(fd_escucha);
            return EXIT_FAILURE;
        }
    }

    /* SO_REUSEADDR permite hacer bind() a un puerto que tiene conexiones
     * viejas en TIME_WAIT. Con "noreuse" se omite para ver el error. */
    if (reusar) {
        int si = 1;
        if (setsockopt(fd_escucha, SOL_SOCKET, SO_REUSEADDR, &si, sizeof si) == -1) {
            perror("setsockopt(SO_REUSEADDR)");
            close(fd_escucha);
            return EXIT_FAILURE;
        }
    }

    /* Dirección comodín + puerto, según la familia elegida.
     * Requisito: estructuras en cero y puerto en orden de red. */
    int rc_bind;
    if (familia == AF_INET6) {
        struct sockaddr_in6 dir6;
        memset(&dir6, 0, sizeof dir6);
        dir6.sin6_family = AF_INET6;
        dir6.sin6_addr   = in6addr_any;           /* "::" */
        dir6.sin6_port   = htons((uint16_t)puerto);
        rc_bind = bind(fd_escucha, (struct sockaddr *)&dir6, sizeof dir6);
    } else {
        struct sockaddr_in dir4;
        memset(&dir4, 0, sizeof dir4);
        dir4.sin_family      = AF_INET;
        dir4.sin_addr.s_addr = htonl(INADDR_ANY);  /* "0.0.0.0" */
        dir4.sin_port        = htons((uint16_t)puerto);
        rc_bind = bind(fd_escucha, (struct sockaddr *)&dir4, sizeof dir4);
    }
    if (rc_bind == -1) {
        perror("bind");
        close(fd_escucha);
        return EXIT_FAILURE;
    }
    if (listen(fd_escucha, BACKLOG) == -1) {
        perror("listen");
        close(fd_escucha);
        return EXIT_FAILURE;
    }

    printf("Servidor de eco en %s:%d (SO_REUSEADDR %s). Ctrl+C para terminar.\n",
           familia == AF_INET6 ? "[::]" : "0.0.0.0", puerto, reusar ? "activado" : "DESACTIVADO");
    fflush(stdout);

    for (;;) {
        struct sockaddr_storage cli;          /* Alcanza para IPv4 e IPv6 */
        socklen_t cli_len = sizeof cli;
        memset(&cli, 0, sizeof cli);

        int fd_cli = accept(fd_escucha, (struct sockaddr *)&cli, &cli_len);
        if (fd_cli == -1) {
            if (errno == EINTR)
                continue;
            perror("accept");
            continue;
        }

        char quien[NI_MAXHOST + NI_MAXSERV + 8];
        dir_a_texto((struct sockaddr *)&cli, cli_len, quien, sizeof quien);
        printf("+ Conectado:    %s\n", quien);
        fflush(stdout);

        long bytes = atender(fd_cli);

        printf("- Desconectado: %s (%ld bytes devueltos)\n", quien, bytes);
        fflush(stdout);

        if (close(fd_cli) == -1)
            perror("close");
    }

    close(fd_escucha);
    return EXIT_SUCCESS;
}
