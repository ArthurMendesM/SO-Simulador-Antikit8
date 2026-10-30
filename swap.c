
#include "memoria.h"
#include "hardware_specs.h"

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <inttypes.h>

#define SWAP_FILE "swap.bin"
#define FRAME_SIZE 4096u

/*
 * Tempo máximo de permanência no Working Set, em milissegundos.
 */
#ifndef WS_TAU_MS
#define WS_TAU_MS 1000u
#endif

/*
 * Cada VPN possui uma área de 8 KB no arquivo de Swap.
 * Páginas de 4 KB usam apenas metade dessa área.
 */
static FILE *swap_file = NULL;
static bool swap_valid[TOTAL_PAGES] = { false };

/* Ponteiro circular do WS-Clock. */
static uint32_t clock_hand = 0;

/*
 * RAM simulada usada por este módulo.
 *
 * IMPORTANTE: os demais módulos que leem/escrevem conteúdo
 * de memória precisam usar este mesmo armazenamento, ou ele
 * deverá ser movido para memoria.c e compartilhado por uma
 * interface pública.
 */
static unsigned char ram_data[TOTAL_FRAMES][FRAME_SIZE];

/* ---------------------------------------------------------
 * Funções auxiliares
 * --------------------------------------------------------- */

static uint32_t page_size(uint32_t vpn)
{
    return (vpn % 2u == 0u) ? 4096u : 8192u;
}

static uint32_t frames_needed(uint32_t vpn)
{
    return page_size(vpn) / FRAME_SIZE;
}

/* ---------------------------------------------------------
 * Inicialização e encerramento
 * --------------------------------------------------------- */

bool swap_init(void)
{
    /*
     * "w+b" cria um arquivo novo ou limpa o arquivo anterior.
     * A simulação começa sem páginas previamente salvas.
     */
    swap_file = fopen(SWAP_FILE, "w+b");

    if (swap_file == NULL) {
        perror("Erro ao abrir arquivo de Swap");
        return false;
    }

    memset(swap_valid, 0, sizeof(swap_valid));
    memset(ram_data, 0, sizeof(ram_data));
    clock_hand = 0;

    return true;
}

void swap_close(void)
{
    if (swap_file != NULL) {
        fclose(swap_file);
        swap_file = NULL;
    }
}

/* ---------------------------------------------------------
 * Operações de armazenamento no Swap
 * --------------------------------------------------------- */

/*
 * Grava uma página no disco.
 * Deve ser chamada quando uma página modificada for removida.
 */
static bool swap_write_page(uint32_t vpn)
{
    if (swap_file == NULL || vpn >= TOTAL_PAGES)
        return false;

    if (!page_table[vpn].present)
        return false;

    uint32_t first_frame = page_table[vpn].frame_number;
    uint32_t count = frames_needed(vpn);

    if (first_frame + count > TOTAL_FRAMES)
        return false;

    if (fseek(swap_file, (long)vpn * 8192L, SEEK_SET) != 0)
        return false;

    for (uint32_t i = 0; i < count; i++) {
        if (fwrite(ram_data[first_frame + i],
                   1, FRAME_SIZE, swap_file) != FRAME_SIZE) {
            return false;
        }
    }

    if (fflush(swap_file) != 0)
        return false;

    swap_valid[vpn] = true;
    return true;
}

/*
 * Recupera uma página do Swap.
 *
 * Se ela nunca foi gravada, seu conteúdo inicial é zero.
 * Se já existe uma cópia válida, lê essa cópia do arquivo.
 */
static bool swap_read_page(uint32_t vpn, uint32_t first_frame)
{
    if (swap_file == NULL || vpn >= TOTAL_PAGES)
        return false;

    uint32_t count = frames_needed(vpn);

    if (first_frame + count > TOTAL_FRAMES)
        return false;

    if (!swap_valid[vpn]) {
        for (uint32_t i = 0; i < count; i++) {
            memset(ram_data[first_frame + i], 0, FRAME_SIZE);
        }
        return true;
    }

    if (fseek(swap_file, (long)vpn * 8192L, SEEK_SET) != 0)
        return false;

    for (uint32_t i = 0; i < count; i++) {
        if (fread(ram_data[first_frame + i],
                  1, FRAME_SIZE, swap_file) != FRAME_SIZE) {
            return false;
        }
    }

    return true;
}

/* ---------------------------------------------------------
 * WS-Clock: seleção da página vítima
 * --------------------------------------------------------- */

/*
 * Política adotada:
 *
 * 1. Percorre as páginas em ordem circular.
 * 2. Páginas ausentes são ignoradas.
 * 3. Páginas com R=1 recebem uma segunda chance:
 *    R é zerado e a página não é escolhida nessa visita.
 * 4. Páginas com R=0 e idade <= tau permanecem no Working Set.
 * 5. Entre as páginas com R=0 e idade > tau, prioriza uma
 *    página limpa (modified=0).
 * 6. Se não houver página antiga limpa, escolhe uma antiga
 *    modificada. Ela será gravada antes de ser removida.
 * 7. Se nenhuma página estiver fora do Working Set, usa a
 *    página mais antiga como fallback para garantir progresso.
 *
 * Este é um WS-Clock adaptado para o simulador, com busca
 * limitada a duas passagens completas.
 */
static int escolher_vitima(uint32_t timestamp_ms)
{
    int limpa_antiga = -1;
    int suja_antiga = -1;
    int mais_antiga = -1;

    uint32_t idade_suja = 0;
    uint32_t maior_idade = 0;

    for (uint32_t i = 0; i < TOTAL_PAGES * 2u; i++) {
        uint32_t vpn = clock_hand;
        clock_hand = (clock_hand + 1u) % TOTAL_PAGES;

        if (!page_table[vpn].present)
            continue;

        uint32_t ultimo = page_table[vpn].last_access_t;
        uint32_t idade = (timestamp_ms >= ultimo)
                       ? timestamp_ms - ultimo
                       : 0u;

        if (mais_antiga == -1 || idade > maior_idade) {
            mais_antiga = (int)vpn;
            maior_idade = idade;
        }

        /*
         * O bit R pode ser apagado pelo hardware a cada cinco
         * acessos. Se R=1 quando examinamos a página, damos
         * uma segunda chance e continuamos a busca.
         */
        if (page_table[vpn].referenced) {
            page_table[vpn].referenced = false;
            continue;
        }

        /* A página ainda pertence ao Working Set. */
        if (idade <= WS_TAU_MS)
            continue;

        /* Página fora do Working Set e não modificada. */
        if (!page_table[vpn].modified) {
            limpa_antiga = (int)vpn;
            break;
        }

        /* Guarda a página suja mais antiga como alternativa. */
        if (suja_antiga == -1 || idade > idade_suja) {
            suja_antiga = (int)vpn;
            idade_suja = idade;
        }
    }

    if (limpa_antiga != -1)
        return limpa_antiga;

    if (suja_antiga != -1)
        return suja_antiga;

    /*
     * Fallback: todas as páginas podem estar dentro do
     * Working Set ou ter recebido segunda chance.
     */
    return mais_antiga;
}

/* ---------------------------------------------------------
 * Page Out: remoção de uma página
 * --------------------------------------------------------- */

static bool swap_out_page(uint32_t vpn,
                          hardware_state_t *hardware,
                          uint32_t timestamp_ms)
{
    if (vpn >= TOTAL_PAGES || !page_table[vpn].present)
        return false;

    uint32_t first_frame = page_table[vpn].frame_number;
    uint32_t count = frames_needed(vpn);

    if (first_frame + count > TOTAL_FRAMES)
        return false;

    /*
     * O enunciado exige contabilizar a operação de Swap
     * quando a página está modificada.
     *
     * Uma página limpa não precisa ser gravada novamente:
     * se tiver cópia válida, ela já está no arquivo; caso
     * contrário, seu conteúdo inicial é zero.
     */
    if (page_table[vpn].modified) {
        if (!swap_write_page(vpn)) {
            fprintf(stderr,
                    "Erro ao gravar VPN %u no Swap.\n", vpn);
            return false;
        }

        if (hardware != NULL) {
            hardware_trigger_page_out(hardware, timestamp_ms);
        }
    }

    /*
     * Só libera os quadros depois de preservar os dados
     * necessários no Swap.
     */
    for (uint32_t i = 0; i < count; i++) {
        ram_frames[first_frame + i] = false;
        memset(ram_data[first_frame + i], 0, FRAME_SIZE);
    }

    page_table[vpn].present = false;
    page_table[vpn].referenced = false;
    page_table[vpn].modified = false;

    /*
     * Mantém last_access_t como histórico do último acesso.
     * A página só será considerada residente quando present=1.
     */

    printf("[WS-CLOCK] Page Out: VPN %u, quadro inicial %u\n",
           vpn, first_frame);

    return true;
}

/* ---------------------------------------------------------
 * Tratamento de Page Fault
 * --------------------------------------------------------- */

bool tratar_page_fault(uint32_t vpn,
                       hardware_state_t *hardware,
                       uint32_t timestamp_ms)
{
    if (vpn >= TOTAL_PAGES)
        return false;

    if (swap_file == NULL) {
        fprintf(stderr, "Swap não foi inicializado.\n");
        return false;
    }

    if (page_table[vpn].present)
        return true;

    int frame = alocar_quadros((int)(page_size(vpn) / 1024u));

    /*
     * Se não houver espaço adequado, remove vítimas até
     * conseguir um quadro (4 KB) ou dois consecutivos (8 KB).
     */
    while (frame == -1) {
        int victim = escolher_vitima(timestamp_ms);

        if (victim < 0) {
            fprintf(stderr,
                    "Não foi possível encontrar página vítima.\n");
            return false;
        }

        if (!swap_out_page((uint32_t)victim,
                           hardware, timestamp_ms)) {
            return false;
        }

        frame = alocar_quadros((int)(page_size(vpn) / 1024u));
    }

    /*
     * Carrega o conteúdo para o armazenamento de RAM usado
     * por este módulo.
     */
    if (!swap_read_page(vpn, (uint32_t)frame)) {
        uint32_t count = frames_needed(vpn);

        for (uint32_t i = 0; i < count; i++) {
            ram_frames[(uint32_t)frame + i] = false;
            memset(ram_data[(uint32_t)frame + i], 0, FRAME_SIZE);
        }

        fprintf(stderr,
                "Erro ao carregar VPN %u do Swap.\n", vpn);
        return false;
    }

    page_table[vpn].frame_number = (uint32_t)frame;
    page_table[vpn].present = true;
    page_table[vpn].referenced = true;
    page_table[vpn].modified = false;
    page_table[vpn].last_access_t = timestamp_ms;

    printf("[WS-CLOCK] Page In: VPN %u, quadro inicial %d\n",
           vpn, frame);

    return true;
}
