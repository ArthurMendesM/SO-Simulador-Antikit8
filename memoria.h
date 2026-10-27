#ifndef MEMORIA_H
#define MEMORIA_H

#include <stdint.h>
#include "hardware_specs.h"

/*
 * O espaço virtual de 1 MB tem 1.048.576 bytes.
 * Como cada "bloco" de duas páginas (uma par e uma ímpar) soma 12.288 bytes,
 * dividindo 1.048.576 por 12.288, temos aproximadamente 85,33 blocos.
 * Isso significa que precisaremos de 86 páginas pares e 86 páginas ímpares,
 * totalizando 172 entradas na Tabela de Páginas para cobrir 1 MB.
 */
#define TOTAL_PAGES 172

// Variável global que representa a Tabela de Páginas do nosso Sistema Operacional
// Usamos a palavra 'extern' para avisar aos outros arquivos que essa variável
// existe e será criada no memoria.c
extern page_table_entry_t page_table[TOTAL_PAGES];

// As assinaturas do Integrante 1, Arthur Mendes Mazzei, encontram-se abaixo

// Inicializa a tabela de páginas zerando seus valores
void init_memoria();

// Recebe um endereço virtual bruto e devolve (por ponteiros) o VPN e o Offset
void calcular_vpn_offset(uint32_t endereco_virtual, uint32_t *vpn, uint32_t *offset);

#endif // MEMORIA_H