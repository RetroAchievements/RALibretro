#!/usr/bin/env bash
# dl-cores.sh
#############
# Download all cores supported by RALibretro
#
# dependencies: wget, unzip
#
# meleu - March/2018


# GLOBALS #####################################################################
SUPPORTED_CORES=(
    fbalpha fceumm gambatte genesis_plus_gx handy mednafen_ngp
    mednafen_supergrafx mednafen_vb mgba picodrive snes9x stella
    prosystem
)
NIGHTLY_URL="https://buildbot.libretro.com/nightly/windows/x86/latest"
DEST_DIR="bin/Cores"
DEPS=(wget unzip)
# uncomment the line below to have a logfile
#LOGFILE="dl-cores.log"
###############################################################################


function print_help() {
    echo "USAGE: $0 [CORE_NAME]"
    echo
    echo "The CORE_NAME must be a supported core."
    echo "Or use 'all' to download all supported cores."
    echo
    echo "List of supported cores:"
    echo "${SUPPORTED_CORES[@]}"
}


function check_deps() {
    local dep
    local hasPacman=false
    local ret=0

    which pacman > /dev/null 2>&1 && hasPacman=true

    for dep in "${DEPS[@]}"; do
        which "$dep" > /dev/null 2>&1 && continue

        echolog "WARNING: \"$dep\" is a dependency for this script and is not installed." >&2

        if [[ "$hasPacman" == true ]]; then
            echolog "Trying to install \"$dep\"..." >&2
            if ! pacman -Sq --noconfirm "$dep"; then
                echolog "ERROR: failed to install the \"$dep\" dependency." >&2
                ret=1
            fi
        else
            ret=1
        fi
    done

    if [[ "$ret" != 0 ]]; then
        echolog "Try to install the dependencies and then run the script again." >&2
        exit 1
    fi
}


function echolog() {
    if [[ -f "$LOGFILE" ]]; then
        echo "$@" | tee -a "$LOGFILE"
    else
        echo "$@"
    fi
}


function dl_core() {
    [[ -z "$1" ]] && return 1

    local core="${1}_libretro.dll.zip"

    echolog -e "\nDownloading \"$core\". Please wait..."
    if wget -nv -a "$LOGFILE" "${NIGHTLY_URL}/$core" -O "$DEST_DIR/$core"; then
        echolog "--- Downloaded \"$core\"." >&2
        if ! unzip -o "$DEST_DIR/$core" -d "$DEST_DIR" >> "$LOGFILE" ; then
            echolog "WARNING: failed to unzip \"$DEST_DIR/$core\"" >&2
            return 1
        fi
	rm -fv "$DEST_DIR/$core"
    else
        echolog "WARNING: failed to download \"$core\"." >&2
        return 1
    fi
}


function main() {
    if [[ -z "$1" || "$1" == "-h" || "$1" == "--help" ]]; then
        print_help
        exit 0
    fi

    local core="$1"
    local ret=0
    local failed_cores=()

    if [[ -f "$LOGFILE" ]]; then
        echo "--- starting $(basename "$0") log - $(date) ---" > "$LOGFILE"
    else
        LOGFILE="/dev/tty"
    fi

    check_deps

    mkdir -p "$DEST_DIR"

    if [[ "$core" == "all" ]]; then
        echolog "--- DOWNLOADING ALL SUPPORTED CORES ---"
        for core in "${SUPPORTED_CORES[@]}"; do
            if dl_core "$core"; then
                echolog "SUCCESS: \"$core\" has been installed"
            else
                echolog "WARNING: failed to install \"$core\"."
                failed_cores+=("$core")
                ret=1
            fi
        done
        if [[ "$ret" == 0 ]]; then
            echolog "--- ALL CORES WERE SUCCESSFULLY INSTALLED ---"
            exit 0
        else
            echolog -e "\n--- WARNING! WARNING! WARNING! ---"
            echolog -e "Failed to install the following cores:\n${failed_cores[@]}"
            echolog "--- FINISH ---"
            exit "$ret"
        fi
    fi

    # ugly hack to check if an array contains an element
    if [[ "${SUPPORTED_CORES[@]//$core/}" == "${SUPPORTED_CORES[@]}" ]]; then
        echolog "ERROR: \"$core\" is NOT a supported core." >&2
        exit 1
    fi

    if dl_core "$core"; then
        echolog "SUCCESS: \"$core\" has been installed"
    else
        echolog "WARNING: failed to install \"$core\"."
        exit 1
    fi
}


main "$@"

