#ifndef TEST_H
#define TEST_H

#include "zabbix_util.h"
#include <iostream>
#include <string>
#include <unordered_map>
#include <stdexcept>
#include <vector>
using namespace std;

const unordered_map<string, string> BIND_SAMPLE_DATA = {
    {"test_regexp_api", "test_data\\sample_regex.yaml"}
};

int test_regexp_api(ZabbixContext& zcontext);

#endif