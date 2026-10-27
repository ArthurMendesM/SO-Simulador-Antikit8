#include "memoria.h"

page_table_entry_t page_table[TOTAL_PAGES];
bool ram_frames[TOTAL_FRAMES];

void init_memoria() {
    // Inicializa todas as páginas como vazias e não presentes na RAM
    for (int i = 0; i < TOTAL_PAGES; i++) {
        page_table[i].frame_number = 0;
        page_table[i].present = false;
        page_table[i].referenced = false;
        page_table[i].modified = false;
        page_table[i].last_access_t = 0;
    }
    // Inicializa a RAM marcando todos os 64 quadros como livres
    for (int i = 0; i < TOTAL_FRAMES; i++) {
        ram_frames[i] = false;
    }
}

void calcular_vpn_offset(uint32_t endereco_virtual, uint32_t *vpn, uint32_t *offset) {
    /* 
     * ESTRATÉGIA DE BLOCOS DE 12 KB
     * 1 Página Par (4096 bytes) + 1 Página Ímpar (8192 bytes) = 1 Bloco de 12288 bytes.
     */
    
    // Descobre em qual bloco de 12 KB o endereço está contido
    uint32_t bloco = endereco_virtual / 12288;
    
    // Descobre a posição exata (o resto) do endereço dentro desse bloco
    uint32_t resto = endereco_virtual % 12288;
    
    // Como cada bloco tem 2 páginas, a base do VPN será o dobro do número do bloco
    uint32_t base_vpn = bloco * 2;
    
    // Decide se o resto caiu na metade Par ou na metade Ímpar do bloco
    if (resto < 4096) {
        // O dado está nos primeiros 4 KB do bloco, ou seja, na Página Par.
        *vpn = base_vpn;
        *offset = resto;
    } else {
        // O dado passou de 4096, logo está na Página Ímpar.
        *vpn = base_vpn + 1;
        
        // O offset recomeça do zero na página nova, logo subtraímos os 4 KB da página par
        *offset = resto - 4096; 
    }
}

int alocar_quadros(int tamanho_kb) {
    // Se a página for Par (4 KB), precisamos de apenas 1 quadro livre
    if (tamanho_kb == 4) {
        for (int i = 0; i < TOTAL_FRAMES; i++) {
            if (ram_frames[i] == false) {
                ram_frames[i] = true; // Ocupa o quadro
                return i;             // Devolve o número do quadro ao sistema
            }
        }
    }
    // Se a página for Ímpar (8 KB), precisamos de 2 quadros livres CONSECUTIVOS
    else if (tamanho_kb == 8) {
        for (int i = 0; i < TOTAL_FRAMES - 1; i++) {
            if (ram_frames[i] == false && ram_frames[i+1] == false) {
                ram_frames[i] = true;     // Ocupa o primeiro quadro
                ram_frames[i+1] = true;   // Ocupa o segundo quadro
                return i;                 // Devolve o início do bloco alocado
            }
        }
    }

    // Se o código chegar aqui, significa que a RAM está cheia ou fragmentada.
    // Retorna -1 para avisar o Integrante 2 que ele precisa de acionar o algoritmo de substituição.
    return -1;
}