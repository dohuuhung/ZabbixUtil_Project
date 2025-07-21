#include "logger.h"
#include <fmt/core.h>
//#include <fmt/format.h>
#include "zabbix_util.h"
#include "test.h"
#include "CLI/CLI.hpp"
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

void handle_test(string zone_name) {
	ZabbixContext zc = findZC(zcvector, zone_name);
	userLogin(zc);

    test_regexp_api(zc);
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

int main(int argc, char** argv) {
    try {
        zcvector = parseZabbixUtilConf(DEFAULT_CFG_FILE);
    } catch (const std::exception& e) {
        cerr << "Error parsing ZabbixUtil configuration: " << DEFAULT_CFG_FILE << endl << e.what() << endl;
        return 1;
    }
	
    CLI::App app{"Zabbix Utility Tool\n\nUse 'ZabbixUtil.exe <subcommand> -h' to see details for a specific subcommand."};

    string zone_name;
    // === test ===
    auto test_cmd = app.add_subcommand("test", "Run test function");
    test_cmd->add_option("<zone_name>", zone_name, "Monitoring zone name")
        ->required() ->type_name("");
    test_cmd->callback([&]() {
        handle_test(zone_name);
    });

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
    create_template_cmd -> footer("E.x: ZabbixUtil.exe create_template PSKRW1 default_template.txt");

    // === modify_metric ===
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
	                               "  ZabbixUtil.exe update_mntr_cfg -l host_list01.txt PSKRW1 modify01.txt\n"
	                               "  ZabbixUtil.exe update_mntr_cfg --host server01 --ip 127.0.0.1 PSKRW1 modify01.txt\n");

    CLI11_PARSE(app, argc, argv);
    return 0;
}