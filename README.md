# TP 4 - Sockets

Optativa II - Integración de Redes (Internetworking), 2026.
Francisco De Los Santos.

Programas en C (biblioteca estándar y cabeceras POSIX) para Linux. Todos compilan sin errores ni advertencias con `gcc -Wall -Wextra`.

## Compilación

```bash
make          # compila todos los programas
make clean    # borra los ejecutables
```

O cada uno por separado:

```bash
gcc -Wall -Wextra -o ej01_sockets ej01_sockets.c
```

## Programas

| Ejercicio | Archivo | Uso |
|---|---|---|
| 1. Creación de sockets y errores | `ej01_sockets.c` | `./ej01_sockets` |
| 2. Orden de bytes | `ej02_endianness.c` | `./ej02_endianness` |
| 3. Resolución de nombres | `ej03_resolver.c` | `./ej03_resolver <host> <servicio>` |
| 4. Servidor TCP de saludo | `ej04_saludo_servidor.c` | `./ej04_saludo_servidor <puerto>` |
| 5. Servidor de eco TCP | `ej05_eco_servidor.c` | `./ej05_eco_servidor <puerto> [noreuse]` |
| 5. Cliente de eco TCP | `ej05_eco_cliente.c` | `./ej05_eco_cliente <host> <servicio>` |
| 6. Servidor de eco UDP | `ej06_udp_servidor.c` | `./ej06_udp_servidor <puerto>` |
| 6. Cliente UDP con timeout | `ej06_udp_cliente.c` | `./ej06_udp_cliente <host> <puerto> [conectado]` |

## Pruebas rápidas

```bash
# Ejercicio 4
./ej04_saludo_servidor 5000
nc -N localhost 5000 </dev/null

# Ejercicio 5
./ej05_eco_servidor 6000
./ej05_eco_cliente localhost 6000
ss -tan | grep 6000

# Ejercicio 6
./ej06_udp_servidor 7000
./ej06_udp_cliente localhost 7000
./ej06_udp_cliente localhost 7000 conectado
```

Paquetes necesarios en Ubuntu: `sudo apt install build-essential netcat-openbsd tmux`.
