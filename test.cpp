#include "test.h"

int sample_test_func(ZabbixContext& zcontext) {
    // return 0 if test successfully, return 1 if test failed
    string epoch_num = getEpochTimeString();
    string func_name = __func__;
    if (BIND_SAMPLE_DATA.find(func_name) == BIND_SAMPLE_DATA.end()) {
        throw std::logic_error("There is no predifined sample test data for function " + func_name + "\n");
    }
    string sample_test_data_file = getExecutableDirectory() + "\\" + BIND_SAMPLE_DATA.at(func_name);
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
    string sample_test_data_file = getExecutableDirectory() + "\\" + BIND_SAMPLE_DATA.at(func_name);
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
    vector<ZabbixRegex> v_updatezr;
    for (const auto& regexp : regexps) {
        string name = regexp["name"].as<std::string>();
        name += "_" + epoch_num;

        ZabbixRegex zr = ZabbixRegex(name);
        const YAML::Node& expressions = regexp["expressions"];
        for (const auto& expression : expressions) {
            zr.expressions.push_back(parseJsonPair(expression));
        }
        vzr.push_back(zr);

        ZabbixRegex update_zr = ZabbixRegex(name);
        const YAML::Node& update_expressions = regexp["test_update_expressions"];
        for (const auto& expression : update_expressions) {
            update_zr.expressions.push_back(parseJsonPair(expression));
        }
        v_updatezr.push_back(update_zr);
    }

    cout << "    Test API regexp.create...";
    for (ZabbixRegex& zr : vzr) {
        int create_result = createRegexp(zr, zcontext);
        if (create_result != -1) {
            cout << "OK" << endl;
            zr.regexpid = to_string(create_result);
            for (ZabbixRegex& update_zr : v_updatezr) {
                if (update_zr.name == zr.name) {
                    update_zr.regexpid = zr.regexpid;
                    break;
                }
            }
        } else {
            cout << "FAILED" << endl;
            return 1;
        }
    }

    cout << "    Test API regexp.get...";
    for (ZabbixRegex zr : vzr) {
        ZabbixRegex _zr = getRegexp(zcontext, zr.name);
        if ((zr.name != _zr.name) or (zr.regexpid != _zr.regexpid)) {
            cout << "FAILED" << endl;
            return 1;
        }

        if (zr.expressions.size() != _zr.expressions.size()) {
            cout << "FAILED" << endl;
            return 1;
        }

        for (auto expression : zr.expressions) {
            bool same = false;
            for (auto _expression : _zr.expressions) {
                if (expression == _expression) {
                    same = true;
                    break;
                }
            }
            if (same) continue;
            cout << "FAILED" << endl;
            return 1;
        }
        cout << "OK" << endl;
    }

    cout << "    Test API regexp.update...";
    for (ZabbixRegex update_zr : v_updatezr) {
        int update_result = updateRegexp(update_zr, zcontext);
        if (update_result == -1) {
            cout << "FAILED" << endl;
            return 1;
        }

        ZabbixRegex _zr = getRegexp(zcontext, update_zr.name);
        if ((update_zr.name != _zr.name) or (update_zr.regexpid != _zr.regexpid)) {
            cout << "FAILED" << endl;
            return 1;
        }

        if (update_zr.expressions.size() != _zr.expressions.size()) {
            cout << "FAILED" << endl;
            return 1;
        }

        for (auto expression : update_zr.expressions) {
            bool same = false;
            for (auto _expression : _zr.expressions) {
                if (expression == _expression) {
                    same = true;
                    break;
                }
            }
            if (same) continue;
            cout << "FAILED" << endl;
            return 1;
        }
        cout << "OK" << endl;
    }

    cout << "    Test API regexp.delete...";
    for (ZabbixRegex zr : vzr) {
        if (deleteRegexp(zr, zcontext) == 0) {
            cout << "OK" << endl;
        } else {
            cout << "FAILED" << endl;
            return 1;
        }
    }

    return 0;
}