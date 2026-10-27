#include "memoria.h"

page_table_entry_t page_table[TOTAL_PAGES];

void init_memoria() {
    // Inicializa todas as páginas como vazias e não presentes na RAM
    for (int i = 0; i < TOTAL_PAGES; i++) {
        page_table[i].frame_number = 0;
        page_table[i].present = false;
        page_table[i].referenced = false;
        page_table[i].modified = false;
        page_table[i].last_access_t = 0;
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