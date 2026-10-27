#ifndef MEMORIA_H
#define MEMORIA_H

#include <stdint.h>
#include "hardware_specs.h"
#include <stdbool.h>

/*
 * O espaço virtual de 1 MB tem 1.048.576 bytes.
 * Como cada "bloco" de duas páginas (uma par e uma ímpar) soma 12.288 bytes,
 * dividindo 1.048.576 por 12.288, temos aproximadamente 85,33 blocos.
 * Isso significa que precisaremos de 86 páginas pares e 86 páginas ímpares,
 * totalizando 172 entradas na Tabela de Páginas para cobrir 1 MB.
 */
#define TOTAL_PAGES 172

// A RAM de 256 KB dividida em blocos de 4 KB resulta em 64 quadros
#define TOTAL_FRAMES 64

// Variável global que representa a Tabela de Páginas do nosso Sistema Operacional
// Usamos a palavra 'extern' para avisar aos outros arquivos que essa variável
// existe e será criada no memoria.c
extern page_table_entry_t page_table[TOTAL_PAGES];

// Vetor booleano para mapear o uso da RAM (false = livre, true = ocupado)
extern bool ram_frames[TOTAL_FRAMES];

// As assinaturas do Integrante 1, Arthur Mendes Mazzei, encontram-se abaixo

// Inicializa a tabela de páginas zerando seus valores
void init_memoria();

// Recebe um endereço virtual bruto e devolve (por ponteiros) o VPN e o Offset
void calcular_vpn_offset(uint32_t endereco_virtual, uint32_t *vpn, uint32_t *offset);

// Função que o sistema chamará quando houver um Page Fault
// Retorna o índice do quadro alocado, ou -1 se a RAM estiver cheia
int alocar_quadros(int tamanho_kb);

#endif // MEMORIA_H