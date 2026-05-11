@echo off
setlocal enabledelayedexpansion

set "PASTA=%~dp0"
set "PASTA=%PASTA:~0,-1%"
set "LIMPAR=0"
set "ABRIR_VSCODE=0"
set "SO_ABRIR_VSCODE=0"
set "RODAR_TESTES=0"
set "COMPILAR_TESTES_C=0"
set "FLASH_TESTES_C=0"
set "USAR_MENU=0"
set "VENV_DIR=%PASTA%\.venv"

if "%~1"=="" goto :menu
if "%~1"=="menu" goto :menu

:parse_args
if "%~1"=="" goto :depois_args

if /i "%~1"=="build"          goto :arg_build
if /i "%~1"=="compilar"       goto :arg_build
if /i "%~1"=="limpar"         goto :arg_limpar
if /i "%~1"=="clean"          goto :arg_limpar
if /i "%~1"=="testar"         goto :arg_testar
if /i "%~1"=="teste"          goto :arg_testar
if /i "%~1"=="testes"         goto :arg_testar
if /i "%~1"=="test"           goto :arg_testar
if /i "%~1"=="validar"        goto :arg_validar
if /i "%~1"=="validate"       goto :arg_validar
if /i "%~1"=="unity"          goto :arg_unity
if /i "%~1"=="testes-c"       goto :arg_unity
if /i "%~1"=="flash-testes"   goto :arg_flash
if /i "%~1"=="flash-tests"    goto :arg_flash
if /i "%~1"=="vscode"         goto :arg_vscode
if /i "%~1"=="abrir"          goto :arg_vscode
if /i "%~1"=="simular"        goto :arg_simular
if /i "%~1"=="wokwi"          goto :arg_simular
if /i "%~1"=="somente-vscode" goto :arg_so_vscode
if /i "%~1"=="so-vscode"      goto :arg_so_vscode
if /i "%~1"=="setup"          goto :arg_setup
if /i "%~1"=="dependencias"   goto :arg_setup
if /i "%~1"=="deps"           goto :arg_setup
if /i "%~1"=="ajuda"          goto :uso
if /i "%~1"=="help"           goto :uso
if /i "%~1"=="-h"             goto :uso
if /i "%~1"=="--help"         goto :uso

echo Opcao desconhecida: %~1
goto :uso

:arg_build
shift
goto :parse_args

:arg_limpar
set "LIMPAR=1"
shift
goto :parse_args

:arg_testar
set "RODAR_TESTES=1"
set "COMPILAR_TESTES_C=1"
shift
goto :parse_args

:arg_validar
set "RODAR_TESTES=1"
set "COMPILAR_TESTES_C=1"
shift
goto :parse_args

:arg_unity
set "COMPILAR_TESTES_C=1"
shift
goto :parse_args

:arg_flash
set "FLASH_TESTES_C=1"
shift
goto :parse_args

:arg_vscode
set "ABRIR_VSCODE=1"
shift
goto :parse_args

:arg_simular
set "RODAR_TESTES=1"
set "COMPILAR_TESTES_C=1"
set "ABRIR_VSCODE=1"
shift
goto :parse_args

:arg_so_vscode
set "SO_ABRIR_VSCODE=1"
shift
goto :parse_args

:arg_setup
call :verificar_dependencias
goto :fim

:: ============================================================================
:: Uso
:: ============================================================================

:uso
echo.
echo Uso:
echo   iniciar.bat                 abre o menu interativo
echo   iniciar.bat build           compila o firmware principal
echo   iniciar.bat testar          roda pytest e compila testes Unity
echo   iniciar.bat validar         compila firmware + roda testes + compila Unity
echo   iniciar.bat vscode          compila e abre o projeto no VS Code
echo   iniciar.bat simular         igual a vscode, com checklist para Wokwi
echo   iniciar.bat unity           compila apenas o app Unity/ESP-IDF
echo   iniciar.bat flash-testes    grava e abre monitor serial do app Unity
echo   iniciar.bat setup           verifica e instala dependencias
echo   iniciar.bat limpar validar  limpa build e executa validacao
echo.
goto :fim

:: ============================================================================
:: Menu interativo
:: ============================================================================

:menu
cls
echo ------------------------------------------------------------
echo   IA Embarcada - Jogo da Velha ESP32-S3
echo ------------------------------------------------------------
echo   1) Compilar firmware principal
echo   2) Limpar tudo e compilar firmware
echo   3) Rodar testes automaticos
echo   4) Validar tudo que da pelo terminal
echo   5) Compilar, abrir VS Code e testar no Wokwi
echo   6) Gravar app Unity e abrir monitor serial
echo   7) Abrir projeto e diagram.json no VS Code
echo   8) Verificar dependencias (setup)
echo   0) Sair
echo ------------------------------------------------------------
set /p "OPCAO=Escolha uma opcao: "

if "%OPCAO%"=="1" goto :depois_args
if "%OPCAO%"=="2" (set "LIMPAR=1" & goto :depois_args)
if "%OPCAO%"=="3" (set "RODAR_TESTES=1" & set "COMPILAR_TESTES_C=1" & goto :depois_args)
if "%OPCAO%"=="4" (set "RODAR_TESTES=1" & set "COMPILAR_TESTES_C=1" & goto :depois_args)
if "%OPCAO%"=="5" (set "RODAR_TESTES=1" & set "COMPILAR_TESTES_C=1" & set "ABRIR_VSCODE=1" & goto :depois_args)
if "%OPCAO%"=="6" (set "FLASH_TESTES_C=1" & goto :depois_args)
if "%OPCAO%"=="7" (set "SO_ABRIR_VSCODE=1" & goto :depois_args)
if "%OPCAO%"=="8" (call :verificar_dependencias & pause & goto :menu)
if "%OPCAO%"=="0" (echo Saindo. Nada foi alterado. & goto :fim)
if /i "%OPCAO%"=="s" (echo Saindo. Nada foi alterado. & goto :fim)
if /i "%OPCAO%"=="q" (echo Saindo. Nada foi alterado. & goto :fim)

echo Opcao invalida.
timeout /t 1 /nobreak >nul
goto :menu

:: ============================================================================
:: Verificacao de dependencias
:: ============================================================================

:verificar_dependencias
echo.
echo ==^> Verificando dependencias...

:: Python
call :encontrar_python
if errorlevel 1 (
    echo   [X] Python3 nao encontrado
    echo       Instale o Python 3 de https://www.python.org/downloads/
    echo       Marque "Add Python to PATH" durante a instalacao.
) else (
    for /f "tokens=*" %%v in ('"%PYTHON_BIN%" --version 2^>^&1') do set "PY_VER=%%v"
    echo   [OK] Python: !PY_VER!
)

:: venv e pytest
if defined PYTHON_BIN (
    call :garantir_venv
    if errorlevel 1 (
        echo   [X] Falha ao configurar ambiente virtual
    ) else (
        echo   [OK] venv: %VENV_DIR%
        for /f "tokens=*" %%v in ('"%PYTHON_BIN%" -m pytest --version 2^>^&1') do set "PT_VER=%%v"
        echo   [OK] pytest: !PT_VER!
    )
)

:: ESP-IDF
call :encontrar_idf
if errorlevel 1 (
    echo   [X] ESP-IDF nao encontrado
    echo.
    echo       O ESP-IDF e necessario para compilar o firmware.
    echo       Instale seguindo as instrucoes oficiais:
    echo       https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/windows-setup.html
    echo.
    echo       Ou use o instalador oficial:
    echo       https://dl.espressif.com/dl/esp-idf/
    echo.
    echo       Depois, defina IDF_PATH ou use o ESP-IDF Command Prompt.
) else (
    echo   [OK] ESP-IDF: !IDF_EXPORT!
)

:: git
where git >nul 2>&1
if errorlevel 1 (
    echo   [!] git nao encontrado (opcional^)
) else (
    for /f "tokens=*" %%v in ('git --version 2^>^&1') do echo   [OK] git: %%v
)

:: VS Code
where code >nul 2>&1
if errorlevel 1 (
    echo   [-] VS Code: nao encontrado (opcional, necessario para Wokwi^)
) else (
    echo   [OK] VS Code: disponivel
)

echo.
exit /b 0

:: ============================================================================
:: Encontrar Python
:: ============================================================================

:encontrar_python
if defined PYTHON_BIN exit /b 0

if exist "%VENV_DIR%\Scripts\python.exe" (
    set "PYTHON_BIN=%VENV_DIR%\Scripts\python.exe"
    exit /b 0
)

where python >nul 2>&1
if not errorlevel 1 (
    set "PYTHON_BIN=python"
    exit /b 0
)

where python3 >nul 2>&1
if not errorlevel 1 (
    set "PYTHON_BIN=python3"
    exit /b 0
)

exit /b 1

:: ============================================================================
:: Garantir venv com pytest
:: ============================================================================

:garantir_venv
if exist "%VENV_DIR%\Scripts\python.exe" (
    "%VENV_DIR%\Scripts\python.exe" -m pytest --version >nul 2>&1
    if not errorlevel 1 (
        set "PYTHON_BIN=%VENV_DIR%\Scripts\python.exe"
        exit /b 0
    )
)

:: Encontra python base do sistema
set "PY_BASE="
where python >nul 2>&1
if not errorlevel 1 (set "PY_BASE=python") else (
    where python3 >nul 2>&1
    if not errorlevel 1 (set "PY_BASE=python3") else (exit /b 1)
)

if not exist "%VENV_DIR%\Scripts\python.exe" (
    echo ==^> Criando ambiente virtual em .venv\...
    "%PY_BASE%" -m venv "%VENV_DIR%"
    if errorlevel 1 (
        echo [ERRO] Falha ao criar venv.
        exit /b 1
    )
)

set "PYTHON_BIN=%VENV_DIR%\Scripts\python.exe"

if exist "%PASTA%\requirements.txt" (
    echo ==^> Instalando dependencias Python na .venv\...
    "%PYTHON_BIN%" -m pip install --upgrade pip >nul 2>&1
    "%PYTHON_BIN%" -m pip install -r "%PASTA%\requirements.txt" >nul 2>&1
    if errorlevel 1 (
        echo [ERRO] Falha ao instalar requirements.txt
        exit /b 1
    )
)

echo ==^> Ambiente virtual pronto.
exit /b 0

:: ============================================================================
:: Encontrar ESP-IDF
:: ============================================================================

:encontrar_idf
if defined IDF_EXPORT (
    if exist "!IDF_EXPORT!" exit /b 0
)

if defined IDF_PATH (
    if exist "!IDF_PATH!\export.bat" (
        set "IDF_EXPORT=!IDF_PATH!\export.bat"
        exit /b 0
    )
)

:: Procura em locais comuns no Windows
set "CAMINHOS_IDF="
set "CAMINHOS_IDF=!CAMINHOS_IDF! %USERPROFILE%\esp\esp-idf\export.bat"
set "CAMINHOS_IDF=!CAMINHOS_IDF! %USERPROFILE%\.espressif\esp-idf\export.bat"
set "CAMINHOS_IDF=!CAMINHOS_IDF! C:\esp\esp-idf\export.bat"
set "CAMINHOS_IDF=!CAMINHOS_IDF! C:\Espressif\frameworks\esp-idf-v5.4\export.bat"
set "CAMINHOS_IDF=!CAMINHOS_IDF! C:\Espressif\frameworks\esp-idf\export.bat"

for %%p in (!CAMINHOS_IDF!) do (
    if exist "%%p" (
        set "IDF_EXPORT=%%p"
        exit /b 0
    )
)

:: Procura em subpastas de .espressif
if exist "%USERPROFILE%\.espressif" (
    for /d %%d in ("%USERPROFILE%\.espressif\*") do (
        if exist "%%d\esp-idf\export.bat" (
            set "IDF_EXPORT=%%d\esp-idf\export.bat"
            exit /b 0
        )
    )
)

exit /b 1

:: ============================================================================
:: Garantir alvo ESP32-S3
:: ============================================================================

:garantir_alvo
set "DIR_ALVO=%~1"
set "BUILD_ALVO=%~2"
set "SDK_ALVO=%DIR_ALVO%\sdkconfig"

if exist "%SDK_ALVO%" (
    findstr /c:"CONFIG_IDF_TARGET=\"esp32s3\"" "%SDK_ALVO%" >nul 2>&1
    if not errorlevel 1 (
        echo ==^> Alvo ESP32-S3 ja configurado em: %SDK_ALVO%
        exit /b 0
    )
)

echo ==^> Configurando alvo ESP32-S3 em: %DIR_ALVO%
pushd "%DIR_ALVO%"
idf.py -B "%BUILD_ALVO%" set-target esp32s3
popd
exit /b 0

:: ============================================================================
:: Abrir VS Code
:: ============================================================================

:abrir_vscode
where code >nul 2>&1
if errorlevel 1 (
    echo [AVISO] Nao achei o comando 'code' para abrir o VS Code.
    echo Abra manualmente esta pasta no VS Code: %PASTA%
    exit /b 0
)
echo ==^> Abrindo projeto e diagram.json no VS Code...
code --reuse-window "%PASTA%" "%PASTA%\diagram.json" >nul 2>&1
exit /b 0

:: ============================================================================
:: Checklist Wokwi
:: ============================================================================

:mostrar_checklist
echo.
echo ------------------------------------------------------------
echo   Validacao manual no Wokwi
echo ------------------------------------------------------------
echo   No VS Code, execute: Wokwi: Start Simulator
echo.
echo   Checklist:
echo     [ ] OLED exibe o menu principal
echo     [ ] Tecla A inicia a partida por teclado
echo     [ ] Tabuleiro exibe formato com ---+---+---
echo     [ ] Teclas 1 a 9 selecionam posicoes no tabuleiro
echo     [ ] Tecla B exibe o placar
echo     [ ] Tecla D exibe o autor
echo     [ ] LCD1602 exibe o algoritmo de IA utilizado
echo     [ ] Tecla * liga o LED dourado
echo     [ ] Tecla # desliga o LED dourado
echo     [ ] Tecla 8 inicia o modo gesto/auto-scan
echo     [ ] No modo 8, gesto confirma a casa destacada
echo     [ ] Tecla 9 ativa o modo de coleta do MPU6050
echo     [ ] Serial exibe timestamp_ms,ax,ay,az,label a 50 Hz
echo     [ ] Na coleta, 0 define label repouso e 1 define label confirmacao
echo     [ ] Na coleta, D encerra e retorna ao menu
echo     [ ] Buzzer toca nas teclas, inicializacao e vitoria
echo     [ ] Partida encerra com vitoria do jogador
echo     [ ] Partida encerra com vitoria do computador
echo     [ ] Partida encerra com empate
echo ------------------------------------------------------------
exit /b 0

:: ============================================================================
:: Fluxo principal
:: ============================================================================

:depois_args

:: Verificar Python
call :encontrar_python
if errorlevel 1 (
    echo [ERRO] Python3 nao encontrado.
    echo Instale o Python 3 de https://www.python.org/downloads/
    goto :fim
)

:: Garantir venv se vai rodar testes
if "%RODAR_TESTES%"=="1" (
    call :garantir_venv
    if errorlevel 1 (
        echo [ERRO] Falha ao preparar ambiente virtual.
        goto :fim
    )
)

:: Encontrar ESP-IDF
call :encontrar_idf
if errorlevel 1 (
    echo [ERRO] ESP-IDF nao encontrado.
    echo Rode "iniciar.bat setup" para ver instrucoes de instalacao,
    echo ou defina a variavel de ambiente:
    echo   set IDF_PATH=C:\caminho\para\esp-idf
    goto :fim
)

:: Somente abrir VS Code
if "%SO_ABRIR_VSCODE%"=="1" (
    call :abrir_vscode
    call :mostrar_checklist
    goto :fim
)

:: Carregar ESP-IDF
echo ==^> Carregando ESP-IDF de: %IDF_EXPORT%
call "%IDF_EXPORT%"

:: Limpar se solicitado
if "%LIMPAR%"=="1" (
    if exist "%PASTA%\build" (
        echo ==^> Limpando build principal...
        pushd "%PASTA%"
        idf.py fullclean
        popd
    ) else (
        echo ==^> Nada para limpar: build\ nao existe.
    )
)

:: Flash testes
if "%FLASH_TESTES_C%"=="1" (
    echo ==^> Preparando app Unity/ESP-IDF...
    call :garantir_alvo "%PASTA%\test" "build_tests"
    pushd "%PASTA%\test"
    idf.py -B build_tests build
    idf.py -B build_tests flash monitor
    popd
    goto :fim
)

:: Build principal
pushd "%PASTA%"
call :garantir_alvo "%PASTA%" "build"
echo ==^> Compilando firmware principal...
idf.py build
popd

:: Testes Python
if "%RODAR_TESTES%"=="1" (
    echo ==^> Rodando testes Python...
    pushd "%PASTA%"
    "%PYTHON_BIN%" -m pytest test\test_diagram_json.py test\test_pipeline_tictactoe.py test\test_pipeline_gestos.py
    popd
)

:: Testes Unity C
if "%COMPILAR_TESTES_C%"=="1" (
    echo ==^> Compilando app Unity/ESP-IDF dos testes C...
    call :garantir_alvo "%PASTA%\test" "build_tests"
    pushd "%PASTA%\test"
    idf.py -B build_tests build
    popd
)

:: Abrir VS Code
if "%ABRIR_VSCODE%"=="1" (
    call :abrir_vscode
)

:: Resultado
echo.
echo Pronto.
echo Arquivos principais gerados:
echo   - build\jogo_da_velha_esp32s3.elf
echo   - build\flasher_args.json

if "%COMPILAR_TESTES_C%"=="1" (
    echo   - test\build_tests\jogo_da_velha_unity_tests.bin
)

if "%ABRIR_VSCODE%"=="1" (
    call :mostrar_checklist
) else (
    echo.
    echo Para testar no Wokwi depois:
    echo   iniciar.bat simular
)

:fim
endlocal
exit /b 0
