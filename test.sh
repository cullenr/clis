#!/usr/bin/env bash

./main -n osc1 -f 300 -p &
./main -n osc2 -f 303 -p &
./main -n osc3 -f 150 -p &
./main -n osc4 -f 75 -p &
./main -n osc5 -f 37 -p &

read -n 1 -s -r -p "Press any key to continue\n"

pkill -P $$
