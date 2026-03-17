#!/bin/bash

arg1=$1

echo "Hello world with first arg $arg1"

echo "ime skrpite: $0"

echo "vsi argumenti po vrsti: $@"

[[ $arg1 = "argument" ]]

echo "ali je prvi argument enak argument: " $?

sedem=$((2 * 3 + 1))
echo "sedem=$sedem"

exit 0
