#include "test.h"

int sample_test_func(ZabbixContext& zcontext) {
    // return 0 if test successfully, return 1 if test failed
    string epoch_num = getEpochTimeString();
    string func_name = __func__;
    if (BIND_SAMPLE_DATA.find(func_name) == BIND_SAMPLE_DATA.end()) {
        throw std::logic_error("There is no predifined sample test data for function " + func_name + "\n");
    }
    string sample_test_data_file = BIND_SAMPLE_DATA.at(func_name);
    YAML::Node root;
    try{
        root = loadYamlFile(sample_test_data_file);
    }
    catch(const std::exception& e){
        cout << "Error parsing sample test data file " << sample_test_data_file << endl;
        return 1;
    }

    /*
    Test code here
    */
    
    return 0;
}

int test_regexp_api(ZabbixContext& zcontext) {
    // return 0 if test successfully, return 1 if test failed
    string epoch_num = getEpochTimeString();
    string func_name = __func__;
    if (BIND_SAMPLE_DATA.find(func_name) == BIND_SAMPLE_DATA.end()) {
        throw std::logic_error("There is no predifined sample test data for function " + func_name + "\n");
    }
    string sample_test_data_file = BIND_SAMPLE_DATA.at(func_name);
    YAML::Node root;
    try{
        root = loadYamlFile(sample_test_data_file);
    }
    catch(const std::exception& e){
        cout << "Error parsing sample test data file " << sample_test_data_file << endl;
        return 1;
    }

    const YAML::Node& regexps = root["regexps"];
    vector<ZabbixRegex> vzr;
    for (const auto& regexp : regexps) {
        string name = regexp["name"].as<std::string>();
        name += "_" + epoch_num;
        ZabbixRegex zr = ZabbixRegex(name);
        const YAML::Node& expressions = regexp["expressions"];
        for (const auto& expression : expressions) {
            zr.expressions.push_back(parseJsonPair(expression));
        }
        vzr.push_back(zr);
    }

    cout << "Start test API for RegularExpression..." << endl;

    cout << "    Test API regexp.create...";
    for (ZabbixRegex zr : vzr) {
        if (createRegexp(zr, zcontext) != -1) {
            cout << "OK" << endl;
        } else {
            cout << "FAILED" << endl;
        }
    }

    return 0;
}