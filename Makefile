# Nome do compilador
CC = gcc

# Flags de compilacao (Avisos detalhados e optimizacao de velocidade)
CFLAGS = -Wall -Wextra -O2

# Nome do executavel gerado
TARGET = simulador

# Lista de modulos do projeto
SRCS = main.c memoria.c swap.c hardware_specs.c

# Regra padrao (o que acontece ao digitar apenas 'make')
all: $(TARGET)

# Regra de compilacao
$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

# Regra de limpeza (o que acontece ao digitar 'make clean')
clean:
	rm -f $(TARGET) swap.bin

