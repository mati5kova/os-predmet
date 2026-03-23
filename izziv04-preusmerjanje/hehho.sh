#!/bin/bash

while [[ -n $1 ]]; do
    echo "$1" | sed -e 's/a/ha/g' -e 's/e/he/g' -e 's/i/hi/g' -e 's/o/ho/g' -e 's/u/hu/g'
    shift
done | nl -v0 -s ": "

exit 0