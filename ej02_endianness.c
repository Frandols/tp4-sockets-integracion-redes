/*
 * ej02_endianness.c
 *
 * Ejercicio 2 - Orden de bytes del host vs. orden de la red.
 *
 * 1. Detecta si la máquina es little-endian o big-endian mirando el
 *    primer byte en memoria de un uint16_t con valor 0x0102.
 * 2. Muestra byte a byte el número 8080 antes y después de htons().
 * 3. Muestra byte a byte 0xC0A8010A (192.168.1.10) antes y después de htonl().
 * 4. Asigna 8080 a sin_port SIN convertir y muestra ntohs(); luego lo
 *    repite correctamente con htons().
 *
 * El orden de red (network byte order) es SIEMPRE big-endian: el byte
 * más significativo va primero. Las funciones hton*() convierten del
 * orden del host al de la red; en una máquina big-endian no hacen nada.
 *
 * Compilar: gcc -Wall -Wextra -o ej02_endianness ej02_endianness.c
 * Uso:      ./ej02_endianness
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>   /* htons, htonl, ntohs, ntohl */
#include <netinet/in.h>  /* struct sockaddr_in */

/*
 * Muestra los bytes de una variable tal como están en memoria, desde la
 * dirección más baja a la más alta. Se recorre con un puntero a
 * unsigned char porque es el único tipo que permite ver la memoria de
 * cualquier objeto byte por byte sin violar las reglas de aliasing de C.
 */
static void mostrar_bytes(const char *etiqueta, const void *dato, size_t tam)
{
    const unsigned char *p = dato;
    printf("  %-28s", etiqueta);
    for (size_t i = 0; i < tam; i++)
        printf(" [%zu]=%02X", i, p[i]);
    printf("\n");
}

int main(int argc, char *argv[])
{
    if (argc != 1) {
        fprintf(stderr, "Uso: %s\n", argv[0]);
        return EXIT_FAILURE;
    }

    /* --- Punto 1: detectar el endianness del host --------------------- */
    /* 0x0102: el byte más significativo es 0x01 y el menos, 0x02.
     * Si en la dirección más baja está 0x02, se guarda primero el menos
     * significativo -> little-endian. Si está 0x01 -> big-endian. */
    uint16_t prueba = 0x0102;
    unsigned char primero = *(unsigned char *)&prueba;

    printf("== 1. Endianness del host ==\n");
    mostrar_bytes("uint16_t 0x0102:", &prueba, sizeof prueba);
    if (primero == 0x02)
        printf("  Primer byte = 0x02 -> la máquina es LITTLE-ENDIAN\n");
    else if (primero == 0x01)
        printf("  Primer byte = 0x01 -> la máquina es BIG-ENDIAN\n");
    else
        printf("  Primer byte inesperado (0x%02X)\n", primero);

    /* --- Punto 2: 8080 (16 bits) antes y después de htons() ----------- */
    uint16_t puerto_host = 8080;               /* 8080 = 0x1F90 */
    uint16_t puerto_red  = htons(puerto_host);

    printf("\n== 2. Puerto 8080 (0x%04X) ==\n", puerto_host);
    mostrar_bytes("orden del host:", &puerto_host, sizeof puerto_host);
    mostrar_bytes("tras htons() (red):", &puerto_red, sizeof puerto_red);

    /* --- Punto 3: 0xC0A8010A (32 bits) antes y después de htonl() ----- */
    uint32_t ip_host = 0xC0A8010A;             /* 192.168.1.10 */
    uint32_t ip_red  = htonl(ip_host);

    printf("\n== 3. Dirección 192.168.1.10 (0x%08X) ==\n", ip_host);
    mostrar_bytes("orden del host:", &ip_host, sizeof ip_host);
    mostrar_bytes("tras htonl() (red):", &ip_red, sizeof ip_red);

    /* Verificación cruzada: inet_ntop() interpreta los bytes en orden de
     * red. Solo el valor convertido debe dar el texto correcto. */
    char txt[INET_ADDRSTRLEN];
    struct in_addr a;
    a.s_addr = ip_host;
    if (inet_ntop(AF_INET, &a, txt, sizeof txt) == NULL)
        perror("inet_ntop");
    else
        printf("  inet_ntop sin convertir:     %s\n", txt);
    a.s_addr = ip_red;
    if (inet_ntop(AF_INET, &a, txt, sizeof txt) == NULL)
        perror("inet_ntop");
    else
        printf("  inet_ntop con htonl():       %s\n", txt);

    /* --- Punto 4: sin_port sin convertir vs. convertido --------------- */
    struct sockaddr_in dir;

    printf("\n== 4. Campo sin_port de struct sockaddr_in ==\n");

    /* Requisito común: inicializar la estructura en cero. */
    memset(&dir, 0, sizeof dir);
    dir.sin_family = AF_INET;
    dir.sin_port = 8080;                       /* ERROR intencional: sin htons() */
    mostrar_bytes("sin_port = 8080:", &dir.sin_port, sizeof dir.sin_port);
    printf("  ntohs(sin_port) = %u   <- puerto que vería la red\n",
           (unsigned)ntohs(dir.sin_port));

    memset(&dir, 0, sizeof dir);
    dir.sin_family = AF_INET;
    dir.sin_port = htons(8080);                /* Forma correcta */
    mostrar_bytes("sin_port = htons(8080):", &dir.sin_port, sizeof dir.sin_port);
    printf("  ntohs(sin_port) = %u\n", (unsigned)ntohs(dir.sin_port));

    return EXIT_SUCCESS;
}
