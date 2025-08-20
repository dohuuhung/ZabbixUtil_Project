#ifndef TEST_H
#define TEST_H

#include "zabbix_util.h"
#include "md5.h"
#include <iostream>
#include <string>
#include <unordered_map>
#include <stdexcept>
#include <vector>
using namespace std;

const unordered_map<string, string> BIND_SAMPLE_DATA = {
    {"test_zabbixhost_api", "test_data\\sample_host.yaml"},
    {"test_regexp_api", "test_data\\sample_regex.yaml"},
    {"common_test_func", "test_data\\sample_log_file_mntr.yaml"},
    {"md5_test_func", "test_data\\md5_test_data.yaml"},
    {"test_createLogFileMntr", "test_data\\sample_log_file_mntr.yaml"}
};

int test_zabbixhost_api(ZabbixContext& zcontext);
int test_regexp_api(ZabbixContext& zcontext);
int md5_test_func(ZabbixContext& zcontext);
int test_createLogFileMntr(ZabbixContext& zcontext);

const unordered_map<string, std::function<int(ZabbixContext&)>> TEST_FUNCT_MAP = {
    {"test_zabbixhost_api", test_zabbixhost_api},
    {"test_regexp_api", test_regexp_api},
    {"md5_test_func", md5_test_func},
    {"test_createLogFileMntr", test_createLogFileMntr}
};

#endif