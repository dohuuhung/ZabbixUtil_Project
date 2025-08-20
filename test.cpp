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
        ZabbixRegex _zr = getRegexp(zcontext, zr.name)[0];
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

        ZabbixRegex _zr = getRegexp(zcontext, update_zr.name)[0];
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

int test_func_parseLogFileMntrYaml(ZabbixContext& zcontext) {
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

    ZabbixLogFileMntr zlfm = parseLogFileMntrYaml(sample_test_data_file);
    cout << "log_file_path: " << zlfm.log_file_path << endl;
    cout << "event_name: " << zlfm.event_name << endl;
    cout << endl << "levels:" << endl;
    for (auto level : zlfm.levels) {
        cout << "  - priority: " << level.first << endl;
        cout << "    match_patterns:" << endl;
        for (auto re : level.second.expressions) {
            if (re.find("expression_type") != re.end() && re.at("expression_type") == "3") {
                cout << "      - \"" << re.at("expression") << "\"" << endl;
            }
        }
        cout << "    skip_patterns:" << endl;
        for (auto re : level.second.expressions) {
            if (re.find("expression_type") != re.end() && re.at("expression_type") == "4") {
                cout << "      - \"" << re.at("expression") << "\"" << endl;
            }
        }
    }
    
    return 0;
}

int md5_test_func(ZabbixContext& zcontext) {
    // return 0 if test successfully, return 1 if test failed
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

    try {
        vector<pair<int, string>> keys;
        keys.push_back(make_pair(0, "input"));
        keys.push_back(make_pair(0, "full_md5"));
        keys.push_back(make_pair(0, "last8"));
        check_exist_yaml_key(root, keys, sample_test_data_file);
    } catch (const std::exception& e) {
        cout << e.what() << endl;
        return 1;
    }

    string input = root["input"].as<string>();
    string full_md5 = root["full_md5"].as<string>();
    string last8 = root["last8"].as<string>();

    cout << "    Test function md5()...";
    if (md5(input) != full_md5) {
        cout << "FAILED" << endl;
    } else {
        cout << "OK" << endl;
    }
    cout << "    Test function md5Last8()...";
    if (md5Last8(input) != last8) {
        cout << "FAILED" << endl;
    } else {
        cout << "OK" << endl;
    }
    
    return 0;
}

int test_createLogFileMntr(ZabbixContext& zcontext) {
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

    string default_group_name = zcontext.get_mntrcfg("default_host_group");
	int default_group_id = getHostGroup(default_group_name, zcontext).first;
	if (default_group_id == -1) {
        int API_RETRY_TIME = 3;
		for (int r = 1; r <= API_RETRY_TIME; r++) {
			default_group_id = createHostGroup(default_group_name, zcontext);
			if (default_group_id > 0) break;
			if (default_group_id == -1 && r == API_RETRY_TIME) {
				return -1;
			}
		}
	}

    string test_host_name = "zabbix_test_host_" + getEpochTimeString();
    ZabbixHost zh = ZabbixHost(test_host_name);
    zh.host_conf["groupid"] = to_string(default_group_id);
    zh.host_id = to_string(createHost(zh, zcontext));
    if (zh.host_id == "-1") return 1;
    ZabbixLogFileMntr zlfm = parseLogFileMntrYaml(sample_test_data_file);

    createLogFileMntr(zh, zlfm, zcontext);

    return 0;
}

int test_zabbixhost_api(ZabbixContext& zcontext) {
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

    try {
        vector<pair<int, string>> keys;
        keys.push_back(make_pair(1, "hosts"));
        check_exist_yaml_key(root, keys, sample_test_data_file);
        keys.clear();
        keys.push_back(make_pair(0, "name"));
        for (auto host : root["hosts"]) {
            check_exist_yaml_key(host, keys, sample_test_data_file);
        }
    } catch (const std::exception& e) {
        cout << e.what() << endl;
        return 1;
    }

    string default_group_name = zcontext.get_mntrcfg("default_host_group");
	int default_group_id = getHostGroup(default_group_name, zcontext).first;
	if (default_group_id == -1) {
        int API_RETRY_TIME = 3;
		for (int r = 1; r <= API_RETRY_TIME; r++) {
			default_group_id = createHostGroup(default_group_name, zcontext);
			if (default_group_id > 0) break;
			if (default_group_id == -1 && r == API_RETRY_TIME) {
				return -1;
			}
		}
	}

    vector<ZabbixHost> zhv;
    for (auto host : root["hosts"]) {
        string hostname = host["name"].as<string>() + "_" + epoch_num;
        ZabbixHost zh = ZabbixHost(hostname);
        zh.host_conf["groupid"] = to_string(default_group_id);
        zhv.push_back(zh);
    }

    // Test API host.create & host.get
    cout << "    Test API host.create & host.get...";
    for (ZabbixHost& zh : zhv) {
        int host_id = createHost(zh, zcontext);
        if (host_id == -1) {
            cout << "    Create host=" << zh.host_name << " failed" << endl;
            return 1;
        }
        zh.host_id = to_string(host_id);
    }

    for (ZabbixHost zh : zhv) {
        vector<ZabbixHost> get_zhv = getHost(zh.host_name, zcontext);
        bool exist = false;
        for (ZabbixHost get_zh : get_zhv) {
            if (get_zh.host_name == zh.host_name && get_zh.host_id == zh.host_id) exist = true;
        }
        if (!exist) {
            cout << "Get host=" << zh.host_name << " failed" << endl;
            return 1;
        }
    }
    cout << "OK" << endl;

    // Test API host.delete
    cout << "    Test API host.delete...";
    for (ZabbixHost zh : zhv) {
        int delete_result = deleteHost(zh, zcontext);
        if (delete_result == 1) {
            cout << "Delete host=" << zh.host_name << " failed" << endl;
            return 1;
        }
    }
    for (ZabbixHost zh : zhv) {
        vector<ZabbixHost> get_zhv = getHost(zh.host_name, zcontext);
        bool exist = false;
        for (ZabbixHost get_zh : get_zhv) {
            if (get_zh.host_id == zh.host_id) exist = true;
        }
        if (exist) {
            cout << "Delete host=" << zh.host_name << " failed" << endl;
            return 1;
        }
    }
    
    return 0;
}