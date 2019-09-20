#!/usr/bin/env bash

./main -n lfo -f 1 &
./main -n osc -f lfo:saw:2000 -p &

read -n 1 -s -r -p "Press any key to continue\n"

pkill -2 -P $$
