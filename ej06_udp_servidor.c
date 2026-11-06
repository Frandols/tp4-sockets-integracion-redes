/*
 * ej06_udp_servidor.c
 *
 * Ejercicio 6 - Servidor de eco UDP.
 *
 * Uso: ./ej06_udp_servidor <puerto>
 *
 * Recibe datagramas con recvfrom(), muestra la dirección de origen y el
 * tamaño de cada uno, y los devuelve al remitente con sendto().
 *
 * UDP no tiene conexiones: no hay listen() ni accept(). Un único socket
 * recibe datagramas de cualquier cliente, y recvfrom() informa en cada
 * llamada quién lo envió para poder responderle.
 *
 * Igual que en el ejercicio 5 se usa un socket IPv6 dual stack (acepta
 * también IPv4) y, si el sistema no tiene IPv6, un socket IPv4.
 *
 * Compilar: gcc -Wall -Wextra -o ej06_udp_servidor ej06_udp_servidor.c
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

/* Tamaño máximo de un datagrama UDP (65535 - 8 de cabecera UDP - 20 de IP).
 * Con un buffer así nunca se trunca un datagrama recibido. */
#define TAM_BUF 65507

static int parsear_puerto(const char *s)
{
    char *fin;
    errno = 0;
    long v = strtol(s, &fin, 10);
    if (errno != 0 || *s == '\0' || *fin != '\0' || v < 1 || v > 65535)
        return -1;
    return (int)v;
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <puerto>\n", argv[0]);
        return EXIT_FAILURE;
    }
    int puerto = parsear_puerto(argv[1]);
    if (puerto == -1) {
        fprintf(stderr, "Puerto inválido: '%s'\nUso: %s <puerto>\n", argv[1], argv[0]);
        return EXIT_FAILURE;
    }

    int familia = AF_INET6;
    int fd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (fd == -1 && errno == EAFNOSUPPORT) {
        fprintf(stderr, "Aviso: sin soporte IPv6, se usa sólo IPv4\n");
        familia = AF_INET;
        fd = socket(AF_INET, SOCK_DGRAM, 0);
    }
    if (fd == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }

    if (familia == AF_INET6) {
        int no = 0;
        if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &no, sizeof no) == -1) {
            perror("setsockopt(IPV6_V6ONLY)");
            close(fd);
            return EXIT_FAILURE;
        }
    }

    int rc_bind;
    if (familia == AF_INET6) {
        struct sockaddr_in6 d6;
        memset(&d6, 0, sizeof d6);
        d6.sin6_family = AF_INET6;
        d6.sin6_addr   = in6addr_any;
        d6.sin6_port   = htons((uint16_t)puerto);
        rc_bind = bind(fd, (struct sockaddr *)&d6, sizeof d6);
    } else {
        struct sockaddr_in d4;
        memset(&d4, 0, sizeof d4);
        d4.sin_family      = AF_INET;
        d4.sin_addr.s_addr = htonl(INADDR_ANY);
        d4.sin_port        = htons((uint16_t)puerto);
        rc_bind = bind(fd, (struct sockaddr *)&d4, sizeof d4);
    }
    if (rc_bind == -1) {
        perror("bind");
        close(fd);
        return EXIT_FAILURE;
    }

    printf("Servidor UDP de eco en %s:%d. Ctrl+C para terminar.\n",
           familia == AF_INET6 ? "[::]" : "0.0.0.0", puerto);
    fflush(stdout);

    static char buf[TAM_BUF];   /* static: no ocupar 64 KB de pila */

    for (;;) {
        struct sockaddr_storage origen;      /* Alcanza para IPv4 e IPv6 */
        socklen_t olen = sizeof origen;
        memset(&origen, 0, sizeof origen);

        /* Cada recvfrom() devuelve UN datagrama completo, con sus límites
         * intactos (a diferencia de TCP, que es un flujo de bytes). */
        ssize_t n = recvfrom(fd, buf, sizeof buf, 0,
                             (struct sockaddr *)&origen, &olen);
        if (n == -1) {
            if (errno == EINTR)
                continue;
            perror("recvfrom");
            continue;
        }

        char ip[NI_MAXHOST], port[NI_MAXSERV];
        int rc = getnameinfo((struct sockaddr *)&origen, olen, ip, sizeof ip,
                             port, sizeof port, NI_NUMERICHOST | NI_NUMERICSERV);
        if (rc != 0) {
            fprintf(stderr, "getnameinfo: %s\n", gai_strerror(rc));
            strcpy(ip, "?");
            strcpy(port, "?");
        }
        printf("Datagrama de [%s]:%s, %zd bytes\n", ip, port, n);
        fflush(stdout);

        /* En UDP sendto() envía el datagrama entero o falla: no hay
         * envíos parciales, así que no hace falta un bucle. */
        ssize_t e = sendto(fd, buf, (size_t)n, 0, (struct sockaddr *)&origen, olen);
        if (e == -1)
            perror("sendto");
        else if (e != n)
            fprintf(stderr, "sendto: se enviaron %zd de %zd bytes\n", e, n);
    }

    close(fd);
    return EXIT_SUCCESS;
}
