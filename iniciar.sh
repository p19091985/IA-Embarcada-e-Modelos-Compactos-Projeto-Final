#!/usr/bin/env bash

if [ -z "${BASH_VERSION:-}" ]; then
    exec bash "$0" "$@"
fi

set -e

PASTA="$(cd "$(dirname "$0")" && pwd)"
LIMPAR=0
ABRIR_VSCODE=0
SO_ABRIR_VSCODE=0
RODAR_TESTES=0
COMPILAR_TESTES_C=0
FLASH_TESTES_C=0
USAR_MENU=0

if [ -t 1 ] && command -v tput >/dev/null 2>&1; then
    CORES="$(tput colors 2>/dev/null || echo 0)"
else
    CORES=0
fi

if [ "$CORES" -ge 8 ] 2>/dev/null; then
    RESET="$(tput sgr0)"
    BOLD="$(tput bold)"
    DIM="$(tput dim)"
    RED="$(tput setaf 1)"
    GREEN="$(tput setaf 2)"
    YELLOW="$(tput setaf 3)"
    BLUE="$(tput setaf 4)"
    MAGENTA="$(tput setaf 5)"
    CYAN="$(tput setaf 6)"
else
    RESET=""
    BOLD=""
    DIM=""
    RED=""
    GREEN=""
    YELLOW=""
    BLUE=""
    MAGENTA=""
    CYAN=""
fi

linha() {
    printf "%b\n" "${DIM}------------------------------------------------------------${RESET}"
}

uso() {
    printf "%b\n" "${BOLD}Uso:${RESET}"
    echo "  ./iniciar.sh                 abre o menu interativo"
    echo "  ./iniciar.sh build           compila o firmware principal"
    echo "  ./iniciar.sh testar          roda pytest do diagram.json e compila testes Unity"
    echo "  ./iniciar.sh validar         compila firmware + roda testes + compila Unity"
    echo "  ./iniciar.sh vscode          compila e abre o projeto/diagram.json no VS Code"
    echo "  ./iniciar.sh simular         igual a vscode, com checklist para Wokwi"
    echo "  ./iniciar.sh unity           compila apenas o app Unity/ESP-IDF em test/build_tests"
    echo "  ./iniciar.sh flash-testes    grava e abre monitor serial do app Unity"
    echo "  ./iniciar.sh limpar validar  limpa build principal e executa validacao"
    echo "  CLEAN=1 ./iniciar.sh build   limpa com idf.py fullclean antes do build"
}

abrir_vscode() {
    if command -v code >/dev/null 2>&1; then
        printf "%b\n" "${CYAN}==>${RESET} Abrindo projeto e diagram.json no VS Code..."
        code --reuse-window "$PASTA" "$PASTA/diagram.json" >/dev/null 2>&1 || true
    else
        printf "%b\n" "${YELLOW}Aviso:${RESET} Nao achei o comando 'code' para abrir o VS Code."
        printf "%b\n" "Abra manualmente esta pasta no VS Code: $PASTA"
    fi
}

mostrar_checklist_wokwi() {
    echo
    linha
    printf "%b\n" "${BOLD}${CYAN}Teste manual no Wokwi${RESET}"
    linha
    printf "%b\n" "No VS Code, execute ${BOLD}Wokwi: Start Simulator${RESET}."
    echo
    echo "Checklist:"
    echo "  [ ] OLED mostra o menu inicial"
    echo "  [ ] Tecla A inicia uma partida"
    echo "  [ ] Teclas 1 a 9 fazem jogadas"
    echo "  [ ] LCD1602 mostra o algoritmo da IA"
    echo "  [ ] Tecla * liga o LED dourado"
    echo "  [ ] Tecla # desliga o LED dourado"
    echo "  [ ] Buzzer toca nas teclas, inicializacao e vitoria"
    echo "  [ ] Partida termina com vitoria do jogador"
    echo "  [ ] Partida termina com vitoria do computador"
    echo "  [ ] Partida termina com empate"
    linha
}

garantir_alvo_esp32s3() {
    local diretorio="$1"
    local build_dir="$2"
    local sdkconfig="$diretorio/sdkconfig"

    if [ -f "$sdkconfig" ] && grep -q '^CONFIG_IDF_TARGET="esp32s3"$' "$sdkconfig"; then
        printf "%b\n" "${CYAN}==>${RESET} Alvo ESP32-S3 ja configurado em: $sdkconfig"
        return
    fi

    printf "%b\n" "${CYAN}==>${RESET} Configurando alvo ESP32-S3 em: $diretorio"
    (
        cd "$diretorio"
        idf.py -B "$build_dir" set-target esp32s3
    )
}

menu() {
    while true; do
        if [ -t 1 ]; then
            clear
        fi

        linha
        printf "%b\n" "${BOLD}${CYAN}IA Embarcada - Jogo da Velha ESP32-S3${RESET}"
        linha
        printf "%b\n" "  ${GREEN}1${RESET}) Compilar firmware principal"
        printf "%b\n" "  ${YELLOW}2${RESET}) Limpar tudo e compilar firmware"
        printf "%b\n" "  ${BLUE}3${RESET}) Rodar testes automaticos"
        printf "%b\n" "  ${MAGENTA}4${RESET}) Validar tudo que da pelo terminal"
        printf "%b\n" "  ${CYAN}5${RESET}) Compilar, abrir VS Code e testar no Wokwi"
        printf "%b\n" "  ${YELLOW}6${RESET}) Gravar app Unity e abrir monitor serial"
        printf "%b\n" "  ${BLUE}7${RESET}) Abrir projeto e diagram.json no VS Code"
        printf "%b\n" "  ${RED}0${RESET}) Sair"
        linha
        printf "%b" "${BOLD}Escolha uma opcao:${RESET} "

        IFS= read -r opcao || exit 0

        case "$opcao" in
            1)
                return
                ;;
            2)
                LIMPAR=1
                return
                ;;
            3)
                RODAR_TESTES=1
                COMPILAR_TESTES_C=1
                return
                ;;
            4)
                RODAR_TESTES=1
                COMPILAR_TESTES_C=1
                return
                ;;
            5)
                RODAR_TESTES=1
                COMPILAR_TESTES_C=1
                ABRIR_VSCODE=1
                return
                ;;
            6)
                FLASH_TESTES_C=1
                return
                ;;
            7)
                SO_ABRIR_VSCODE=1
                return
                ;;
            0|s|S|sair|Sair|SAIR|q|Q)
                printf "%b\n" "${GREEN}Saindo. Nada foi alterado.${RESET}"
                exit 0
                ;;
            *)
                printf "%b\n" "${RED}Opcao invalida.${RESET}"
                sleep 1
                ;;
        esac
    done
}

for arg in "$@"; do
    case "$arg" in
        build|compilar|--build)
            ;;
        limpar|/limpar|clean|/clean|--clean|-c)
            LIMPAR=1
            ;;
        testar|teste|testes|test|tests|--test|--tests)
            RODAR_TESTES=1
            COMPILAR_TESTES_C=1
            ;;
        validar|validacao|validate|--validate)
            RODAR_TESTES=1
            COMPILAR_TESTES_C=1
            ;;
        unity|testes-c|--unity)
            COMPILAR_TESTES_C=1
            ;;
        flash-testes|flash-tests|monitor-testes|--flash-testes)
            FLASH_TESTES_C=1
            ;;
        vscode|abrir|open|--vscode|--open)
            ABRIR_VSCODE=1
            ;;
        simular|wokwi|--simular|--wokwi)
            RODAR_TESTES=1
            COMPILAR_TESTES_C=1
            ABRIR_VSCODE=1
            ;;
        somente-vscode|so-vscode|--only-vscode)
            SO_ABRIR_VSCODE=1
            ;;
        menu|--menu|-m)
            USAR_MENU=1
            ;;
        ajuda|help|--help|-h)
            uso
            exit 0
            ;;
        *)
            echo "Opcao desconhecida: $arg"
            uso
            exit 1
            ;;
    esac
done

if { [ "$#" -eq 0 ] && [ "${CLEAN:-0}" != "1" ]; } || [ "$USAR_MENU" = "1" ]; then
    menu
fi

if [ -z "${IDF_EXPORT:-}" ]; then
    if [ -n "${IDF_PATH:-}" ]; then
        IDF_EXPORT="$IDF_PATH/export.sh"
    else
        IDF_EXPORT="$HOME/.espressif/v6.0.1/esp-idf/export.sh"
    fi
fi

cd "$PASTA"

if [ "$SO_ABRIR_VSCODE" = "1" ]; then
    abrir_vscode
    mostrar_checklist_wokwi
    exit 0
fi

printf "%b\n" "${CYAN}==>${RESET} Carregando ESP-IDF de: $IDF_EXPORT"
if [ ! -f "$IDF_EXPORT" ]; then
    printf "%b\n" "${RED}Erro:${RESET} Nao achei o ESP-IDF em: $IDF_EXPORT"
    echo "Ajuste a variavel IDF_EXPORT se instalou em outro lugar."
    exit 1
fi

. "$IDF_EXPORT"

if [ "${CLEAN:-0}" = "1" ] || [ "$LIMPAR" = "1" ]; then
    if [ -d build ]; then
        printf "%b\n" "${CYAN}==>${RESET} Limpando build principal antigo..."
        idf.py fullclean
    else
        printf "%b\n" "${YELLOW}==>${RESET} Nada para limpar: build/ nao existe."
    fi
fi

if [ "$FLASH_TESTES_C" = "1" ]; then
    printf "%b\n" "${CYAN}==>${RESET} Preparando app Unity/ESP-IDF..."
    garantir_alvo_esp32s3 "$PASTA/test" "build_tests"
    (
        cd "$PASTA/test"
        idf.py -B build_tests build
        idf.py -B build_tests flash monitor
    )
    exit 0
fi

garantir_alvo_esp32s3 "$PASTA" "build"

printf "%b\n" "${CYAN}==>${RESET} Compilando firmware principal..."
idf.py build

if [ "$RODAR_TESTES" = "1" ]; then
    printf "%b\n" "${CYAN}==>${RESET} Rodando teste Python do diagram.json..."
    python -m pytest test/test_diagram_json.py
fi

if [ "$COMPILAR_TESTES_C" = "1" ]; then
    printf "%b\n" "${CYAN}==>${RESET} Compilando app Unity/ESP-IDF dos testes C..."
    garantir_alvo_esp32s3 "$PASTA/test" "build_tests"
    (
        cd "$PASTA/test"
        idf.py -B build_tests build
    )
fi

if [ "$ABRIR_VSCODE" = "1" ]; then
    abrir_vscode
fi

echo
printf "%b\n" "${GREEN}Pronto.${RESET}"
echo "Arquivos principais gerados:"
echo "  - build/jogo_da_velha_esp32s3.elf"
echo "  - build/flasher_args.json"

if [ "$COMPILAR_TESTES_C" = "1" ]; then
    echo "  - test/build_tests/jogo_da_velha_unity_tests.bin"
fi

if [ "$ABRIR_VSCODE" = "1" ]; then
    mostrar_checklist_wokwi
else
    echo
    echo "Para testar no Wokwi depois:"
    echo "  ./iniciar.sh simular"
fi
