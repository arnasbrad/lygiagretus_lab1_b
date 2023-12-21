# lygiagretus_lab1_b

## Author: Arnas Bradauskas IFF 1-1

## Dependencies:
```bash
sudo apt install nlohmann-json3-dev
```

## Usage:
```bash
# compile manually
nvcc -c cuda_computation.cu -o cuda_computation.o
g++ -c main.cpp -o main.o
g++ main.o cuda_computation.o -o sunset_computation -lcudart

# OR compile using the premade script
./compile.sh

# run
./sunset_computation <input_file_path>
```


