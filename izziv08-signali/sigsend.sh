#!/bin/bash

if [ $# -lt 1 ]; then
    echo "Uporaba: $0 <PID|ime_procesa>"
    exit 1
fi

PROC="$1"

# Če je prvi znak števka, argument obravnavamo kot PID
if [[ "$PROC" =~ ^[0-9] ]]; then
    PID="$PROC"
else
    # Poiščemo proces po imenu
    PID=$(pgrep -n "$PROC")

    if [ -z "$PID" ]; then
        echo "Proces z imenom '$PROC' ni bil najden."
        exit 1
    fi
fi

help() {
    echo "Ukazi:"
    echo "  x - izhod iz programa"
    echo "  h - izpis pomoči"
    echo "  w - pošlji SIGHUP"
    echo "  i - pošlji SIGINT"
    echo "  q - pošlji SIGQUIT"
    echo "  t - pošlji SIGTERM"
    echo "  k - pošlji SIGKILL"
    echo "  c - pošlji SIGCONT"
    echo "  s - pošlji SIGSTOP"
    echo "  z - pošlji SIGTSTP"
    echo "  1 - pošlji SIGUSR1"
    echo "  2 - pošlji SIGUSR2"
    echo "  y - pošlji SIGCHLD"
}

send_signal() {
    SIGNAL="$1"

    # Preverimo, ali proces še obstaja
    if ! kill -0 "$PID" 2>/dev/null; then
        echo "Proces s PID $PID ne obstaja več. Izhod."
        exit 1
    fi

    kill -s "$SIGNAL" "$PID"

    if [ $? -eq 0 ]; then
        echo "Uspešno poslan signal $SIGNAL procesu s PID $PID."
    else
        echo "Napaka pri pošiljanju signala $SIGNAL procesu s PID $PID."
    fi
}

echo "Ciljni proces: PID $PID"
echo "Pritisnite h za pomoč."

while true; do
    echo -n "Ukaz: "
    read -r -n 1 KEY
    echo

    case "$KEY" in
        x)
            echo "Izhod iz programa."
            exit 0
            ;;
        h)
            help
            ;;
        w)
            send_signal "HUP"
            ;;
        i)
            send_signal "INT"
            ;;
        q)
            send_signal "QUIT"
            ;;
        t)
            send_signal "TERM"
            ;;
        k)
            send_signal "KILL"
            ;;
        c)
            send_signal "CONT"
            ;;
        s)
            send_signal "STOP"
            ;;
        z)
            send_signal "TSTP"
            ;;
        1)
            send_signal "USR1"
            ;;
        2)
            send_signal "USR2"
            ;;
        y)
            send_signal "CHLD"
            ;;
        *)
            echo "Neznan ukaz. Pritisnite h za pomoč."
            ;;
    esac
done