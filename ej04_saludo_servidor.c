/*
 * ej04_saludo_servidor.c
 *
 * Ejercicio 4 - Secuencia básica de un servidor TCP:
 *               socket() -> bind() -> listen() -> accept()
 *
 * Uso: ./ej04_saludo_servidor <puerto>
 *
 * 1. Crea un socket TCP asociado a la dirección comodín (INADDR_ANY) y
 *    al puerto indicado.
 * 2. Por cada cliente muestra su IP y su puerto.
 * 3. Le envía "Hola, te conectaste desde <IP>:<puerto>".
 * 4. Cierra la conexión con ese cliente y espera al siguiente (bucle
 *    infinito; se termina con Ctrl+C).
 *
 * Prueba: nc localhost <puerto>   (varias veces seguidas)
 *
 * Compilar: gcc -Wall -Wextra -o ej04_saludo_servidor ej04_saludo_servidor.c
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

#define BACKLOG 10   /* Conexiones pendientes que el kernel encola */

/*
 * Convierte el argumento a número de puerto validando que sea un entero
 * entre 1 y 65535. Devuelve -1 si no es válido.
 */
static int parsear_puerto(const char *s)
{
    char *fin;
    errno = 0;
    long v = strtol(s, &fin, 10);
    if (errno != 0 || *s == '\0' || *fin != '\0' || v < 1 || v > 65535)
        return -1;
    return (int)v;
}

/*
 * Envía todo el buffer: send() puede enviar menos bytes de los pedidos,
 * así que se repite hasta completar. Devuelve 0 si tuvo éxito, -1 si no.
 */
static int enviar_todo(int fd, const char *buf, size_t len)
{
    size_t enviado = 0;
    while (enviado < len) {
        ssize_t n = send(fd, buf + enviado, len - enviado, 0);
        if (n == -1) {
            if (errno == EINTR)
                continue;           /* Interrumpido por una señal: reintentar */
            return -1;
        }
        enviado += (size_t)n;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <puerto>\n", argv[0]);
        return EXIT_FAILURE;
    }
    int puerto = parsear_puerto(argv[1]);
    if (puerto == -1) {
        fprintf(stderr, "Puerto inválido: '%s' (debe ser 1-65535)\n", argv[1]);
        fprintf(stderr, "Uso: %s <puerto>\n", argv[0]);
        return EXIT_FAILURE;
    }

    /* --- socket(): descriptor de "escucha" ---------------------------- */
    int fd_escucha = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_escucha == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }

    /* SO_REUSEADDR permite reiniciar el servidor enseguida aunque queden
     * conexiones viejas en TIME_WAIT en este puerto (se estudia en el
     * ejercicio 5). Sin esto, bind() fallaría con EADDRINUSE. */
    int si = 1;
    if (setsockopt(fd_escucha, SOL_SOCKET, SO_REUSEADDR, &si, sizeof si) == -1) {
        perror("setsockopt(SO_REUSEADDR)");
        close(fd_escucha);
        return EXIT_FAILURE;
    }

    /* --- bind(): dirección comodín + puerto --------------------------- */
    struct sockaddr_in dir;
    memset(&dir, 0, sizeof dir);              /* Requisito: inicializar en cero */
    dir.sin_family      = AF_INET;
    dir.sin_addr.s_addr = htonl(INADDR_ANY);  /* Todas las interfaces locales */
    dir.sin_port        = htons((uint16_t)puerto); /* Orden de red */

    if (bind(fd_escucha, (struct sockaddr *)&dir, sizeof dir) == -1) {
        perror("bind");
        close(fd_escucha);
        return EXIT_FAILURE;
    }

    /* --- listen(): el socket pasa a ser pasivo ------------------------ */
    if (listen(fd_escucha, BACKLOG) == -1) {
        perror("listen");
        close(fd_escucha);
        return EXIT_FAILURE;
    }

    printf("Servidor escuchando en 0.0.0.0:%d (Ctrl+C para terminar)\n", puerto);
    fflush(stdout);

    /* --- Bucle de atención: un cliente por vez ------------------------ */
    for (;;) {
        struct sockaddr_in cli;
        socklen_t cli_len = sizeof cli;
        memset(&cli, 0, sizeof cli);

        /* accept() bloquea hasta que llega un cliente y devuelve un
         * descriptor NUEVO, conectado sólo con ese cliente. El de escucha
         * sigue abierto para aceptar a los siguientes. */
        int fd_cli = accept(fd_escucha, (struct sockaddr *)&cli, &cli_len);
        if (fd_cli == -1) {
            if (errno == EINTR)
                continue;
            perror("accept");
            continue;   /* Un fallo con un cliente no debe tumbar el servidor */
        }

        /* IP y puerto del cliente: la IP se pasa a texto con inet_ntop()
         * y el puerto se convierte del orden de red con ntohs(). */
        char ip[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &cli.sin_addr, ip, sizeof ip) == NULL) {
            perror("inet_ntop");
            strcpy(ip, "?");
        }
        unsigned cli_puerto = ntohs(cli.sin_port);

        printf("Cliente conectado desde %s:%u (descriptor %d)\n",
               ip, cli_puerto, fd_cli);
        fflush(stdout);

        char msg[128];
        int len = snprintf(msg, sizeof msg,
                           "Hola, te conectaste desde %s:%u\n", ip, cli_puerto);
        if (len < 0 || (size_t)len >= sizeof msg) {
            fprintf(stderr, "snprintf: mensaje truncado\n");
        } else if (enviar_todo(fd_cli, msg, (size_t)len) == -1) {
            perror("send");
        }

        /* Se cierra el descriptor del CLIENTE, no el de escucha. */
        if (close(fd_cli) == -1)
            perror("close");
    }

    /* No se alcanza (bucle infinito), pero se deja por prolijidad. */
    close(fd_escucha);
    return EXIT_SUCCESS;
}
