/*
 * ej06_udp_cliente.c
 *
 * Ejercicio 6 - Cliente UDP con timeout y reintentos.
 *
 * Uso: ./ej06_udp_cliente <host> <puerto> [conectado]
 *
 * 1. Envía tres datagramas de 1, 18 y 1000 bytes y verifica que cada
 *    respuesta tenga exactamente el tamaño (y el contenido) enviado.
 * 2. Espera cada respuesta como máximo 2 s (SO_RCVTIMEO).
 * 3. Si no llega respuesta, reintenta hasta 3 veces en total antes de
 *    abandonar ese mensaje, informando cada intento.
 * 4. Sin "conectado": sendto() + recvfrom().
 *    Con "conectado": connect() + send() + recv().
 *
 * UDP no garantiza la entrega: la confiabilidad (timeout + reintento)
 * la tiene que implementar la aplicación, que es lo que hace este programa.
 *
 * Compilar: gcc -Wall -Wextra -o ej06_udp_cliente ej06_udp_cliente.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>       /* struct timeval para SO_RCVTIMEO */
#include <netinet/in.h>
#include <netdb.h>

#define TIMEOUT_SEG  2
#define MAX_INTENTOS 3
#define TAM_BUF      2048   /* Mayor que el datagrama más grande que se envía */

/* Compara dos direcciones de socket (familia, IP y puerto). Se usa en el
 * modo no conectado para comprobar que la respuesta viene del servidor
 * y no de un tercero: recvfrom() acepta datagramas de cualquiera. */
static int misma_direccion(const struct sockaddr *a, const struct sockaddr *b)
{
    if (a->sa_family != b->sa_family)
        return 0;
    if (a->sa_family == AF_INET) {
        const struct sockaddr_in *x = (const void *)a, *y = (const void *)b;
        return x->sin_port == y->sin_port &&
               x->sin_addr.s_addr == y->sin_addr.s_addr;
    }
    if (a->sa_family == AF_INET6) {
        const struct sockaddr_in6 *x = (const void *)a, *y = (const void *)b;
        return x->sin6_port == y->sin6_port &&
               memcmp(&x->sin6_addr, &y->sin6_addr, sizeof x->sin6_addr) == 0;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    setvbuf(stdout, NULL, _IONBF, 0);

    if (argc < 3 || argc > 4 || (argc == 4 && strcmp(argv[3], "conectado") != 0)) {
        fprintf(stderr, "Uso: %s <host> <puerto> [conectado]\n", argv[0]);
        return EXIT_FAILURE;
    }
    int conectado = (argc == 4);

    struct addrinfo hints, *lista;
    memset(&hints, 0, sizeof hints);
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;     /* UDP */

    int rc = getaddrinfo(argv[1], argv[2], &hints, &lista);
    if (rc != 0) {
        fprintf(stderr, "getaddrinfo(%s, %s): %s\n", argv[1], argv[2],
                rc == EAI_SYSTEM ? strerror(errno) : gai_strerror(rc));
        return EXIT_FAILURE;
    }

    /* Se usa la primera dirección para la que se pueda crear un socket.
     * En UDP no hay forma de "probar" la conexión como en TCP: connect()
     * no envía nada y siempre funciona si la dirección es válida. */
    int fd = -1;
    struct sockaddr_storage destino;
    socklen_t dlen = 0;
    for (struct addrinfo *p = lista; p != NULL; p = p->ai_next) {
        fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd == -1) {
            perror("socket");
            continue;
        }
        memset(&destino, 0, sizeof destino);
        memcpy(&destino, p->ai_addr, p->ai_addrlen);
        dlen = p->ai_addrlen;
        break;
    }
    freeaddrinfo(lista);
    if (fd == -1) {
        fprintf(stderr, "No se pudo crear el socket\n");
        return EXIT_FAILURE;
    }

    char ip[NI_MAXHOST], port[NI_MAXSERV];
    if (getnameinfo((struct sockaddr *)&destino, dlen, ip, sizeof ip,
                    port, sizeof port, NI_NUMERICHOST | NI_NUMERICSERV) != 0)
        strcpy(ip, "?"), strcpy(port, "?");
    printf("Destino [%s]:%s, modo %s\n", ip, port,
           conectado ? "CONECTADO (connect + send/recv)"
                     : "NO CONECTADO (sendto/recvfrom)");

    /* --- Punto 2: timeout de recepción -------------------------------- */
    /* Con SO_RCVTIMEO, recv()/recvfrom() devuelven -1 con EAGAIN o
     * EWOULDBLOCK si pasan 2 s sin datos, en lugar de bloquear para siempre. */
    struct timeval tv;
    memset(&tv, 0, sizeof tv);
    tv.tv_sec = TIMEOUT_SEG;
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv) == -1) {
        perror("setsockopt(SO_RCVTIMEO)");
        close(fd);
        return EXIT_FAILURE;
    }

    /* --- Punto 4: modo conectado -------------------------------------- */
    /* connect() en UDP no envía ningún paquete: sólo fija en el kernel la
     * dirección remota por defecto. Desde ahí se puede usar send()/recv(),
     * se descartan datagramas de otros orígenes y el kernel puede
     * informar errores ICMP (p. ej. "puerto inalcanzable") al socket. */
    if (conectado) {
        if (connect(fd, (struct sockaddr *)&destino, dlen) == -1) {
            perror("connect");
            close(fd);
            return EXIT_FAILURE;
        }
        printf("connect() OK (no se envió ningún paquete)\n");
    }

    /* --- Punto 1: tres datagramas de tamaños distintos ---------------- */
    char mensajes[3][TAM_BUF];
    size_t tams[3] = { 1, 18, 1000 };
    memcpy(mensajes[0], "A", 1);
    memcpy(mensajes[1], "Hola servidor UDP!", 18);         /* 18 bytes */
    for (size_t i = 0; i < 1000; i++)
        mensajes[2][i] = (char)('a' + i % 26);              /* patrón conocido */

    int exitos = 0;
    char resp[TAM_BUF];

    for (int m = 0; m < 3; m++) {
        size_t len = tams[m];
        int ok = 0;

        printf("\nMensaje %d (%zu bytes)\n", m + 1, len);

        /* --- Punto 3: hasta MAX_INTENTOS intentos -------------------- */
        for (int intento = 1; intento <= MAX_INTENTOS && !ok; intento++) {
            printf("  intento %d/%d: ", intento, MAX_INTENTOS);

            ssize_t e = conectado
                ? send(fd, mensajes[m], len, 0)
                : sendto(fd, mensajes[m], len, 0, (struct sockaddr *)&destino, dlen);
            if (e == -1) {
                /* En modo conectado, un error ICMP pendiente de un envío
                 * anterior puede aparecer acá (ECONNREFUSED). */
                printf("envío falló: %s\n", strerror(errno));
                continue;
            }

            struct sockaddr_storage origen;
            socklen_t olen = sizeof origen;
            memset(&origen, 0, sizeof origen);

            ssize_t n = conectado
                ? recv(fd, resp, sizeof resp, 0)
                : recvfrom(fd, resp, sizeof resp, 0, (struct sockaddr *)&origen, &olen);

            if (n == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    printf("sin respuesta en %d s (timeout)\n", TIMEOUT_SEG);
                else
                    printf("recepción falló: %s\n", strerror(errno));
                continue;
            }

            if (!conectado && !misma_direccion((struct sockaddr *)&origen,
                                               (struct sockaddr *)&destino)) {
                printf("respuesta de un origen inesperado, se descarta\n");
                continue;
            }

            if ((size_t)n != len) {
                printf("respuesta de %zd bytes, se esperaban %zu\n", n, len);
                continue;
            }
            if (memcmp(resp, mensajes[m], len) != 0) {
                printf("tamaño correcto pero contenido distinto\n");
                continue;
            }
            printf("respuesta OK (%zd bytes, contenido idéntico)\n", n);
            ok = 1;
        }

        if (ok)
            exitos++;
        else
            printf("  -> mensaje %d abandonado tras %d intentos\n", m + 1, MAX_INTENTOS);
    }

    printf("\nResumen: %d de 3 mensajes con eco correcto\n", exitos);

    if (close(fd) == -1)
        perror("close");
    return exitos == 3 ? EXIT_SUCCESS : EXIT_FAILURE;
}
