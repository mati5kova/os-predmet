#!/bin/bash

cat "$1" | egrep "^[^#\s]" | cut -d: -f2 | sort | uniq -c | sort -r | nl

exit 0