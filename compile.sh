#!/bin/bash

nvcc -c cuda_computation.cu -o cuda_computation.o
g++ -c main.cpp -o main.o
g++ main.o cuda_computation.o -o sunset_computation -lcudart
