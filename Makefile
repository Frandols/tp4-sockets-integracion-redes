CC     = gcc
CFLAGS = -Wall -Wextra

PROGS = ej01_sockets ej02_endianness ej03_resolver ej04_saludo_servidor \
        ej05_eco_servidor ej05_eco_cliente ej06_udp_servidor ej06_udp_cliente

all: $(PROGS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(PROGS)

.PHONY: all clean
