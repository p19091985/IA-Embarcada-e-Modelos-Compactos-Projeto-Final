#!/usr/bin/env bash
# monitor.sh — Wrapper para idf.py monitor com captura de log
# Usa config.ini para decidir se salva log ou nao
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CONFIG="$SCRIPT_DIR/config.ini"
LOG_DIR="$SCRIPT_DIR/logs"

log_enabled() {
    if [ ! -f "$CONFIG" ]; then
        return 0
    fi
    local val
    val=$(grep -i '^\s*enabled\s*=' "$CONFIG" 2>/dev/null | tail -1 | sed 's/.*=\s*//' | tr -d '[:space:]')
    case "$val" in
        false|False|FALSE|0|no|No|NO) return 1 ;;
        *) return 0 ;;
    esac
}

TIMESTAMP=$(date +"%Y-%m-%d_%H-%M-%S")

if log_enabled; then
    mkdir -p "$LOG_DIR"
    LOGFILE="$LOG_DIR/${TIMESTAMP}.log"
    echo -e "\033[1;36m[i] Salvando log do monitor serial em:\033[0m $LOGFILE"
    source ~/.espressif/v6.0.1/esp-idf/export.sh > /dev/null 2>&1
    idf.py monitor 2>&1 | tee "$LOGFILE"
else
    echo -e "\033[1;33m[!] Log desabilitado (configure enabled = true em config.ini se precisar).\033[0m"
    source ~/.espressif/v6.0.1/esp-idf/export.sh > /dev/null 2>&1
    idf.py monitor
fi
