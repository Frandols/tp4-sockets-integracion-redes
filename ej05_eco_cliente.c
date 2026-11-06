/*
 * ej05_eco_cliente.c
 *
 * Ejercicio 5 - Cliente de eco TCP.
 *
 * Uso: ./ej05_eco_cliente <host> <servicio>
 *
 * 1. Resuelve el destino con getaddrinfo() y prueba cada dirección de la
 *    lista hasta que una conexión tenga éxito, mostrando cada intento.
 * 2. Lee líneas del teclado, las envía y muestra la respuesta completa.
 * 3. Termina con fin de entrada (Ctrl+D).
 *
 * Compilar: gcc -Wall -Wextra -o ej05_eco_cliente ej05_eco_cliente.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define TAM_LINEA 4096

/* Igual que en el servidor: completar envíos parciales; MSG_NOSIGNAL
 * evita morir por SIGPIPE si el servidor ya cerró. */
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

/*
 * Recibe exactamente len bytes. TCP es un flujo de bytes sin límites de
 * mensaje: un recv() puede traer sólo una parte del eco, así que se
 * repite hasta tener todo lo enviado.
 * Devuelve len si tuvo éxito, 0 si el servidor cerró, -1 en error.
 */
static ssize_t recibir_exacto(int fd, char *buf, size_t len)
{
    size_t recibido = 0;
    while (recibido < len) {
        ssize_t n = recv(fd, buf + recibido, len - recibido, 0);
        if (n == 0)
            return 0;
        if (n == -1) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        recibido += (size_t)n;
    }
    return (ssize_t)recibido;
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <host> <servicio>\n", argv[0]);
        return EXIT_FAILURE;
    }

    /* stdout sin buffer: los mensajes salen en orden aun redirigidos. */
    setvbuf(stdout, NULL, _IONBF, 0);

    struct addrinfo hints, *lista;
    memset(&hints, 0, sizeof hints);
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int rc = getaddrinfo(argv[1], argv[2], &hints, &lista);
    if (rc != 0) {
        fprintf(stderr, "getaddrinfo(%s, %s): %s\n", argv[1], argv[2],
                rc == EAI_SYSTEM ? strerror(errno) : gai_strerror(rc));
        return EXIT_FAILURE;
    }

    /* --- Punto 1: probar cada dirección hasta que una funcione -------- */
    /* Un nombre puede tener varias direcciones (IPv4/IPv6, varios
     * servidores). Si una falla, se sigue con la siguiente. */
    int fd = -1;
    for (struct addrinfo *p = lista; p != NULL; p = p->ai_next) {
        char ip[NI_MAXHOST], port[NI_MAXSERV];
        if (getnameinfo(p->ai_addr, p->ai_addrlen, ip, sizeof ip,
                        port, sizeof port, NI_NUMERICHOST | NI_NUMERICSERV) != 0)
            strcpy(ip, "?"), strcpy(port, "?");

        printf("Intentando [%s]:%s ... ", ip, port);
        fflush(stdout);

        fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd == -1) {
            printf("socket: %s\n", strerror(errno));
            continue;
        }
        if (connect(fd, p->ai_addr, p->ai_addrlen) == 0) {
            printf("conectado\n");
            break;                       /* Éxito: salimos del bucle */
        }
        printf("connect: %s\n", strerror(errno));
        close(fd);                       /* Liberar el socket fallido */
        fd = -1;
    }
    freeaddrinfo(lista);                 /* Ya no se necesita la lista */

    if (fd == -1) {
        fprintf(stderr, "No se pudo conectar a ninguna dirección\n");
        return EXIT_FAILURE;
    }

    /* --- Punto 2: bucle de lectura/envío/eco -------------------------- */
    printf("Escribí líneas (Ctrl+D para terminar):\n");
    char linea[TAM_LINEA], eco[TAM_LINEA];
    int estado = EXIT_SUCCESS;

    /* fgets() devuelve NULL en fin de entrada (Ctrl+D) -> punto 3. */
    while (fgets(linea, sizeof linea, stdin) != NULL) {
        size_t len = strlen(linea);

        if (enviar_todo(fd, linea, len) == -1) {
            perror("send");
            estado = EXIT_FAILURE;
            break;
        }

        ssize_t n = recibir_exacto(fd, eco, len);
        if (n == 0) {
            fprintf(stderr, "El servidor cerró la conexión\n");
            estado = EXIT_FAILURE;
            break;
        }
        if (n == -1) {
            perror("recv");
            estado = EXIT_FAILURE;
            break;
        }
        printf("eco: %.*s", (int)n, eco);
        if (eco[n - 1] != '\n')
            printf("\n");
        fflush(stdout);
    }
    if (ferror(stdin))
        perror("fgets");

    /* Cierre: close() envía FIN. Como el cliente cierra primero, será
     * este extremo el que quede en TIME_WAIT. */
    printf("Cerrando conexión\n");
    if (close(fd) == -1)
        perror("close");
    return estado;
}
