#!/usr/bin/env bash

./main -n lfo1 -f 0.5 &
./main -n lfo2 -f 1 &
./main -n osc -f 40 -f lfo1:sqr:0.0001 -f lfo2:sqr:0.0001 -p &

read -n 1 -s -r -p "Press any key to continue\n"

pkill -2 -P $$
