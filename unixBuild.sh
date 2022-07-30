#!/bin/bash
mkdir -p buid
cd buid
cmake -S ../ -B .
make && make Shaders && ./LveEngine
cd ..