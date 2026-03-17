#!/bin/bash
while [[ -n $1 ]]; do
    [[ $(( $1 % 4)) -eq 0 ]] && [[ $(($1 % 100)) -ne 0 ]] || [[ $(($1 % 400)) -eq 0 ]] && echo DA || echo NE
    shift
done