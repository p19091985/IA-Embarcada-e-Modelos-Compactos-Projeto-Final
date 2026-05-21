#!/usr/bin/env bash

if [ -z "${BASH_VERSION:-}" ]; then
    exec bash "$0" "$@"
fi

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

detectar_pasta_projeto() {
    local raiz_atual=""

    if command -v git >/dev/null 2>&1; then
        raiz_atual="$(git -C "$PWD" rev-parse --show-toplevel 2>/dev/null || true)"
    fi

    if [ -n "$raiz_atual" ] &&
       [ -f "$raiz_atual/CMakeLists.txt" ] &&
       [ -f "$raiz_atual/diagram.json" ] &&
       [ -f "$raiz_atual/iniciar.sh" ]; then
        printf "%s\n" "$raiz_atual"
        return
    fi

    printf "%s\n" "$SCRIPT_DIR"
}

PASTA="$(detectar_pasta_projeto)"
LIMPAR=0
ABRIR_VSCODE=0
SO_ABRIR_VSCODE=0
RODAR_TESTES=0
COMPILAR_TESTES_C=0
FLASH_TESTES_C=0
USAR_MENU=0
PYTHON_BIN="${PYTHON_BIN:-}"

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

if [ "$SCRIPT_DIR" != "$PASTA" ]; then
    printf "%b\n" "${YELLOW}Aviso:${RESET} script chamado de '$SCRIPT_DIR', usando projeto atual em '$PASTA'."
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
    echo "  ./iniciar.sh setup           verifica e instala dependencias Python/ESP-IDF"
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

garantir_venv() {
    if [ -f "$VENV_DIR/bin/python" ] && "$VENV_DIR/bin/python" -m pytest --version >/dev/null 2>&1; then
        PYTHON_BIN="$VENV_DIR/bin/python"
        return 0
    fi

    local python_base=""
    if command -v python3 >/dev/null 2>&1; then
        python_base=python3
    elif command -v python >/dev/null 2>&1; then
        python_base=python
    else
        return 1
    fi

    if [ ! -f "$VENV_DIR/bin/python" ]; then
        printf "%b\n" "${CYAN}==>${RESET} Criando ambiente virtual em .venv/..."
        "$python_base" -m venv "$VENV_DIR" || {
            printf "%b\n" "${RED}Erro:${RESET} Falha ao criar venv."
            printf "%b\n" "Instale python3-venv: ${BOLD}sudo apt install python3-venv${RESET}"
            return 1
        }
    fi

    PYTHON_BIN="$VENV_DIR/bin/python"

    if [ -f "$PASTA/requirements.txt" ]; then
        printf "%b\n" "${CYAN}==>${RESET} Instalando dependencias Python na .venv/..."
        "$PYTHON_BIN" -m pip install --upgrade pip >/dev/null 2>&1 || true
        "$PYTHON_BIN" -m pip install -r "$PASTA/requirements.txt" >/dev/null 2>&1 || {
            printf "%b\n" "${RED}Erro:${RESET} Falha ao instalar requirements.txt"
            return 1
        }
    fi

    printf "%b\n" "${GREEN}==>${RESET} Ambiente virtual pronto."
    return 0
}

verificar_dependencias() {
    local erro=0

    printf "%b\n" "${CYAN}==> Verificando dependencias...${RESET}"

    # Python
    if encontrar_python; then
        printf "%b\n" "  ${GREEN}✓${RESET} Python: $($PYTHON_BIN --version 2>&1)"
    else
        printf "%b\n" "  ${RED}✗${RESET} Python3 nao encontrado"
        printf "%b\n" "    Instale com: ${BOLD}sudo apt install python3 python3-pip${RESET}"
        erro=1
    fi

    # venv e pytest
    if [ "$erro" = "0" ]; then
        if garantir_venv; then
            printf "%b\n" "  ${GREEN}✓${RESET} venv: $VENV_DIR"
            printf "%b\n" "  ${GREEN}✓${RESET} pytest: $($PYTHON_BIN -m pytest --version 2>&1 | head -1)"
        else
            printf "%b\n" "  ${RED}✗${RESET} Falha ao configurar ambiente virtual"
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
    echo "  [ ] Tecla D exibe Patrik, Janiel e Joao nos creditos"
    echo "  [ ] LCD1602 exibe TFLite na linha 1 e autores rolando na linha 2"
    echo "  [ ] Tecla * liga o LED dourado"
    echo "  [ ] Tecla # desliga o LED dourado"
    echo "  [ ] LDR liga automaticamente o LED dourado no escuro"
    echo "  [ ] Serial registra inferencia de presenca do HC-SR04"
    echo "  [ ] Tecla 9 ativa a coleta CSV opcional do HC-SR04"
    echo "  [ ] Serial exibe timestamp_ms,distancia_cm,eco_us,label"
    echo "  [ ] Buzzer toca nas teclas, inicializacao e vitoria"
    echo "  [ ] Partida encerra com vitoria do jogador"
    echo "  [ ] Partida encerra com vitoria do computador"
    echo "  [ ] Partida encerra com empate"
    linha
}

limpar_build_invalido() {
    local diretorio="$1"
    local build_dir="$2"
    local build_path="$build_dir"
    local cache=""
    local origem=""
    local origem_real=""
    local diretorio_real=""
    local build_real=""

    case "$build_path" in
        /*) ;;
        *) build_path="$diretorio/$build_path" ;;
    esac

    cache="$build_path/CMakeCache.txt"
    if [ ! -f "$cache" ]; then
        return
    fi

    origem="$(awk -F= '/^CMAKE_HOME_DIRECTORY:INTERNAL=/{print $2; exit}' "$cache")"
    if [ -z "$origem" ]; then
        return
    fi

    diretorio_real="$(cd "$diretorio" && pwd -P)"
    if [ -d "$origem" ]; then
        origem_real="$(cd "$origem" && pwd -P)"
    else
        origem_real="$origem"
    fi

    if [ "$origem_real" = "$diretorio_real" ]; then
        return
    fi

    build_real="$(cd "$(dirname "$build_path")" && pwd -P)/$(basename "$build_path")"
    case "$build_real" in
        "$diretorio_real"/*) ;;
        *)
            printf "%b\n" "${RED}Erro:${RESET} build fora do projeto: $build_real"
            exit 1
            ;;
    esac

    printf "%b\n" "${YELLOW}Aviso:${RESET} build antigo aponta para outro projeto:"
    printf "%b\n" "  $origem"
    printf "%b\n" "${CYAN}==>${RESET} Removendo build incompativel: $build_real"
    rm -rf "$build_real"
}

garantir_alvo_esp32s3() {
    local diretorio="$1"
    local build_dir="$2"
    local sdkconfig="$diretorio/sdkconfig"

    limpar_build_invalido "$diretorio" "$build_dir"

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
        printf "%b\n" "${BOLD}${CYAN}  === Painel de Controle: Jogo da Velha (Patrik, Janiel e Joao) ===${RESET}"
        linha
        printf "%b\n" "  ${GREEN}1${RESET}) Compilar firmware principal"
        printf "%b\n" "  ${YELLOW}2${RESET}) Limpar tudo e compilar firmware"
        printf "%b\n" "  ${BLUE}3${RESET}) Rodar testes automaticos"
        printf "%b\n" "  ${MAGENTA}4${RESET}) Validar tudo que da pelo terminal"
        printf "%b\n" "  ${CYAN}5${RESET}) Compilar, abrir VS Code e testar no Wokwi"
        printf "%b\n" "  ${YELLOW}6${RESET}) Gravar app Unity e abrir monitor serial"
        printf "%b\n" "  ${BLUE}7${RESET}) Abrir projeto e diagram.json no VS Code"
        printf "%b\n" "  ${MAGENTA}8${RESET}) Verificar dependencias (setup)"
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
            8)
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
        vscode|abrir|open|--vscode|--open)
            ABRIR_VSCODE=1
            ;;
        simular|wokwi|--simular|--wokwi)
            ABRIR_VSCODE=1
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
        printf "%b\n" "${RED}Erro:${RESET} Falha ao preparar ambiente virtual."
        printf "%b\n" "Instale python3-venv: ${BOLD}sudo apt install python3-venv${RESET}"
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
    if [ -d build ]; then
        printf "%b\n" "${CYAN}==>${RESET} Limpando build principal..."
        idf.py -B build fullclean
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
if ! idf.py -B build build; then
    printf "%b\n" "${RED}Erro:${RESET} Falha ao compilar o firmware principal."
    if [ "$ABRIR_VSCODE" = "1" ]; then
        abrir_vscode
        printf "%b\n" "${YELLOW}Aviso:${RESET} VS Code aberto para voce corrigir a compilacao antes do Wokwi."
    fi
    exit 1
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
