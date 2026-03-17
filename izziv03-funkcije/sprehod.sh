#!/bin/bash

g_imenik=${2:-"."}
g_globina=${3:-3}

function izpisi_globino {
    for (( i = 0; i < $1; i++ )); do 
        echo -n "----"; 
    done
}

function do_drevo {
    local l_imenik=$1
    local l_globina=$2
    
    [[ $l_globina -gt $g_globina ]] && return 0
    
    for el in "$l_imenik"/*; do
        [[ -e "$el" || -L "$el" ]] || continue
        [[ -L "$el" ]] && izpisi_globino $l_globina && echo "LINK  ${el##*/}" && continue
        [[ -f "$el" ]] && izpisi_globino $l_globina && echo "FILE  ${el##*/}" && continue
        [[ -b "$el" ]] && izpisi_globino $l_globina && echo "BLOCK ${el##*/}" && continue
        [[ -c "$el" ]] && izpisi_globino $l_globina && echo "CHAR  ${el##*/}" && continue
        [[ -p "$el" ]] && izpisi_globino $l_globina && echo "PIPE  ${el##*/}" && continue
        [[ -S "$el" ]] && izpisi_globino $l_globina && echo "SOCK  ${el##*/}" && continue
        [[ -d "$el" ]] && izpisi_globino $l_globina && echo "DIR   ${el##*/}/ globina $l_globina" && do_drevo "$el" $(( $l_globina + 1 )) && continue
    done
}

g_size=0
g_blocks=0

function do_prostor {
    local l_imenik=$1
    local l_globina=$2
    local l_size=0
    local l_blocks=0
    
    [[ $l_globina -gt $g_globina ]] && return 0
    
    for el in "$l_imenik"/*; do
        [[ -e "$el" || -L "$el" ]] || continue
        
        if [[ -L "$el" || -f "$el" || -b "$el" || -c "$el" || -p "$el" || -S "$el" ]]; then
            eval "$(stat -s "$el")"
            (( l_size += $st_size ))
            (( l_blocks += $st_blocks ))
        fi
        
        [[ -d "$el" ]] && do_prostor "$el" $(( $l_globina + 1 ))
    done
    (( g_size += l_size ))
    (( g_blocks += l_blocks ))
}


case "$1" in
    drevo) do_drevo "$g_imenik" 0 ;;
    prostor) do_prostor "$g_imenik" 0 && echo "Velikost: $g_size" && echo "Blokov: $g_blocks" && echo "Prostor: $(( g_blocks * 512 ))" ;;
    *) echo "Napačna uporaba" ;;
esac

exit 0