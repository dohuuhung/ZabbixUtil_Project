#include "logger.h"
#include <fmt/core.h>
#include "zabbix_util.h"
#include "test.h"
#include "CLI/CLI.hpp"
#include <windows.h>
#include <iostream>
#include <stdexcept>
#include <array>
#include <memory>
#include <string>
#include <vector>
using namespace std;

string DEFAULT_CFG_FILE = "config\\zabbix_util.yaml";
vector<ZabbixContext> zcvector;

ZabbixContext findZC(vector<ZabbixContext>& zcv, string zone_name) {
	for (auto zc : zcv) {
		if (zc.get_zone_name() == zone_name) return zc;
	}
	return ZabbixContext("Na");
}

void handle_test(string zone_name, string test_funct, bool list_mode) {
	ZabbixContext zc = findZC(zcvector, zone_name);
	userLogin(zc);

    if (list_mode) {
        cout << "List of test function name:" << endl;
        for (auto it : TEST_FUNCT_MAP) {
            cout << "    " << it.first << endl;
        }
        return;
    }

    if (!test_funct.empty()) {
        if (TEST_FUNCT_MAP.find(test_funct) == TEST_FUNCT_MAP.end()) {
            cout << "Test function " << test_funct << " hasn't been defined before." << endl;
            cout << "Stop job." << endl;
            return;
        }

        cout << "Testing function " << test_funct << "..." << endl;
        if (TEST_FUNCT_MAP.at(test_funct)(zc) == 0) {
            cout << "OK" << endl;
        } else {
            cout << "FAIL" << endl;
        }
        return;
    }

    vector<string> fail_funct_list;
    for (auto it : TEST_FUNCT_MAP) {
        string test_funct_name = it.first;
        cout << "Testing function " << test_funct_name << "..." << endl;
        if (it.second(zc) == 0) {
            cout << "OK" << endl;
        } else {
            cout << "FAIL" << endl;
            fail_funct_list.push_back(test_funct_name);
        }
    }
    if (fail_funct_list.size() > 0) {
        cout << "List of failed functions:" << endl;
        for (string fail_funct : fail_funct_list) {
            cout << "    " << fail_funct << endl;
        }
    }
}

void handle_create_template(const string& zone_name, const string& template_input) {
    cout << "[" << zone_name << "] Creating template from: " << template_input << "\n";
    ZabbixTemplate ztmpl = ZabbixTemplate("NA");
    try {
        ztmpl = parseZabbixTemplate(template_input);
    } catch (const std::exception& e) {
        cerr << "Error parsing input template file: " << template_input << endl << e.what() << endl;
        return;
    }
    ZabbixContext zc = findZC(zcvector, zone_name);
	userLogin(zc);

    cout << "Start validate template..." << endl;
    if (validateZabbixTemplate(ztmpl, zc) == 0) {
        cout << "Template is valid" << endl;
    } else {
        cout << "Template is invalid." << endl << "Please check template content again." << endl << "Stop job" << endl;
        return;
    }
	
	int template_id = createTemplate(ztmpl.template_name, zc);	
	if (template_id == -1) {
		cout << "Create template " << ztmpl.template_name << " failed!" << endl;
		return;
	}
	cout << "Empty template with name " << ztmpl.template_name << " is created." << endl;
	
	cout << "Continue to create items for template:" << endl;
	for (auto item : ztmpl.zitems) {
		int item_id = createItem(item, template_id, ztmpl.monitoring_type, zc);
		if (item_id == -1) {
			cout << "    Create item '" << item.name << "' failed!" << endl;
		} else {
			cout << "    Create item '" << item.name << "' successfully! ItemID=" << item_id << endl;
		}
	}
	
	cout << "Continue to create triggers for template:" << endl;
    for (auto event : ztmpl.zevents) {
        for (auto trigger : event.ztv) {
            int trigger_id = createTrigger(trigger, zc);
            if (trigger_id == -1) {
                cout << "    Create trigger '" << trigger.description << "' failed!" << endl;
            } else {
                cout << "    Create trigger '" << trigger.description << "' successfully! TriggerID=" << trigger_id << endl;
            }
        }
    }
}

void handle_update_mntr_conf(string zone_name, const string& update_file,
                             const string& host_list_file="",
                             const string& hostname="", const string& ip="") {
    ZabbixContext zc = findZC(zcvector, zone_name);
	userLogin(zc);
	
	vector<ZabbixHost> zhv;
    if (!hostname.empty()) {
    	ZabbixHost zh = ZabbixHost(hostname);
        zh.host_conf["ip"] = ip;
        zhv.push_back(zh);
	}
     
    if (!host_list_file.empty()) {
    	zhv = parseHostList(host_list_file);
	}

    // Validate host or host list
    cout << "Validate host list..." << endl;
    vector<ZabbixHost> valid_zhv;
    try {
        valid_zhv = validateHostList(zhv, zc);
    } catch (const std::exception& e) {
        cerr << e.what() << "Please check your host list again" << endl;
        return;
    }
    cout << "OK" << endl;

    pair<int, pair<vector<ZabbixItem>, vector<ZabbixEvent>>> puc;
    try {
        puc = parseUpdateConf(update_file);
    } catch (const std::exception& e) {
        cerr << "Error parsing input update file: " << update_file << endl << e.what() << endl;
        return;
    }
    int update_mode = puc.first;
    pair<vector<ZabbixItem>, vector<ZabbixEvent>> update_pair = puc.second;
    vector<ZabbixItem> update_ziv = update_pair.first;
    vector<ZabbixEvent> update_zev = update_pair.second;

    // Validate update items
    cout << "Validating updated items..." << endl;
    try {
        validateItemList(update_ziv, zc);
    } catch (const std::exception& e) {
        cerr << "Error validating update items." << endl << e.what() << endl;
        return;
    }
    cout << "OK" << endl;

    // Validate update events
    cout << "Validating updated events..." << endl;
    try {
        validateEventList(update_ziv, update_zev, zc);
    } catch (const std::exception& e) {
        cerr << "Error validating update events." << endl << e.what() << endl;
        return;
    }
    cout << "OK" << endl;

    // Start update monitoring config
    cout << "Start update monitoring config for " << valid_zhv.size() << " hosts" << endl;
    for (ZabbixHost valid_zh : valid_zhv) {
    	int update_result = update_mntr_conf(update_mode, valid_zh, update_ziv, update_zev, zc);
    	if (update_result == 0) {
    		cout << "Update monitoring config for " << valid_zh.host_name << " successfull." << endl;
		} else {
			cout << "Some update jobs fail for " << valid_zh.host_name << ", please check zabbix_util.log for more details." << endl;
		}
	}
}

void handle_log_mntr(string zone_name, const string& action,
                     const string& log_mntr_file,
                     const string& host_list_file="",
                     const string& hostname="", const string& ip="") {
    ZabbixContext zc = findZC(zcvector, zone_name);
	userLogin(zc);
	
	vector<ZabbixHost> zhv;
    if (!hostname.empty()) {
    	ZabbixHost zh = ZabbixHost(hostname);
        zh.host_conf["ip"] = ip;
        zhv.push_back(zh);
	}
     
    if (!host_list_file.empty()) {
    	zhv = parseHostList(host_list_file);
	}

    // Validate host or host list
    cout << "Validate host list..." << endl;
    vector<ZabbixHost> valid_zhv;
    try {
        valid_zhv = validateHostList(zhv, zc);
    } catch (const std::exception& e) {
        cerr << e.what() << "Please check your host list again" << endl;
        return;
    }
    cout << "OK" << endl;

    ZabbixLogFileMntr zlfm = ZabbixLogFileMntr("NA");
    try {
        zlfm = parseLogFileMntrYaml(log_mntr_file);
    } catch (const std::exception& e) {
        cerr << e.what() << endl;
        return;
    }

    if (action == "create") {
        for (ZabbixHost zh : valid_zhv) {
            cout << "Start create mew monitoring setting for log file"
                 << zlfm.log_file_path << " of server "
                 << zh.host_name << "..." << endl;
            if (createLogFileMntr(zh, zlfm, zc) == 1) {
                cout << "FAILED" << endl;
            } else {
                cout << "SUCCESS" << endl;
            }
        }
    } else if (action == "overwrite_patterns") {
        cout << "TODO" << endl;
    } else if (action == "add_patterns") {
        cout << "TODO" << endl;
    } else if (action == "remove_patterns") {
        cout << "TODO" << endl;
    } else if (action == "replace_patterns") {
        cout << "TODO" << endl;
    }
}

int main(int argc, char** argv) {
    DEFAULT_CFG_FILE = getExecutableDirectory() + "\\" + DEFAULT_CFG_FILE;
    try {
        zcvector = parseZabbixUtilConf(DEFAULT_CFG_FILE);
    } catch (const std::exception& e) {
        cerr << "Error parsing ZabbixUtil configuration: " << DEFAULT_CFG_FILE << endl << e.what() << endl;
        return 1;
    }
	
    CLI::App app{"Zabbix Utility Tool\n\nUse 'ZabbixUtil.exe <subcommand> -h' to see details for a specific subcommand."};

    string zone_name, test_funct;
    bool list_mode = false;
    test_funct = "";
    // === test ===
    auto test_cmd = app.add_subcommand("test", "Run test function");
    test_cmd->add_option("<zone_name>", zone_name, "Monitoring zone name")
        ->required() ->type_name("");
    test_cmd->add_option("-f", test_funct, "Test function name") ->type_name("");
    test_cmd->add_flag("-l", list_mode, "List all supported test function names.") ->type_name("");
    test_cmd->callback([&]() {
        if (!test_funct.empty() && list_mode) {
            throw CLI::RequiredError("Options -f and -l mustn't exist together.");
        }
        handle_test(zone_name, test_funct, list_mode);
    });
    test_cmd -> footer("E.x:\n"
                       "  zabbix-util test PSKRW1\n"
                       "  zabbix-util test -f test_regexp_api PSKRW1\n"
                       "  zabbix-util test -l PSKRW1\n");

    // === create_template ===
    string template_input;
    auto create_template_cmd = app.add_subcommand("create_template", "Create template from input file");
    create_template_cmd->add_option("<zone_name>", zone_name, "Monitoring zone name")
        ->required() ->type_name("");
    create_template_cmd->add_option("<input_template_file>", template_input, "Input file path")
        ->required() ->type_name("");
    create_template_cmd->callback([&]() {
        handle_create_template(zone_name, template_input);
    });
    create_template_cmd -> footer("E.x: zabbix-util create_template PSKRW1 default_template.txt");

    // === update_mntr_cfg ===
    string update_file, update_hostlist, update_hostname, ip_addr;
    auto update_mntr_conf_cmd = app.add_subcommand("update_mntr_cfg", "Update monitoring config from input file");
    update_mntr_conf_cmd->add_option("<zone_name>", zone_name, "Monitoring zone name")
        ->required() ->type_name("");
    update_mntr_conf_cmd->add_option("<update_file>", update_file, "Input file path")
        ->required() ->type_name("");
    update_mntr_conf_cmd->add_option("-l", update_hostlist, "Host list file path") ->type_name("");
    update_mntr_conf_cmd->add_option("--host", update_hostname, "Single hostname") ->type_name("");
    update_mntr_conf_cmd->add_option("--ip", ip_addr, "IP address of host determined in --host") ->type_name("");

    update_mntr_conf_cmd->callback([&]() {
        if (!update_hostname.empty() && !update_hostlist.empty()) {
            throw CLI::RequiredError("Only one option is used in this case, --host or -l\nThey are not allowed to exist at the same time.");
        }
        if (update_hostname.empty() && update_hostlist.empty()) {
        	throw CLI::RequiredError("--host or -l must be provided");
		}
		if (!update_hostname.empty()) {
			if (ip_addr.empty()) {
				throw CLI::RequiredError("If option --host is used, option --ip must be provided");
			}
		}
        handle_update_mntr_conf(zone_name, update_file, update_hostlist, update_hostname, ip_addr);
    });
    update_mntr_conf_cmd -> footer("E.x:\n"
	                               "  zabbix-util update_mntr_cfg -l host_list01.txt PSKRW1 modify01.yaml\n"
	                               "  zabbix-util update_mntr_cfg --host server01 --ip 127.0.0.1 PSKRW1 modify01.yaml\n");

    // === log_mntr ===
    string log_mntr_file, action;
    set<string> action_set = {"create", "overwrite_patterns", "add_patterns",
                              "remove_patterns", "replace_patterns"};
    auto log_mntr_cmd = app.add_subcommand("log_mntr", "Update log file monitoring from input file");
    log_mntr_cmd->add_option("<zone_name>", zone_name, "Monitoring zone name")
        ->required() ->type_name("");
    log_mntr_cmd->add_option("<action>", action, "Supported action:\n"
                                                 "  create: create new log montioring setting\n"
                                                 "  overwrite_patterns: overwrite old log montioring patterns\n"
                                                 "  add_patterns: add new patterns to current log monitoring setting\n"
                                                 "  remove_patterns: remove sepcified existing patterns\n"
                                                 "  replace_patterns: replace patterns")
        ->required() ->type_name("");
    log_mntr_cmd->add_option("<log_mntr_file>", log_mntr_file, "Input file path")
        ->required() ->type_name("");
    log_mntr_cmd->add_option("-l", update_hostlist, "Host list file path") ->type_name("");
    log_mntr_cmd->add_option("--host", update_hostname, "Single hostname") ->type_name("");
    log_mntr_cmd->add_option("--ip", ip_addr, "IP address of host determined in --host") ->type_name("");

    log_mntr_cmd->callback([&]() {
        if (!update_hostname.empty() && !update_hostlist.empty()) {
            throw CLI::RequiredError("Only one option is used in this case, --host or -l\nThey are not allowed to exist at the same time.");
        }
        if (update_hostname.empty() && update_hostlist.empty()) {
        	throw CLI::RequiredError("--host or -l must be provided");
		}
		if (!update_hostname.empty()) {
			if (ip_addr.empty()) {
				throw CLI::RequiredError("If option --host is used, option --ip must be provided");
			}
		}
        if (action_set.find(action) == action_set.end()) {
            throw CLI::ValidationError("Entered action value is not supported");
        }
        handle_log_mntr(zone_name, action, log_mntr_file, update_hostlist, update_hostname, ip_addr);
    });
    log_mntr_cmd -> footer("E.x:\n"
	                       "  zabbix-util log_mntr -l host_list01.txt PSKRW1 add_patterns log_mntr.yaml\n"
	                       "  zabbix-util log_mntr --host server01 --ip 10.1.1.1 PSKRW1 overwrite log_mntr.yaml\n");

    CLI11_PARSE(app, argc, argv);
    return 0;
}