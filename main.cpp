#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <nlohmann/json.hpp>
#include <algorithm>
#include "sunset.h"

using json = nlohmann::json;
using namespace std;

extern "C" void runCudaComputation(const sunset* sunsets, double* output, int numEntries);

vector<sunset> readSunsets(const string& jsonFilePath) {
    ifstream file(jsonFilePath);
    if (!file.is_open()) {
        cerr << "Could not open file: " << jsonFilePath << endl;
        exit(1);
    }

    json j;
    file >> j;
    file.close();

    vector<sunset> sunsets;
    for (const auto& element : j) {
        sunset s;
        s.lat = element["Lat"].get<double>();
        s.lng = element["Lng"].get<double>();
        s.guessHour = element["Hour"].get<int>();
        sunsets.push_back(s);
    }
    return sunsets;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " <JSON file path>" << endl;
        return 1;
    }

    string fileName = argv[1];
    vector<sunset> sunsets = readSunsets(fileName);

    int numEntries = sunsets.size();
    vector<double> output(numEntries);

    auto start = chrono::high_resolution_clock::now();
    runCudaComputation(sunsets.data(), output.data(), numEntries);
    auto stop = chrono::high_resolution_clock::now();

    auto duration = chrono::duration_cast<chrono::milliseconds>(stop - start);

    // Convert double results to strings and sort
    vector<string> results(numEntries);
    for (int i = 0; i < numEntries; ++i) {
        results[i] = to_string(output[i]);
    }

    std::ofstream outFile("results.txt");

    if (!outFile) {
        cerr << "Error: Unable to open file for writing." << endl;
        return 1;
    }

    for (const auto& result : results) {
        outFile << result << endl;
    }

    // Close the file stream
    outFile.close();

    cout << "CUDA computation took " << duration.count() << " milliseconds." << endl;

    return 0;
}

