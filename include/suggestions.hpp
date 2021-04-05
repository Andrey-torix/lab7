// Copyright 2020 Andreytorix
#pragma once
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <boost/algorithm/string.hpp>
struct suggestion {
  std::string id;
  std::string name;
  int cost;
  static bool compare(suggestion &sug1, suggestion &sug2) {
    return (sug1.cost > sug2.cost);
  }
};
std::vector<suggestion> getFromJson(std::string path) {
  std::vector<suggestion> v;
  std::ifstream i(path);
  nlohmann::json j;
  i >> j;

  for (nlohmann::json::iterator it = j.begin(); it != j.end(); ++it) {
    nlohmann::json sug = it.value();
    suggestion temp;
    temp.id = sug["id"];
    temp.name = sug["name"];
    temp.cost = static_cast<int>(sug["cost"]);
    v.push_back(temp);
  }

  std::sort(v.begin(), v.end(), suggestion::compare);
  return v;
}

bool search(std::string item, std::string searchText) {
  return (boost::contains(item, searchText));
}

std::vector<suggestion> getFromJson(std::string path);
bool search(std::string item, std::string searchText);
