#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <inttypes.h>

// Inclusão das interfaces desenvolvidas pelos Integrantes 1 e 2 e pelo Professor
#include "memoria.h"
#include "swap.h"
#include "hardware_specs.h"

// Vetor local do Integrante 3: Rastreia o tamanho real solicitado pelo processo para cada VPN.
// Utilizado exclusivamente no cálculo de fragmentação interna do Visualizador ASCII (Teste 4).
static uint32_t bytes_alocados_vpn[TOTAL_PAGES] = {0};

// Estrutura para consolidação das métricas finais do simulador
typedef struct {
    uint32_t total_acessos;
    uint32_t page_faults_global;
    float max_swap_temp;
    uint64_t wasted_io_cycles;
} SimulationMetrics;

static SimulationMetrics metrics = {0, 0, 25.0f, 0};

// -----------------------------------------------------------------------------
// TÓPICO 2: Módulo de Auditoria Anti-Fraude (Atende ao Teste 3)
// -----------------------------------------------------------------------------
// Calcula um hash acumulado (checksum FNV-1a) baseado nos bits 'present' e
// nos 'frame_number' da Tabela de Páginas. Se o estado real do hardware for
// alterado ou burlado por estruturas paralelas, este hash mudará/divergirá.
void auditar_tabela_estado(const char *motivo) {
    uint64_t hash = UINT64_C(1469598103934665603);

    for (uint32_t vpn = 0; vpn < TOTAL_PAGES; vpn++) {
        // Incorpora o bit de presença na RAM ao hash
        hash ^= (uint64_t)page_table[vpn].present;
        hash *= UINT64_C(1099511628211);

        // Incorpora o número do quadro físico (frame) ao hash
        hash ^= (uint64_t)page_table[vpn].frame_number;
        hash *= UINT64_C(1099511628211);
    }

    printf("[AUDITORIA] %s | Checksum Acumulado: %" PRIu64 "\n", motivo, hash);
}

// -----------------------------------------------------------------------------
// TÓPICO 3: Visualizador de Fragmentação ASCII (Atende ao Teste 4)
// -----------------------------------------------------------------------------
// Varre a Tabela de Páginas a cada 10 acessos processados e imprime um mapa
// em caracteres ASCII ([U] para Usado, [F] para Fragmentado) diferenciando
// páginas pares (4KB) de páginas ímpares (8KB).
void exibir_mapa_fragmentacao_ascii(uint32_t linha_atual) {
    printf("\n========================================================\n");
    printf("   MAPA DE OCUPAÇÃO E FRAGMENTAÇÃO DA RAM (Linha %u)\n", linha_atual);
    printf("========================================================\n");

    uint32_t desperdicio_total_bytes = 0;
    uint32_t paginas_na_ram = 0;

    for (uint32_t vpn = 0; vpn < TOTAL_PAGES; vpn++) {
        if (page_table[vpn].present) {
            paginas_na_ram++;

            // Mapeamento Assimétrico: Páginas Pares = 4KB (4096B), Ímpares = 8KB (8192B)
            uint32_t tamanho_pagina_kb = (vpn % 2 == 0) ? 4 : 8;
            uint32_t tamanho_pagina_bytes = tamanho_pagina_kb * 1024;

            // Limita o uso ao tamanho máximo da página correspondente
            uint32_t bytes_usados = bytes_alocados_vpn[vpn];
            if (bytes_usados > tamanho_pagina_bytes) {
                bytes_usados = tamanho_pagina_bytes;
            }

            // Converte bytes em blocos visuais de 1 KB para exibição ASCII
            uint32_t kb_usados = (bytes_usados + 1023) / 1024;
            if (kb_usados == 0 && bytes_usados > 0) kb_usados = 1;

            // Calcula o desperdício em KB (Fragmentação Interna)
            uint32_t kb_fragmentados = (tamanho_pagina_kb > kb_usados) ? (tamanho_pagina_kb - kb_usados) : 0;
            desperdicio_total_bytes += (kb_fragmentados * 1024);

            printf("VPN %03u (%uKB) | Quadro %02u: [", vpn, tamanho_pagina_kb, page_table[vpn].frame_number);

            // Imprime 'U' para memória efetivamente utilizada pelo processo
            for (uint32_t k = 0; k < kb_usados; k++) printf("U");
            
            // Imprime 'F' para memória desperdiçada por fragmentação interna
            for (uint32_t k = 0; k < kb_fragmentados; k++) printf("F");

            printf("] (Usado: %u B / Desperdiçado: %u B)\n", bytes_usados, kb_fragmentados * 1024);
        }
    }

    if (paginas_na_ram == 0) {
        printf("Nenhuma pagina residente na RAM neste momento.\n");
    }

    // Calcula a porcentagem de desperdício em relação ao tamanho total da RAM física (256 KB)
    float porcentagem_desperdicio = ((float)desperdicio_total_bytes / 262144.0f) * 100.0f;
    printf("--------------------------------------------------------\n");
    printf("Desperdício Total por Fragmentação Interna: %u Bytes (%.2f%% da RAM)\n", 
           desperdicio_total_bytes, porcentagem_desperdicio);
    printf("========================================================\n\n");
}

// -----------------------------------------------------------------------------
// TÓPICO 1: Parser do Arquivo de Trace e Execução Central (O Motor do Sistema)
// -----------------------------------------------------------------------------
int main(void) {
    // Inicializa as estruturas da RAM do Integrante 1
    init_memoria();

    // Inicializa o arquivo de Swap do Integrante 2
    if (!swap_init()) {
        fprintf(stderr, "Erro critico: Falha ao inicializar o arquivo de Swap.\n");
        return EXIT_FAILURE;
    }

    // Abertura do arquivo de trace para leitura linha por linha
    FILE *arquivo_trace = fopen("memory_trace.txt", "r");
    if (arquivo_trace == NULL) {
        fprintf(stderr, "Erro: Nao foi possivel abrir 'memory_trace.txt'. Crie o arquivo para testar.\n");
        swap_close();
        return EXIT_FAILURE;
    }

    char linha[256];
    uint32_t timestamp, pid, endereco_virtual, tamanho_alocacao;
    char operacao;
    hardware_state_t hardware_simulado;

    printf("=== INICIANDO SIMULADOR ANTIKIT-8 ===\n\n");

    // Loop do Parser: Lê uma linha do arquivo texto por vez sem sobrecarregar a memória
    while (fgets(linha, sizeof(linha), arquivo_trace) != NULL) {
        // Ignora linhas em branco ou comentários iniciados com '#'
        if (linha[0] == '\n' || linha[0] == '#') continue;

        // Extrai os 5 campos: Tempo, PID, Operação (R/W), Endereço Virtual e Tamanho de Alocação
        if (sscanf(linha, "%u %u %c %u %u", &timestamp, &pid, &operacao, &endereco_virtual, &tamanho_alocacao) == 5) {
            metrics.total_acessos++;

            // Tradução do endereço virtual em VPN e Offset (Função da MMU do Integrante 1)
            uint32_t vpn = 0;
            uint32_t offset = 0;
            calcular_vpn_offset(endereco_virtual, &vpn, &offset);

            // Validação do Teste 1: Intercepta acessos além de 1MB (VPNs inválidas retornadas pela MMU)
            if (vpn >= TOTAL_PAGES) {
                printf("[ERRO] Timestamp %u: Processo %u tentou aceder ao endereco %u (Segmentation Fault).\n",
                       timestamp, pid, endereco_virtual);
                continue; // Pula a instrução inválida e prossegue a simulação
            }

            // Armazena a alocação do processo no vetor local para o Mapa ASCII
            bytes_alocados_vpn[vpn] = tamanho_alocacao;
            bool is_write = (operacao == 'W' || operacao == 'w');

            // --- LÓGICA DE PAGE HIT VS PAGE FAULT ---
            if (page_table[vpn].present) {
                // PAGE HIT: A página já reside na RAM física.
                // Atualiza bits de estado cruciais para o algoritmo WS-Clock do Integrante 2
                page_table[vpn].referenced = true;
                if (is_write) page_table[vpn].modified = true;
                page_table[vpn].last_access_t = timestamp;
            } else {
                // PAGE FAULT: A página não está na RAM.
                metrics.page_faults_global++;
                
                // Aciona o tratador do Integrante 2 (Executa substituição no Swap se a RAM estiver cheia)
                if (!tratar_page_fault(vpn, &hardware_simulado, timestamp)) {
                    printf("[FALHA] Nao foi possivel tratar o Page Fault para a VPN %u\n", vpn);
                    break;
                }
                
                if (is_write) page_table[vpn].modified = true;
            }

            // --- SIMULAÇÃO DA ANOMALIA DE HARDWARE (Fading do Bit R a cada 5 acessos) ---
            if (metrics.total_acessos % 5 == 0) {
                printf("\n[HARDWARE] Anomalia eletrica: Fading do Bit R no acesso #%u\n", metrics.total_acessos);
                
                // O SO nao mexe na RAM. Apenas aciona a anomalia fisica da MMU.
                simular_fading_mmu();
                
                // Executa a verificação Anti-Fraude imediatamente após o evento físico (Teste 3)
                auditar_tabela_estado("Recalculo apos Fading Eletrico");
            }

            // --- DISPARO DO VISUALIZADOR ASCII (A cada 10 linhas processadas) ---
            if (metrics.total_acessos % 10 == 0) {
                exibir_mapa_fragmentacao_ascii(metrics.total_acessos);
            }
        }
    }

    // Limpeza de recursos
    fclose(arquivo_trace);
    swap_close();

    // --- CONSOLIDAÇÃO DOS RESULTADOS FINAIS ---
    printf("\n========================================================\n");
    printf("   RESULTADOS FINAIS DA SIMULAÇÃO\n");
    printf("========================================================\n");
    printf("Total de Acessos a Memoria: %u\n", metrics.total_acessos);
    printf("Total de Page Faults Gerados: %u\n", metrics.page_faults_global);
    printf("========================================================\n");
    printf("Temperatura Maxima do Swap: %.2f Graus Celsius\n", metrics.max_swap_temp);
    printf("Ciclos de Clock (E/S) Desperdicados: %llu\n", (unsigned long long)metrics.wasted_io_cycles);
    printf("========================================================\n");

    return EXIT_SUCCESS;
}
