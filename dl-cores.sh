#!/usr/bin/env bash
# dl-cores.sh
#############
# Download all cores supported by RALibretro
#
# meleu - March/2018


# GLOBALS #####################################################################
SUPPORTED_CORES=(
    fbalpha fceumm gambatte genesis_plus_gx handy mednafen_ngp
    mednafen_supergrafx mednafen_vb mgba picodrive snes9x stella
)
NIGHTLY_URL="https://buildbot.libretro.com/nightly/windows/x86/latest"
DEST_DIR="bin/Cores"
LOGFILE="dl-cores.log"

###############################################################################


function print_help() {
    echo "USAGE: $0 [CORE_NAME]"
    echo
    echo "List of supported cores:"
    echo "${SUPPORTED_CORES[@]}"
}


function dl_core() {
    [[ -z "$1" ]] && return 1

    local core="${1}_libretro.dll.zip"
    if wget -a "$LOGFILE" -c "${NIGHTLY_URL}/$core" -O "$DEST_DIR/$core"; then
        echo "SUCCESS: downloaded \"$core\"." | tee -a "$LOGFILE" >&2
        if ! unzip -q -o "$DEST_DIR/$core" -d "$DEST_DIR"; then
            echo "WARNING: failed to unzip \"$DEST_DIR/$core\"" | tee -a "$LOGFILE" >&2
            return 1
        fi
    else
        echo "WARNING: failed to download \"$core\"." | tee -a "$LOGFILE" >&2
        return 1
    fi
}


function main() {
    local core="$1"
    local ret=0

    if [[ -z "$1" || "$1" == "-h" || "$1" == "--help" ]]; then
        print_help
        exit 0
    fi

    echo "--- starting $(basename "$0") log - $(date) ---" > "$LOGFILE"
    
    mkdir -p "$DEST_DIR"

    if [[ "$core" == "all" ]]; then
        for core in "${SUPPORTED_CORES[@]}"; do
            dl_core "$core" || ret="$?"
        done
        exit "$ret"
    fi

    # ugly hack to check if an array contains an element
    if [[ "${SUPPORTED_CORES[@]//$1/}" == "${SUPPORTED_CORES[@]}" ]]; then
        echo "ERROR: \"$1\" is NOT a supported core." | tee -a "$LOGFILE" >&2
        exit 1
    fi
}


main "$@"
