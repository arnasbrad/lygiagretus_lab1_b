#include <cuda_runtime.h>
#include "sunset.h"
#include <stdio.h>

__global__ void computeValueKernel(const double* lat, const double* lng, const int* guessHour, double* output, int numEntries) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < numEntries) {
        double temp = 0;
        // Added complexity: Perform multiple operations in a loop
        for (int i = 0; i < 10000; ++i) {
            temp += sin(lat[idx]) * cos(lng[idx]) * tan(0.1 * guessHour[idx]) - log(1 + fabs(lat[idx]));
            temp *= 1.1; // Arbitrary multiplier for added complexity
        }
        output[idx] = temp;
    }
}

extern "C" void runCudaComputation(const sunset* sunsets, double* output, int numEntries) {
    double *d_lat, *d_lng;
    int *d_guessHour;
    double *d_output;

    cudaMalloc(&d_lat, numEntries * sizeof(double));
    cudaMalloc(&d_lng, numEntries * sizeof(double));
    cudaMalloc(&d_guessHour, numEntries * sizeof(int));
    cudaMalloc(&d_output, numEntries * sizeof(double));

    // Properly copy data to device arrays
    for (int i = 0; i < numEntries; ++i) {
        cudaMemcpy(&d_lat[i], &sunsets[i].lat, sizeof(double), cudaMemcpyHostToDevice);
        cudaMemcpy(&d_lng[i], &sunsets[i].lng, sizeof(double), cudaMemcpyHostToDevice);
        cudaMemcpy(&d_guessHour[i], &sunsets[i].guessHour, sizeof(int), cudaMemcpyHostToDevice);
    }

    int blockSize = 32;
    int numBlocks = (numEntries + blockSize - 1) / blockSize;
    computeValueKernel<<<numBlocks, blockSize>>>(d_lat, d_lng, d_guessHour, d_output, numEntries);

    // Add error checking
    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) {
        fprintf(stderr, "CUDA Error: %s\n", cudaGetErrorString(err));
        // Handle error...
    }

    cudaMemcpy(output, d_output, numEntries * sizeof(double), cudaMemcpyDeviceToHost);

    cudaFree(d_lat);
    cudaFree(d_lng);
    cudaFree(d_guessHour);
    cudaFree(d_output);
}
