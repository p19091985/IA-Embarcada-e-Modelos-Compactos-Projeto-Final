#!/usr/bin/env bash

if [ -z "${BASH_VERSION:-}" ]; then
    exec bash "$0" "$@"
fi

set -e
set -o pipefail

PASTA="$(cd "$(dirname "$0")" && pwd)"
LIMPAR=0
ABRIR_VSCODE=0
SO_ABRIR_VSCODE=0
RODAR_TESTES=0
COMPILAR_TESTES_C=0
FLASH_TESTES_C=0
MONITOR_AUDITORIA=0
FLASH_AUDITORIA=0
AUDITORIA_WOKWI=0
USAR_MENU=0
PYTHON_BIN="${PYTHON_BIN:-}"
AUDITORIA_LOG="$PASTA/logs/jogo_auditoria.log"
AUDITORIA_COMANDO="./iniciar.sh monitor"

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
    echo "  ./iniciar.sh simular-auditoria igual a simular, com log de auditoria preparado"
    echo "  ./iniciar.sh auditoria       compila, grava e monitora salvando logs/jogo_auditoria.log"
    echo "  ./iniciar.sh monitor         abre monitor serial salvando logs/jogo_auditoria.log"
    echo "  ./iniciar.sh unity           compila apenas o app Unity/ESP-IDF em test/build_tests"
    echo "  ./iniciar.sh flash-testes    grava e abre monitor serial do app Unity"
    echo "  ./iniciar.sh setup           verifica dependencias e prepara .venv"
    echo "  ./iniciar.sh limpar validar  limpa build principal e executa validacao"
    echo "  CLEAN=1 ./iniciar.sh build   limpa com idf.py fullclean antes do build"
}

# ============================================================================
# Deteccao e instalacao automatica de dependencias
# ============================================================================

encontrar_idf() {
    # Prioridade: variavel de ambiente > instalacao padrao > home do usuario
    if [ -n "${IDF_EXPORT:-}" ] && [ -f "$IDF_EXPORT" ]; then
        return 0
    fi

    if [ -n "${IDF_PATH:-}" ] && [ -f "$IDF_PATH/export.sh" ]; then
        IDF_EXPORT="$IDF_PATH/export.sh"
        return 0
    fi

    # Procura em locais comuns
    local caminhos_possiveis=(
        "$HOME/.espressif/v6.0.1/esp-idf/export.sh"
        "$HOME/esp/esp-idf/export.sh"
        "$HOME/.espressif/esp-idf/export.sh"
        "$HOME/esp-idf/export.sh"
        "/opt/esp-idf/export.sh"
        "/usr/local/esp-idf/export.sh"
    )

    # Tambem procura por qualquer versao em .espressif
    if [ -d "$HOME/.espressif" ]; then
        for dir in "$HOME/.espressif"/*/esp-idf; do
            if [ -f "$dir/export.sh" ]; then
                caminhos_possiveis+=("$dir/export.sh")
            fi
        done
    fi

    for caminho in "${caminhos_possiveis[@]}"; do
        if [ -f "$caminho" ]; then
            IDF_EXPORT="$caminho"
            return 0
        fi
    done

    return 1
}

VENV_DIR="$PASTA/.venv"

encontrar_python() {
    if [ -n "$PYTHON_BIN" ]; then
        return 0
    fi

    # Se a venv existe e tem python, usa ela
    if [ -f "$VENV_DIR/bin/python" ]; then
        PYTHON_BIN="$VENV_DIR/bin/python"
        return 0
    fi

    if command -v python3 >/dev/null 2>&1; then
        PYTHON_BIN=python3
    elif command -v python >/dev/null 2>&1; then
        PYTHON_BIN=python
    else
        return 1
    fi
    return 0
}

verificar_pytest() {
    "$PYTHON_BIN" -m pytest --version >/dev/null 2>&1
}

criar_ou_atualizar_venv() {
    local python_base=""

    if [ -f "$VENV_DIR/bin/python" ]; then
        PYTHON_BIN="$VENV_DIR/bin/python"
    else
        if command -v python3 >/dev/null 2>&1; then
            python_base=python3
        elif command -v python >/dev/null 2>&1; then
            python_base=python
        else
            printf "%b\n" "${RED}Erro:${RESET} Python3 nao encontrado."
            printf "%b\n" "Instale com: ${BOLD}sudo apt install python3 python3-pip python3-venv${RESET}"
            return 1
        fi

        printf "%b\n" "${CYAN}==>${RESET} Criando ambiente virtual em: $VENV_DIR"
        if ! "$python_base" -m venv "$VENV_DIR"; then
            printf "%b\n" "${RED}Erro:${RESET} nao foi possivel criar a .venv."
            printf "%b\n" "No Ubuntu/Debian, instale o suporte a venv:"
            printf "%b\n" "  ${BOLD}sudo apt install python3-venv${RESET}"
            return 1
        fi
        PYTHON_BIN="$VENV_DIR/bin/python"
    fi

    if verificar_pytest; then
        return 0
    fi

    printf "%b\n" "${CYAN}==>${RESET} Instalando dependencias Python em .venv..."
    if ! "$PYTHON_BIN" -m pip install -r "$PASTA/requirements.txt"; then
        printf "%b\n" "${RED}Erro:${RESET} nao foi possivel instalar requirements.txt na .venv."
        return 1
    fi

    verificar_pytest
}

garantir_venv() {
    if criar_ou_atualizar_venv; then
        PYTHON_BIN="$VENV_DIR/bin/python"
        return 0
    fi

    printf "%b\n" "${RED}Erro:${RESET} pytest indisponivel na .venv."
    return 1
}

verificar_dependencias() {
    local erro=0

    printf "%b\n" "${CYAN}==> Verificando dependencias...${RESET}"

    # Python
    if encontrar_python; then
        printf "%b\n" "  ${GREEN}✓${RESET} Python: $($PYTHON_BIN --version 2>&1)"
    else
        printf "%b\n" "  ${RED}✗${RESET} Python3 nao encontrado"
        printf "%b\n" "    Instale com: ${BOLD}sudo apt install python3 python3-pip python3-venv${RESET}"
        erro=1
    fi

    # pytest
    if [ "$erro" = "0" ]; then
        if garantir_venv; then
            if [ "$PYTHON_BIN" = "$VENV_DIR/bin/python" ]; then
                printf "%b\n" "  ${GREEN}✓${RESET} .venv pronta: $VENV_DIR"
            else
                printf "%b\n" "  ${GREEN}✓${RESET} Python para testes: $PYTHON_BIN"
            fi
            printf "%b\n" "  ${GREEN}✓${RESET} pytest: $($PYTHON_BIN -m pytest --version 2>&1 | head -1)"
        else
            printf "%b\n" "  ${RED}✗${RESET} pytest indisponivel"
            erro=1
        fi
    fi

    # ESP-IDF
    if encontrar_idf; then
        printf "%b\n" "  ${GREEN}✓${RESET} ESP-IDF: $IDF_EXPORT"
    else
        printf "%b\n" "  ${RED}✗${RESET} ESP-IDF nao encontrado"
        printf "%b\n" ""
        printf "%b\n" "    O ESP-IDF e necessario para compilar o firmware."
        printf "%b\n" "    Instale seguindo as instrucoes oficiais:"
        printf "%b\n" "    ${BOLD}https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/${RESET}"
        printf "%b\n" ""
        printf "%b\n" "    Resumo rapido para Ubuntu/Debian:"
        printf "%b\n" "      ${DIM}sudo apt install git wget flex bison gperf python3 python3-pip \\"
        printf "%b\n" "        python3-venv cmake ninja-build ccache libffi-dev libssl-dev \\"
        printf "%b\n" "        dfu-util libusb-1.0-0${RESET}"
        printf "%b\n" "      ${DIM}mkdir -p ~/esp && cd ~/esp${RESET}"
        printf "%b\n" "      ${DIM}git clone -b v5.4 --recursive https://github.com/espressif/esp-idf.git${RESET}"
        printf "%b\n" "      ${DIM}cd esp-idf && ./install.sh esp32s3${RESET}"
        printf "%b\n" ""
        printf "%b\n" "    Depois, defina IDF_PATH ou IDF_EXPORT:"
        printf "%b\n" "      ${BOLD}export IDF_PATH=~/esp/esp-idf${RESET}"
        erro=1
    fi

    # git
    if command -v git >/dev/null 2>&1; then
        printf "%b\n" "  ${GREEN}✓${RESET} git: $(git --version 2>&1)"
    else
        printf "%b\n" "  ${YELLOW}!${RESET} git nao encontrado (opcional, mas recomendado)"
    fi

    # VS Code (opcional)
    if command -v code >/dev/null 2>&1; then
        printf "%b\n" "  ${GREEN}✓${RESET} VS Code: disponivel"
    else
        printf "%b\n" "  ${DIM}-${RESET} VS Code: nao encontrado (opcional, necessario para simulacao Wokwi)"
    fi

    echo
    if [ "$erro" = "1" ]; then
        printf "%b\n" "${RED}Dependencias faltando. Corrija os itens acima e tente novamente.${RESET}"
        return 1
    else
        printf "%b\n" "${GREEN}Todas as dependencias obrigatorias estao presentes.${RESET}"
        return 0
    fi
}

# ============================================================================
# Interface
# ============================================================================

limpar_build_incompativel() {
    local diretorio="$1"
    local build_dir="$2"
    local build_path="$diretorio/$build_dir"
    local cache="$build_path/CMakeCache.txt"
    local origem_build=""

    if [ ! -d "$build_path" ] || [ ! -f "$cache" ]; then
        return 0
    fi

    origem_build="$(sed -n 's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' "$cache" | tail -n 1)"
    if [ -n "$origem_build" ] && [ "$origem_build" != "$diretorio" ]; then
        printf "%b\n" "${YELLOW}==>${RESET} Build antigo detectado em: $build_path"
        printf "%b\n" "${DIM}    Era de: $origem_build${RESET}"
        printf "%b\n" "${CYAN}==>${RESET} Limpando para reconfigurar neste caminho..."
        rm -rf "$build_path"
    fi
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

preparar_log_auditoria() {
    local comando="${1:-./iniciar.sh monitor}"

    mkdir -p "$PASTA/logs"
    if [ ! -f "$AUDITORIA_LOG" ]; then
        cat >"$AUDITORIA_LOG" <<'EOF'
# Log de Auditoria do Jogo da Velha
#
# O firmware emite linhas com a tag JOGO_AUDIT no monitor serial.
# Este arquivo e alimentado automaticamente por:
#   ./iniciar.sh auditoria
#   ./iniciar.sh monitor
#   ./iniciar.sh simular-auditoria
# ou manualmente por:
#   idf.py monitor | tee -a logs/jogo_auditoria.log
EOF
    fi

    {
        echo
        echo "# =================================================================="
        echo "# Sessao de auditoria iniciada em $(date '+%Y-%m-%d %H:%M:%S %z')"
        echo "# Comando: $comando"
        echo "# =================================================================="
    } >>"$AUDITORIA_LOG"
}

preparar_auditoria_wokwi() {
    local comando="${1:-./iniciar.sh simular-auditoria}"

    preparar_log_auditoria "$comando"
    {
        echo "# Modo Wokwi: execute Wokwi: Start Simulator no VS Code."
        echo "# O firmware ja emite JOGO_AUDIT no Serial Monitor do simulador."
        echo "# Se a extensao nao expuser pipe serial, copie as linhas JOGO_AUDIT para esta sessao."
    } >>"$AUDITORIA_LOG"

    printf "%b\n" "${CYAN}==>${RESET} Auditoria Wokwi preparada em: $AUDITORIA_LOG"
}

abrir_monitor_auditoria() {
    local gravar_firmware="${1:-0}"
    local comando="${2:-./iniciar.sh monitor}"

    preparar_log_auditoria "$comando"
    printf "%b\n" "${CYAN}==>${RESET} Abrindo monitor serial com auditoria em: $AUDITORIA_LOG"
    printf "%b\n" "${DIM}    Use Ctrl+] para sair do monitor ESP-IDF.${RESET}"
    printf "%b\n" "${DIM}    As linhas JOGO_AUDIT ficarao salvas para revisao posterior.${RESET}"
    if [ "$gravar_firmware" = "1" ]; then
        printf "%b\n" "${CYAN}==>${RESET} Gravando firmware principal antes do monitor..."
        idf.py flash monitor | tee -a "$AUDITORIA_LOG"
    else
        idf.py monitor | tee -a "$AUDITORIA_LOG"
    fi
}

mostrar_checklist_wokwi() {
    echo
    linha
    printf "%b\n" "${BOLD}${YELLOW}=== Validacao Manual no Wokwi (Checklist) ===${RESET}"
    linha
    printf "%b\n" "Fala pessoal! No VS Code, execute ${BOLD}Wokwi: Start Simulator${RESET}."
    echo
    echo "Checklist de testes:"
    echo "  [ ] OLED exibe o menu principal"
    echo "  [ ] Tecla A inicia a partida por teclado"
    echo "  [ ] Tabuleiro exibe formato com ---+---+---"
    echo "  [ ] Teclas 1 a 9 selecionam posicoes no tabuleiro"
    echo "  [ ] Tecla B exibe o placar"
    echo "  [ ] Tecla D exibe Janiel, Joao e Patrik no About"
    echo "  [ ] LCD1602 IA exibe TFLite na linha 1 e autores rolando na linha 2"
    echo "  [ ] LCD1602 presenca exibe PRESENTE/AUSENTE, distancia e score"
    echo "  [ ] LCD1602 estatisticas exibe tempo, jogadas e media"
    echo "  [ ] Tecla * liga o LED dourado"
    echo "  [ ] Tecla # desliga o LED dourado"
    echo "  [ ] LDR liga automaticamente o LED dourado no escuro"
    echo "  [ ] Serial registra inferencia de presenca do HC-SR04 sempre ativa"
    echo "  [ ] Buzzer toca nas teclas, inicializacao e vitoria"
    echo "  [ ] Partida encerra com vitoria do jogador"
    echo "  [ ] Partida encerra com vitoria do computador"
    echo "  [ ] Partida encerra com empate"
    linha
}

mostrar_auditoria_wokwi() {
    echo
    linha
    printf "%b\n" "${BOLD}${YELLOW}=== Auditoria no Wokwi ===${RESET}"
    linha
    printf "%b\n" "Arquivo preparado: ${BOLD}$AUDITORIA_LOG${RESET}"
    printf "%b\n" "No Serial Monitor do Wokwi, acompanhe as linhas ${BOLD}JOGO_AUDIT${RESET}."
    printf "%b\n" "Em placa real, use ${BOLD}./iniciar.sh auditoria${RESET} para gravar o serial automaticamente."
    linha
}

garantir_alvo_esp32s3() {
    local diretorio="$1"
    local build_dir="$2"
    local sdkconfig="$diretorio/sdkconfig"

    limpar_build_incompativel "$diretorio" "$build_dir"

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
        printf "%b\n" "${BOLD}${CYAN}  === Painel de Controle: Jogo da Velha (Janiel, Joao e Patrik) ===${RESET}"
        linha
        printf "%b\n" "  ${GREEN}1${RESET}) Compilar firmware principal"
        printf "%b\n" "  ${YELLOW}2${RESET}) Limpar tudo e compilar firmware"
        printf "%b\n" "  ${BLUE}3${RESET}) Rodar testes automaticos"
        printf "%b\n" "  ${MAGENTA}4${RESET}) Validar tudo que da pelo terminal"
        printf "%b\n" "  ${CYAN}5${RESET}) Compilar, abrir VS Code e testar no Wokwi"
        printf "%b\n" "  ${YELLOW}6${RESET}) Compilar, abrir VS Code e testar no Wokwi com auditoria"
        printf "%b\n" "  ${BLUE}7${RESET}) Gravar app Unity e abrir monitor serial"
        printf "%b\n" "  ${CYAN}8${RESET}) Abrir projeto e diagram.json no VS Code"
        printf "%b\n" "  ${MAGENTA}9${RESET}) Gravar firmware e monitorar com auditoria"
        printf "%b\n" "  ${CYAN}10${RESET}) Verificar dependencias e preparar .venv"
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
                RODAR_TESTES=1
                COMPILAR_TESTES_C=1
                ABRIR_VSCODE=1
                AUDITORIA_WOKWI=1
                AUDITORIA_COMANDO="./iniciar.sh simular-auditoria"
                return
                ;;
            7)
                FLASH_TESTES_C=1
                return
                ;;
            8)
                SO_ABRIR_VSCODE=1
                return
                ;;
            9)
                MONITOR_AUDITORIA=1
                FLASH_AUDITORIA=1
                AUDITORIA_COMANDO="./iniciar.sh auditoria"
                return
                ;;
            10)
                verificar_dependencias || true
                printf "%b" "\nPressione Enter para voltar ao menu..."
                IFS= read -r _ || true
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
        auditoria|--auditoria)
            MONITOR_AUDITORIA=1
            FLASH_AUDITORIA=1
            AUDITORIA_COMANDO="./iniciar.sh auditoria"
            ;;
        monitor|monitor-auditoria|log|logs|--monitor)
            MONITOR_AUDITORIA=1
            FLASH_AUDITORIA=0
            AUDITORIA_COMANDO="./iniciar.sh monitor"
            ;;
        vscode|abrir|open|--vscode|--open)
            ABRIR_VSCODE=1
            ;;
        simular|wokwi|--simular|--wokwi)
            RODAR_TESTES=1
            COMPILAR_TESTES_C=1
            ABRIR_VSCODE=1
            ;;
        simular-auditoria|wokwi-auditoria|auditoria-wokwi|--simular-auditoria|--wokwi-auditoria)
            RODAR_TESTES=1
            COMPILAR_TESTES_C=1
            ABRIR_VSCODE=1
            AUDITORIA_WOKWI=1
            AUDITORIA_COMANDO="./iniciar.sh simular-auditoria"
            ;;
        somente-vscode|so-vscode|--only-vscode)
            SO_ABRIR_VSCODE=1
            ;;
        setup|dependencias|deps|--setup|--deps)
            verificar_dependencias
            exit $?
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

# Verificacao automatica de dependencias na primeira execucao
if ! encontrar_python; then
    printf "%b\n" "${RED}Erro:${RESET} Python3 nao encontrado."
    printf "%b\n" "Instale com: ${BOLD}sudo apt install python3 python3-pip python3-venv${RESET}"
    exit 1
fi

if [ "$RODAR_TESTES" = "1" ]; then
    if ! garantir_venv; then
        printf "%b\n" "${RED}Erro:${RESET} Nao foi possivel rodar os testes Python."
        exit 1
    fi
fi

if ! encontrar_idf; then
    printf "%b\n" "${RED}Erro:${RESET} ESP-IDF nao encontrado."
    printf "%b\n" ""
    printf "%b\n" "Rode ${BOLD}./iniciar.sh setup${RESET} para ver instrucoes de instalacao,"
    printf "%b\n" "ou defina a variavel de ambiente:"
    printf "%b\n" "  ${BOLD}export IDF_PATH=/caminho/para/esp-idf${RESET}"
    exit 1
fi

cd "$PASTA"

if [ "$SO_ABRIR_VSCODE" = "1" ]; then
    abrir_vscode
    mostrar_checklist_wokwi
    exit 0
fi

printf "%b\n" "${CYAN}==>${RESET} Carregando ESP-IDF de: $IDF_EXPORT"
. "$IDF_EXPORT"

if [ "${CLEAN:-0}" = "1" ] || [ "$LIMPAR" = "1" ]; then
    limpar_build_incompativel "$PASTA" "build"
    if [ -d build ]; then
        printf "%b\n" "${CYAN}==>${RESET} Limpando build principal..."
        idf.py fullclean
    else
        printf "%b\n" "${YELLOW}==>${RESET} Nada para limpar: build/ nao existe."
    fi
fi

if [ "$FLASH_TESTES_C" = "1" ]; then
    printf "%b\n" "${CYAN}==>${RESET} Preparando app Unity/ESP-IDF..."
    preparar_log_auditoria "./iniciar.sh flash-testes"
    garantir_alvo_esp32s3 "$PASTA/test" "build_tests"
    (
        cd "$PASTA/test"
        idf.py -B build_tests build
        idf.py -B build_tests flash monitor | tee -a "$AUDITORIA_LOG"
    )
    exit 0
fi

garantir_alvo_esp32s3 "$PASTA" "build"

printf "%b\n" "${CYAN}==>${RESET} Compilando firmware principal..."
idf.py build

if [ "$MONITOR_AUDITORIA" = "1" ]; then
    abrir_monitor_auditoria "$FLASH_AUDITORIA" "$AUDITORIA_COMANDO"
    exit 0
fi

if [ "$RODAR_TESTES" = "1" ]; then
    printf "%b\n" "${CYAN}==>${RESET} Rodando testes Python..."
    "$PYTHON_BIN" -m pytest test/test_diagram_json.py test/test_pipeline_tictactoe.py test/test_pipeline_presenca.py test/test_requisitos_sistema.py
fi

if [ "$COMPILAR_TESTES_C" = "1" ]; then
    printf "%b\n" "${CYAN}==>${RESET} Compilando app Unity/ESP-IDF dos testes C..."
    garantir_alvo_esp32s3 "$PASTA/test" "build_tests"
    (
        cd "$PASTA/test"
        idf.py -B build_tests build
    )
fi

if [ "$AUDITORIA_WOKWI" = "1" ]; then
    preparar_auditoria_wokwi "$AUDITORIA_COMANDO"
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
    if [ "$AUDITORIA_WOKWI" = "1" ]; then
        mostrar_auditoria_wokwi
    fi
else
    echo
    echo "Para testar no Wokwi depois:"
    echo "  ./iniciar.sh simular"
fi
