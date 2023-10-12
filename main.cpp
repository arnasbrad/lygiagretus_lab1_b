#include "fort.h"
#include <cmath>
#include <cstdio>
#include <curl/curl.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <nlohmann/json.hpp>
#include <omp.h>
#include <string>
#include <vector>

using namespace std;

struct sunset {
  string date;
  double lat;
  double lng;
  int guessHour;
};

struct sunsetWithComputedValue {
  sunset sunsetObj;
  int sunsetHour;
};

class sortedResultMonitor {
private:
  sunsetWithComputedValue sunsetResults[100];
  int count;
  const int capacity;
  omp_lock_t lock{};

public:
  explicit sortedResultMonitor(int capacity) : capacity(capacity) {
    count = 0;
    omp_init_lock(&lock);
  }

  ~sortedResultMonitor() { omp_destroy_lock(&lock); }

  void addItemSorted(const sunsetWithComputedValue &sunsetResult) {
    omp_set_lock(&lock);

    int i;
    for (i = 0; i < count; i++) {
      if (sunsetResults[i].sunsetObj.guessHour >
          sunsetResult.sunsetObj.guessHour)
        break;
    }

    for (int j = count; j > i; j--) {
      sunsetResults[j] = sunsetResults[j - 1];
    }

    sunsetResults[i] = sunsetResult;
    count++;

    omp_unset_lock(&lock);
  }

  sunsetWithComputedValue getItem(int index) { return sunsetResults[index]; }

  int getCount() { return count; }
};

std::array<sunset, 25> readSunsets(const std::string &jsonFilePath) {
  std::ifstream file(jsonFilePath);

  if (!file.is_open()) {
    std::cerr << "Could not open file: " << jsonFilePath << "\n";
    exit(1);
  }

  nlohmann::json j;
  try {
    file >> j;
  } catch (const nlohmann::json::exception &e) {
    std::cerr << "JSON parse error: " << e.what() << "\n";
    exit(1);
  }
  file.close();

  std::array<sunset, 25> sunsets;
  int index = 0;

  for (const auto &element : j) {
    if (index >= 25) {
      std::cerr << "JSON data has more than 25 elements. Truncating.\n";
      break;
    }

    sunset s;
    s.date = element["Date"].get<std::string>();
    s.lat = element["Lat"].get<double>();
    s.lng = element["Lng"].get<double>();
    s.guessHour = element["Hour"].get<int>();
    sunsets[index] = s;
    ++index;
  }

  if (index < 25) {
    std::cerr << "JSON data has less than 25 elements. Filling with default "
                 "values.\n";
    for (; index < 25; ++index) {
      sunset s = {"", 0.0, 0.0, 0};
      sunsets[index] = s;
    }
  }

  return sunsets;
}

static size_t WriteCallback(void *contents, size_t size, size_t nmemb,
                            void *userp) {
  ((std::string *)userp)->append((char *)contents, size * nmemb);
  return size * nmemb;
}

int findSunsetHour(sunset sunsetObj) {
  CURL *curl;
  CURLcode res;
  std::string readBuffer;

  curl_global_init(CURL_GLOBAL_DEFAULT);
  curl = curl_easy_init();
  if (curl) {
    std::string apiUrl = "https://api.sunrisesunset.io/json?lat=" +
                         std::to_string(sunsetObj.lat) +
                         "&lng=" + std::to_string(sunsetObj.lng) +
                         "&date=" + sunsetObj.date;

    curl_easy_setopt(curl, CURLOPT_URL, apiUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

    res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
      std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res)
                << std::endl;
      return -1;
    }

    curl_easy_cleanup(curl);
  }

  nlohmann::json jsonObject = nlohmann::json::parse(readBuffer);
  if (jsonObject.empty()) {
    return -1;
  }

  std::string sunsetTime = jsonObject["results"]["sunset"];
  if (sunsetTime.empty()) {
    return -1;
  }

  // Extracting the hour from the sunset time string "h:mm:ss AM/PM"
  std::string hourStr = sunsetTime.substr(0, sunsetTime.find(":"));
  int sunsetHour = std::stoi(hourStr);

  // Convert to 24-hour time if it's PM
  if (sunsetTime.find("PM") != std::string::npos && sunsetHour != 12) {
    sunsetHour += 12;
  }

  // Convert back to 12-hour time if it's 12 AM
  if (sunsetTime.find("AM") != std::string::npos && sunsetHour == 12) {
    sunsetHour = 0;
  }

  return sunsetHour;
}

void addGoodSunsets(sunset sunsetObj, sortedResultMonitor &newData, int &sumInt,
                    double &sumDouble) {
  int hour = findSunsetHour(sunsetObj);

  if (hour == sunsetObj.guessHour) {
    sunsetWithComputedValue sunsetResult;
    sunsetResult.sunsetObj = sunsetObj;
    sunsetResult.sunsetHour = hour;

    newData.addItemSorted(sunsetResult);

    sumInt += sunsetObj.guessHour;
    sumDouble += sunsetObj.lng;
  }
}

void printResults(sortedResultMonitor data) {
  ft_table_t *table = ft_create_table();
  /* Set "header" type for the first row */
  ft_set_cell_prop(table, 0, FT_ANY_COLUMN, FT_CPROP_ROW_TYPE, FT_ROW_HEADER);

  ft_write_ln(table, "Date", "Latitude", "Longitude", "Guess Hour",
              "Sunset Hour");

  for (int i = 0; i < data.getCount(); i++) {
    auto item = data.getItem(i);

    ft_write_ln(table, item.sunsetObj.date.c_str(),
                std::to_string(item.sunsetObj.lat).c_str(),
                std::to_string(item.sunsetObj.lng).c_str(),
                std::to_string(item.sunsetObj.guessHour).c_str(),
                std::to_string(item.sunsetHour).c_str());
  }

  printf("%s\n", ft_to_string(table));
  ft_destroy_table(table);
}

int main(int argc, char *argv[]) {
  string fileName = argv[1];
  string resultFile = "output/IFF1-1_BradauskasA_L1_rez.txt";

  int sunsetCount = 25;
  sortedResultMonitor results(100);

  array<sunset, 25> sunsets = readSunsets(fileName);

  // Calculate the number of elements per thread
  int numThreads = 6;
  int numElements = sunsetCount;
  int elementsPerThread = numElements / numThreads;
  int extraElements = numElements % numThreads;

  int sumInt = 0;
  double sumDouble = 0.0;

#pragma omp parallel num_threads(numThreads) default(none) shared(sunsets, results, elementsPerThread, extraElements) reduction(+:sumInt, sumDouble)
  {
    int threadId = omp_get_thread_num();
    int startIndex = threadId * elementsPerThread;
    int endIndex = startIndex + elementsPerThread;

    if (threadId < extraElements) {
      startIndex += threadId;
      endIndex += threadId + 1;
    } else {
      startIndex += extraElements;
      endIndex += extraElements;
    }

    printf("Worker thread %d: startIndex = %d, endIndex = %d, number of "
           "elements: %d\n",
           threadId, startIndex, endIndex, endIndex - startIndex);

    for (int i = startIndex; i < endIndex; i++) {
      addGoodSunsets(sunsets[i], results, sumInt, sumDouble);
    }
  }

  printResults(results);
    printf("%d \n", sumInt);
    printf("%f \n", sumDouble);

  return 0;
}
