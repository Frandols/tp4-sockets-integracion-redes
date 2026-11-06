/*
 * ej01_sockets.c
 *
 * Ejercicio 1 - Creación de sockets y manejo de errores.
 *
 * 1. Crea (sin cerrarlos) tres sockets: TCP/IPv4, UDP/IPv4 y TCP/IPv6,
 *    con protocolo 0 (el sistema elige el protocolo por defecto del tipo).
 * 2. Muestra el descriptor de cada uno.
 * 3. Cierra los tres.
 * 4. Intenta combinaciones inválidas tipo/protocolo y muestra el error.
 *
 * Además (pregunta 2) demuestra la reutilización de descriptores:
 * cierra el primer socket y crea uno nuevo para ver qué número recibe.
 *
 * Compilar: gcc -Wall -Wextra -o ej01_sockets ej01_sockets.c
 * Uso:      ./ej01_sockets
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

/*
 * Crea un socket e informa el resultado.
 * Si falla, informa el error con perror() y devuelve -1, pero NO termina
 * el programa: la consigna pide continuar con los demás sockets.
 */
static int crear_socket(int dominio, int tipo, int protocolo, const char *desc)
{
    int fd = socket(dominio, tipo, protocolo);
    if (fd == -1) {
        /* Guardamos errno antes de cualquier otra llamada que pueda pisarlo. */
        int err = errno;
        fprintf(stderr, "  %-40s -> ERROR (errno=%d): ", desc, err);
        errno = err;
        perror("socket");
        return -1;
    }
    printf("  %-40s -> descriptor %d\n", desc, fd);
    return fd;
}

/* Cierra un descriptor válido e informa si close() falla. */
static void cerrar_socket(int fd, const char *desc)
{
    if (fd == -1)
        return;                 /* No se creó: no hay nada que cerrar. */
    if (close(fd) == -1)
        perror("close");
    else
        printf("  %-40s -> descriptor %d cerrado\n", desc, fd);
}

int main(int argc, char *argv[])
{
    /* stdout sin buffer: así los mensajes normales (stdout) y los de
     * error (stderr) salen en el orden real aunque se redirija la salida. */
    setvbuf(stdout, NULL, _IONBF, 0);

    /* El programa no recibe argumentos: si se pasan, mostramos el uso. */
    if (argc != 1) {
        fprintf(stderr, "Uso: %s\n", argv[0]);
        return EXIT_FAILURE;
    }

    /* --- Puntos 1 y 2: crear los tres sockets sin cerrarlos ----------- */
    printf("== Creación de sockets (protocolo 0) ==\n");
    int tcp4 = crear_socket(AF_INET,  SOCK_STREAM, 0, "TCP sobre IPv4 (AF_INET, SOCK_STREAM)");
    int udp4 = crear_socket(AF_INET,  SOCK_DGRAM,  0, "UDP sobre IPv4 (AF_INET, SOCK_DGRAM)");
    int tcp6 = crear_socket(AF_INET6, SOCK_STREAM, 0, "TCP sobre IPv6 (AF_INET6, SOCK_STREAM)");

    /* --- Pregunta 2: reutilización del descriptor más bajo libre ------ */
    /* POSIX garantiza que socket()/open() devuelven el descriptor libre
     * de número más bajo. Si cerramos el primero, el nuevo debería
     * recibir ese mismo número. */
    printf("\n== Reutilización de descriptores ==\n");
    cerrar_socket(tcp4, "Cierro el primer socket (TCP/IPv4)");
    tcp4 = crear_socket(AF_INET, SOCK_STREAM, 0, "Nuevo TCP sobre IPv4");

    /* --- Punto 3: cerrar los tres sockets ----------------------------- */
    printf("\n== Cierre de sockets ==\n");
    cerrar_socket(tcp4, "TCP/IPv4");
    cerrar_socket(udp4, "UDP/IPv4");
    cerrar_socket(tcp6, "TCP/IPv6");

    /* --- Punto 4 y pregunta 4: combinaciones tipo/protocolo inválidas - */
    /* El tipo define la semántica (flujo confiable vs. datagramas) y el
     * protocolo debe ser uno que implemente esa semántica. TCP solo
     * ofrece flujo y UDP solo datagramas, por eso estas mezclas fallan. */
    printf("\n== Combinaciones inválidas ==\n");
    int inv1 = crear_socket(AF_INET, SOCK_STREAM, IPPROTO_UDP,
                            "AF_INET, SOCK_STREAM, IPPROTO_UDP");
    int inv2 = crear_socket(AF_INET, SOCK_DGRAM, IPPROTO_TCP,
                            "AF_INET, SOCK_DGRAM, IPPROTO_TCP");

    /* Por si algún sistema las aceptara, liberamos igual el recurso. */
    cerrar_socket(inv1, "inv1");
    cerrar_socket(inv2, "inv2");

    return EXIT_SUCCESS;
}
