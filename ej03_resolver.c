/*
 * ej03_resolver.c
 *
 * Ejercicio 3 - Resolución de nombres con getaddrinfo() y getnameinfo().
 *
 * Uso: ./ej03_resolver <host> <servicio>
 *
 * 1. Llama a getaddrinfo() pidiendo cualquier familia (AF_UNSPEC) para
 *    sockets SOCK_STREAM.
 * 2. Para cada nodo de la lista muestra familia, IP en texto y puerto.
 * 3. Para cada dirección busca el nombre inverso con getnameinfo();
 *    si no existe, lo indica.
 * 4. Libera la lista con freeaddrinfo().
 * 5. Informa los errores de resolución con gai_strerror().
 *
 * Compilar: gcc -Wall -Wextra -o ej03_resolver ej03_resolver.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>          /* getaddrinfo, getnameinfo, gai_strerror */
#include <netinet/in.h>
#include <arpa/inet.h>      /* inet_ntop, ntohs */

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <host> <servicio>\n", argv[0]);
        fprintf(stderr, "Ej.: %s google.com https\n", argv[0]);
        return EXIT_FAILURE;
    }
    const char *host = argv[1];
    const char *servicio = argv[2];

    /* --- Punto 1: preparar las "pistas" (hints) para getaddrinfo() ---- */
    /* Requisito común: inicializar en cero. Los campos que no se usan
     * (ai_addr, ai_next, etc.) DEBEN valer 0/NULL en hints. */
    struct addrinfo hints;
    memset(&hints, 0, sizeof hints);
    hints.ai_family   = AF_UNSPEC;    /* IPv4 o IPv6, lo que haya */
    hints.ai_socktype = SOCK_STREAM;  /* Sólo TCP: evita duplicados por UDP/RAW */

    struct addrinfo *lista;
    int rc = getaddrinfo(host, servicio, &hints, &lista);
    if (rc != 0) {
        /* getaddrinfo() no usa errno: devuelve su propio código EAI_*,
         * que se traduce a texto con gai_strerror(). La única excepción
         * es EAI_SYSTEM, que indica un error del sistema guardado en errno. */
        if (rc == EAI_SYSTEM)
            perror("getaddrinfo");
        else
            fprintf(stderr, "getaddrinfo(%s, %s): %s\n",
                    host, servicio, gai_strerror(rc));
        return EXIT_FAILURE;
    }

    printf("Resultados para %s %s:\n", host, servicio);

    /* --- Puntos 2 y 3: recorrer la lista enlazada --------------------- */
    int n = 0;
    for (struct addrinfo *p = lista; p != NULL; p = p->ai_next) {
        char ip[INET6_ADDRSTRLEN];    /* alcanza para IPv4 e IPv6 */
        const void *addr;
        const char *familia;
        unsigned puerto;

        /* ai_addr apunta a un sockaddr genérico; según la familia hay que
         * tratarlo como sockaddr_in (IPv4) o sockaddr_in6 (IPv6). El
         * puerto viene en orden de red: se convierte con ntohs(). */
        if (p->ai_family == AF_INET) {
            const struct sockaddr_in *s4 = (const struct sockaddr_in *)p->ai_addr;
            addr = &s4->sin_addr;
            puerto = ntohs(s4->sin_port);
            familia = "IPv4";
        } else if (p->ai_family == AF_INET6) {
            const struct sockaddr_in6 *s6 = (const struct sockaddr_in6 *)p->ai_addr;
            addr = &s6->sin6_addr;
            puerto = ntohs(s6->sin6_port);
            familia = "IPv6";
        } else {
            printf("  [%d] familia desconocida (%d), se omite\n", ++n, p->ai_family);
            continue;
        }

        if (inet_ntop(p->ai_family, addr, ip, sizeof ip) == NULL) {
            perror("inet_ntop");
            continue;
        }

        printf("  [%d] %-4s  %-40s puerto %u\n", ++n, familia, ip, puerto);

        /* Punto 3: resolución inversa. NI_NAMEREQD hace que getnameinfo()
         * falle (EAI_NONAME) si no hay nombre, en lugar de devolver la IP
         * en texto: así podemos distinguir "no tiene nombre inverso". */
        char nombre[NI_MAXHOST];
        int rn = getnameinfo(p->ai_addr, p->ai_addrlen,
                             nombre, sizeof nombre, NULL, 0, NI_NAMEREQD);
        if (rn == 0)
            printf("        nombre inverso: %s\n", nombre);
        else if (rn == EAI_NONAME)
            printf("        nombre inverso: (no existe)\n");
        else if (rn == EAI_SYSTEM)
            perror("        getnameinfo");
        else
            printf("        nombre inverso: error (%s)\n", gai_strerror(rn));
    }

    printf("Total: %d dirección(es)\n", n);

    /* --- Punto 4: liberar la lista ------------------------------------ */
    freeaddrinfo(lista);
    return EXIT_SUCCESS;
}
