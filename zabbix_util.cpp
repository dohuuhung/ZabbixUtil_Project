#include "logger.h"
#include "zabbix_util.h"
#include <yaml-cpp/yaml.h>
#include <fmt/core.h>
//#include <fmt/format.h>
#include <curl/curl.h>
#include "json.hpp"
#include <iostream>
#include <stdexcept>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>

using namespace std;
using json = nlohmann::json;

const int API_RETRY_TIME = 2;
const string SUPPORT_ITEM_KEY_FILE = "config\\zabbix_supported_item_key.txt";
typedef unordered_map<string, string> zinterface;

typedef pair<string, string> Credentials;

const unordered_map<string, int> ZITEM_VALUE_TYPE_MAP = {
	// second component (int):
	// 0: string type
	// 1: none string type
	{"itemid", 0},
	{"name", 0},
	{"key_", 0},
	{"type", 1},
	{"delay", 0},
	{"status", 1},
	{"value_type", 1},
	{"units", 0}
};

const unordered_map<string, int> ZTRIGGER_VALUE_TYPE_MAP = {
	// second component (int):
	// 0: string type
	// 1: none string type
	{"triggerid", 0},
	{"desc", 0},
	{"description", 0},
	{"event_name", 0},
	{"expression", 0},
	{"priority", 1},
	{"status", 1},
	{"type", 1}
};

const unordered_map<string, int> REGEX_EXPRESSION_VALUE_TYPE_MAP = {
	// second component (int):
	// 0: string type
	// 1: none string type
	{"name", 0},
	{"test_string", 0},
	{"expressions", 1},
	{"expression", 0},
	{"expression_type", 0},
	{"case_sensitive", 0}
};

string getEpochTimeString() {
    auto now = std::chrono::system_clock::now();
    auto epoch_seconds = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()
    ).count();

    return to_string(epoch_seconds);
}

string makeKeyValueStr(string k, string v, int value_type) {
	// value_type:
	// 0: string type
	// 1: none string type
	string result;
	if (value_type == 0) {
		result = R"("{}": "{}")";
	} else if (value_type == 1) {
		result = R"("{}": {})";
	}
	result = fmt::format(result, k, v);
	return result;
}

unordered_map<string, string> parseJsonPair(const YAML::Node& jp) {
	unordered_map<string, string> result;
	for (const auto& kv : jp) {
        string k = kv.first.as<std::string>();
        string v = kv.second.as<std::string>();
        if (v == "null") continue;
        result[k] = v;
    }
    return result;
}

YAML::Node loadYamlFile(const string& file_path) {
    try {
        YAML::Node root = YAML::LoadFile(file_path);
        return root;
    } catch (const YAML::BadFile& e) {
        throw;
    } catch (const YAML::Exception& e) {
        throw;
    }
}

ZabbixContext::ZabbixContext(string zn) {
	this->zone_name = zn;
}

ZabbixContext::ZabbixContext(string zn, string h, int p, bool s, Credentials c) {
	this->zone_name = zn;
	this->host = h;
	this->port = p;
	this->ssl = s;
	this->credential = c;
}

void ZabbixContext::update_mntr_cfg(string k, string v) {
	this->mntr_cfg[k] = v;
}

void ZabbixContext::update_host(string h) {
	this->host = h;
}

void ZabbixContext::update_port(int p) {
	this->port = p;
}

void ZabbixContext::update_auth_token(string auth) {
	this->auth_token = auth;
}

string ZabbixContext::get_zone_name() {
	return this->zone_name;
}

string ZabbixContext::get_host() {
	return this->host;
}

int ZabbixContext::get_port() {
	return this->port;
}

bool ZabbixContext::get_ssl() {
	return this->ssl;
}

Credentials ZabbixContext::get_credentials() {
	return this->credential;
}

string ZabbixContext::get_auth_token() {
	return this->auth_token;
}

string ZabbixContext::get_mntrcfg(string k) {
	return this->mntr_cfg[k];
}

ZabbixHost::ZabbixHost(string hn) {
	this->host_name = hn;
}

ZabbixHost::ZabbixHost(string hn, string hi) {
	this->host_name = hn;
	this->host_id = hi;
}

ZabbixItem::ZabbixItem(string n) {
    this->name = n;
}

ZabbixTrigger::ZabbixTrigger(string desc) {
    this->description = desc;
}

ZabbixEvent::ZabbixEvent(string desc, string en) {
    this->description = desc;
    this->event_name = en;
}

ZabbixTemplate::ZabbixTemplate(string temp_name) {
	this->template_name = temp_name;
}

ZabbixTemplate parseZabbixTemplate(string template_file) {
    YAML::Node root;
    try{
        root = loadYamlFile(template_file);
    }
    catch(const std::exception& e){
        throw;
    }

    string template_name = root["template_name"].as<std::string>();
    ZabbixTemplate zt = ZabbixTemplate(template_name);
    if (!root["monitoring_type"]) {
    	zt.monitoring_type = 7;
	} else {
		zt.monitoring_type = root["monitoring_type"].as<int>();
	}

    // parse items
    if (root["template_content"]["items"]) {
    	const YAML::Node& items = root["template_content"]["items"];

	    for (const auto& item : items) {
	        string name = item["name"].as<std::string>();
	
	        ZabbixItem zi = ZabbixItem(name);
            zi.item_conf = parseJsonPair(item);
            zi.item_conf.erase("name");
			if (zi.item_conf.find("type") == zi.item_conf.end()) {
				zi.item_conf["type"] = to_string(zt.monitoring_type);
			}
	        zt.zitems.push_back(zi);
	    }
	}

    // parse events
    if (root["template_content"]["events"]) {
    	const YAML::Node& events = root["template_content"]["events"];
    	
    	for (const auto& event : events) {
    		string description = event["description"].as<std::string>();
    		string event_name = event["event_name"].as<std::string>();
            ZabbixEvent ze = ZabbixEvent(description, event_name);

            const YAML::Node& triggers = event["triggers"];
            for (const auto& trigger : triggers) {
                ZabbixTrigger ztg = ZabbixTrigger(description);
                ztg.trigger_conf = parseJsonPair(trigger);
                ztg.trigger_conf["event_name"] = event_name;
                ztg.trigger_conf["expression"] = fmt::format(ztg.trigger_conf["expression"], template_name);
                ze.ztv.push_back(ztg);
            }

    		zt.zevents.push_back(ze);
		}
	}
    
    return zt;
}

vector<ZabbixHost> parseHostList(string host_list_file) {
	vector<ZabbixHost> hosts;
	ifstream infile(host_list_file);
	string line;
    
    while (getline(infile, line)) {
        istringstream ss(line);
        string name, ip;

        if (getline(ss, name, '|') && getline(ss, ip)) {
        	if (!name.empty() && !ip.empty()) {
        		ZabbixHost host = ZabbixHost(name);
                host.host_conf["ip"] = ip;
	            hosts.push_back(host);
			}
        }
    }
    return hosts;
}

vector<ZabbixContext> parseZabbixUtilConf(string conf_file) {
    vector<ZabbixContext> zcvector;
    YAML::Node root;
    try{
        root = loadYamlFile(conf_file);
    }
    catch(const std::exception& e){
        throw;
    }
    
    string default_host_group = "";
    string log_file = "";
    if (root["default_config"]) {
    	default_host_group = root["default_config"]["default_host_group"].as<std::string>();
    	log_file = root["default_config"]["log_file"].as<std::string>();
	}
	
	if (root["zabbix_masters"]) {
		const YAML::Node& zabbix_masters = root["zabbix_masters"];
		for (const auto& master : zabbix_masters) {
			string zone_name = master["zone_name"].as<std::string>();
			string host = master["host"].as<std::string>();
			int port = master["port"].as<int>();
			bool ssl = master["ssl"].as<bool>();
			string user = master["user"].as<std::string>();
			string password = master["password"].as<std::string>();
			
			ZabbixContext zc = ZabbixContext(zone_name, host, port, ssl, make_pair(user, password));
			zc.update_mntr_cfg("default_host_group", default_host_group);
			zc.common_cfg["log_file"] = log_file;

			std::ifstream infile(SUPPORT_ITEM_KEY_FILE);
            std::string line;
			if (!infile) {
				cerr << "Can not open file: " << SUPPORT_ITEM_KEY_FILE << endl;
			} else {
				while (getline(infile, line)) {
					if (!line.empty()) {
						if (zc.supported_item_key.find(line) == zc.supported_item_key.end()) {
							line.erase(std::remove(line.begin(), line.end(), ' '), line.end());
							zc.supported_item_key[line] = "1";
						}
					}
				}
			}

			zcvector.push_back(zc);
		}
	} else {
		zcvector.push_back(ZabbixContext("Na", "Na", -1, false, make_pair("Na", "Na")));
	}
	
	return zcvector;
}

// Callback for receive response from server
size_t WriteCallback(void* contents, size_t size, size_t nmemb, string* output) {
    size_t totalSize = size * nmemb;
    output->append((char*)contents, totalSize);
    return totalSize;
}

string callZabbixAPI(ZabbixContext& zcontext, string jsonData) {
	string host = zcontext.get_host();
	string port = to_string(zcontext.get_port());
	string protocol = "http";
	if (zcontext.get_ssl()) protocol = "https";
	
    CURL* curl;
    CURLcode res;
    string readBuffer;

    string url = protocol + "://" + host + ":" + port + "/zabbix/api_jsonrpc.php";

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    if (curl) {
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonData.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << endl;
        } else {
            json response_json = json::parse(readBuffer);
        }

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }

    curl_global_cleanup();
    return readBuffer;
}

void check_default_mntr_cfg(ZabbixContext& zcontext) {
	// Check default default_host_group.
	// If it doesn't exist, create it
	string default_host_group = zcontext.get_mntrcfg("default_host_group");
	if (getHostGroup(default_host_group, zcontext).first == -1) {
		createHostGroup(default_host_group, zcontext);
	}
}

// API user.login
void userLogin(ZabbixContext& zcontext) {
	string jsonData = R"(
    {{
        "jsonrpc": "2.0",
        "method": "user.login",
        "params": {{
            "user": "{}",
            "password": "{}"
        }},
        "id": 1,
        "auth": null
    }})";
    jsonData = fmt::format(jsonData, zcontext.get_credentials().first,
	                                 zcontext.get_credentials().second);
    
    string response = callZabbixAPI(zcontext, jsonData);
    json response_json = json::parse(response);
    if (response_json.contains("result")) {
        string auth_token = response_json["result"];
        zcontext.update_auth_token(auth_token);
    } else {
        cerr << "Login failed. Full response:\n" << response_json.dump(2) << endl;
    }
}

// API hostgroup.get
pair<int, string> getHostGroup(string group_name, ZabbixContext& zcontext) {
	// pair<int, string>: first: id of group, second: name of group
	string jsonData = R"(
	{{
	    "jsonrpc": "2.0",
	    "method": "hostgroup.get",
	    "params": {{
	        "output": "extend",
	        "filter": {{
	            "name": [
	                "{}"
	            ]
	        }}
	    }},
	    "auth": "{}",
	    "id": 1
    }}
	)";
	jsonData = fmt::format(jsonData, group_name, zcontext.get_auth_token());
	string response = callZabbixAPI(zcontext, jsonData);
	json response_json = json::parse(response);
    if (response_json.contains("result")) {
        for (auto item : response_json["result"]) {
        	if (item["name"] == group_name) {
        		return make_pair(std::stoi(item["groupid"].get<std::string>()), group_name);
			}
		}
		return make_pair(-1, "Na");
    } else {
        cerr << "Get failed. Full response:\n" << response_json.dump(2) << endl;
        return make_pair(-1, "Na");
    }
}

// API hostgroup.create
int createHostGroup(string group_name, ZabbixContext& zcontext) {
	// return group_id
	string jsonData = R"(
	{{
	    "jsonrpc": "2.0",
	    "method": "hostgroup.create",
	    "params": {{
	        "name": "{}"
	    }},
	    "auth": "{}",
	    "id": 1
    }}
	)";
	jsonData = fmt::format(jsonData, group_name, zcontext.get_auth_token());
	string response = callZabbixAPI(zcontext, jsonData);
	json response_json = json::parse(response);
    if (response_json.contains("result")) {
        int group_id = stoi(response_json["result"]["groupids"][0].get<std::string>());
        return group_id;
    } else {
        cerr << "Create failed. Full response:\n" << response_json.dump(2) << endl;
        return -1;
    }
}

// API host.get
vector<ZabbixHost> getHost(string host_name, ZabbixContext& zcontext) {
	// return vector<ZabbixHost>
	// return an empty vector<ZabbixHost> if visiting error
	vector<ZabbixHost> zhv;
	string log_file = zcontext.common_cfg["log_file"];
	string jsonData = R"(
	{{
	    "jsonrpc": "2.0",
	    "method": "host.get",
	    "params": {{
	        "filter": {{
	            "host": [
	                "{}"
	            ]
	        }},
	        "selectInterfaces": ["interfaceid", "ip", "dns", "useip", "type"]
	    }},
	    "auth": "{}",
	    "id": 1
	}}
	)";
	jsonData = fmt::format(jsonData, host_name, zcontext.get_auth_token());
	string response = callZabbixAPI(zcontext, jsonData);
	json response_json = json::parse(response);
    if (response_json.contains("result")) {
    	for (auto host : response_json["result"]) {
    		string _host_name = host["host"];
    		string _host_id = host["hostid"];
    		ZabbixHost zh = ZabbixHost(_host_name, _host_id);
    		for (auto i : host["interfaces"]) {
    			zinterface zi;
    			zi["interfaceid"] = i["interfaceid"];
    			zi["ip"] = i["ip"];
    			zi["dns"] = i["dns"];
    			zi["useip"] = i["useip"];
    			zi["type"] = i["type"];
    			zh.zinterfaces.push_back(zi);
			}
			zhv.push_back(zh);
		}
        writeLog(log_file, "[zabbix-util.cpp->getHost()]Get host successfully: host_name=" + host_name + "\n");
        return zhv;
    } else {
    	writeLog(log_file, "Get host fail: host_name=" + host_name + "\n");
        cerr << "Get failed. Full response:\n" << response_json.dump(2) << endl;
        return zhv;
    }
}

ZabbixHost findHost(string host_name, string ip_addr, ZabbixContext& zcontext) {
	string log_file = zcontext.common_cfg["log_file"];
	vector<ZabbixHost> zhv = getHost(host_name, zcontext);
	for (ZabbixHost zh : zhv) {
		if (zh.host_name == host_name) {
			for (zinterface zi : zh.zinterfaces) {
				if (zi["ip"] == ip_addr) {
					writeLog(log_file, "[zabbix-util.cpp->findHost()] Found host with: host_name=" + host_name + ", ip_addr=" + ip_addr + " and host_id=" + zh.host_id + "\n");
					return zh;
				}
			}
		}
	}
	writeLog(log_file, "[zabbix-util.cpp->findHost()] Can not find any host with: host_name=" + host_name + ", ip_addr=" + ip_addr + "\n");
	throw std::logic_error("Not found host " + host_name + " with ip " + ip_addr);
}

ZabbixHost validateHost(ZabbixHost& zh, ZabbixContext& zcontext) {
    // Validate host configuration
    try {
            ZabbixHost valid_zh = findHost(zh.host_name, zh.host_conf["ip"], zcontext);
            return valid_zh;
    } catch (const std::exception& e) {
            throw;
    }
}

vector<ZabbixHost> validateHostList(vector<ZabbixHost>& zhv, ZabbixContext& zcontext) {
    vector<ZabbixHost> valid_zhv;
	vector<string> invalidHosts;
	string host_name, ip;

	for (ZabbixHost zh : zhv) {
		try {
		    host_name = zh.host_name;
		    ip = zh.host_conf["ip"];
            ZabbixHost valid_zh = validateHost(zh, zcontext);
            valid_zhv.push_back(valid_zh);
        } catch (const std::exception& e) {
            invalidHosts.push_back(host_name + "|" + ip);
        }
	}

	if (!invalidHosts.empty()) {
		string msg = "";
		for (string invalidHost : invalidHosts) {
			msg += "    " + invalidHost + "\n";
		}
		throw std::logic_error("Not found the following host:\n" + msg);
	}

    return valid_zhv;
}

int validateItemList(vector<ZabbixItem>& ziv, ZabbixContext& zcontext) {
    // return 0 if sucess, 1 if fail
    string test_template_name = "zabbix_test_template_" + getEpochTimeString();
    int template_id = createTemplate(test_template_name, zcontext);
    if (template_id == -1) return 1;

	vector<string> unsupportedKeyItems;
	for (ZabbixItem zi : ziv) {
		string key_str = zi.item_conf["key_"];
		size_t pos = key_str.find('[');
		if (pos != std::string::npos) {
			key_str = key_str.substr(0, pos);
		}
		
		if (zcontext.supported_item_key.find(key_str) == zcontext.supported_item_key.end()) {
			unsupportedKeyItems.push_back(zi.name + "[" + zi.item_conf["key_"] + "]");
		}
	}
	if (!unsupportedKeyItems.empty()) {
		string msg = "";
		for (auto unsuportKey : unsupportedKeyItems) {
			msg += "    " + unsuportKey + "\n";
		}
		throw std::logic_error("The following items have unsupported key:\n" + msg + "Please check details for them");
	}

	vector<string> invalidObjects;
    for (ZabbixItem zi : ziv) {
		if (zi.item_conf.find("type") == zi.item_conf.end()) {
			zi.item_conf["type"] = "7";
		}
        if (createItem(zi, template_id, stoi(zi.item_conf["type"]), zcontext) == -1) {
			invalidObjects.push_back(zi.name + "[" + zi.item_conf["key_"] + "]");
		}
    }
    
	deleteTemplate(template_id, zcontext, test_template_name);

	if (!invalidObjects.empty()) {
		string msg = "";
		for (auto invalidObj : invalidObjects) {
			msg += "    " + invalidObj + "\n";
		}
		throw std::logic_error("The following items are not correct:\n" + msg + "Please check details for them");
	}
	return 0;
}

int validateEventList(vector<ZabbixItem>& valid_ziv, vector<ZabbixEvent>& zev, ZabbixContext& zcontext) {
	// return 0 if sucess, 1 if fail
	string test_template_name = "zabbix_test_template_" + getEpochTimeString();
    int template_id = createTemplate(test_template_name, zcontext);
    if (template_id == -1) return 1;

	for (ZabbixItem zi : valid_ziv) {
		if (zi.item_conf.find("type") == zi.item_conf.end()) {
			zi.item_conf["type"] = "7";
		}
		createItem(zi, template_id, stoi(zi.item_conf["type"]), zcontext);
    }

	vector<string> invalidObjects;
	for (ZabbixEvent ze : zev) {
		ZabbixHost zh = ZabbixHost(test_template_name);
		int create_result = createEvent(ze, zh, zcontext);
		if (create_result != 0) {
			invalidObjects.push_back(ze.event_name);
		}
	}

	deleteTemplate(template_id, zcontext, test_template_name);

	if (!invalidObjects.empty()) {
		string msg = "";
		for (auto invalidObj : invalidObjects) {
			msg += "    " + invalidObj + "\n";
		}
		throw std::logic_error("The following events are not correct:\n" + msg + "Please check details for them");
	}
	return 0;
}

// API template.create
int createTemplate(string tempplate_name, ZabbixContext& zcontext) {
	// return template_id
	string log_file = zcontext.common_cfg["log_file"];
	string default_group_name = zcontext.get_mntrcfg("default_host_group");
	int default_group_id = getHostGroup(default_group_name, zcontext).first;
	if (default_group_id == -1) {
		for (int r = 1; r <= API_RETRY_TIME; r++) {
			default_group_id = createHostGroup(default_group_name, zcontext);
			if (default_group_id > 0) break;
			if (default_group_id == -1 && r == API_RETRY_TIME) {
				return -1;
			}
		}
	}
	
	string jsonData = R"(
	{{
	    "jsonrpc": "2.0",
	    "method": "template.create",
	    "params": {{
	        "host": "{}",
	        "groups": {{
	            "groupid": {}
	        }},
	        "templates": [
	        ],
	        "tags": [
	        ]
	    }},
	    "auth": "{}",
	    "id": 1
    }}
	)";
	jsonData = fmt::format(jsonData, tempplate_name, default_group_id, zcontext.get_auth_token());
	string response = callZabbixAPI(zcontext, jsonData);
	json response_json = json::parse(response);
    if (response_json.contains("result")) {
        int template_id = stoi(response_json["result"]["templateids"][0].get<std::string>());
        writeLog(log_file, "[zabbix-util.cpp->createTemplate()] Create template successfully: template_name=" + tempplate_name + ", template_id=" + to_string(template_id) + "\n");
        return template_id;
    } else {
    	writeLog(log_file, "[zabbix-util.cpp->createTemplate()] Create template fail: template_name=" + tempplate_name + "\n");
        cerr << "Create failed. Full response:\n" << response_json.dump(2) << endl;
        return -1;
    }
}

int deleteTemplate(int template_id, ZabbixContext& zcontext, string template_name) {
	// return template_id if success, -1 if fail;
	string log_file = zcontext.common_cfg["log_file"];
	
	string jsonData = R"(
	{{
        "jsonrpc": "2.0",
        "method": "template.delete",
        "params": [
            "{}"
        ],
        "auth": "{}",
        "id": 1
    }}
	)";
	jsonData = fmt::format(jsonData, template_id, zcontext.get_auth_token());
	string response = callZabbixAPI(zcontext, jsonData);
	json response_json = json::parse(response);
    if (response_json.contains("result")) {
        int template_id = stoi(response_json["result"]["templateids"][0].get<std::string>());
        writeLog(log_file, "[zabbix-util.cpp->deleteTemplate()] Delete template successfully: template_name=" + template_name + ", template_id=" + to_string(template_id) + "\n");
        return template_id;
    } else {
    	writeLog(log_file, "[zabbix-util.cpp->deleteTemplate()] Delete template fail: template_name=" + template_name + "\n");
        cerr << "Delete failed. Full response:\n" << response_json.dump(2) << endl;
        return -1;
    }
}

// API item.get get all item of a host determined by host_id
vector<ZabbixItem> getAllItemOfHost(int host_id, ZabbixContext& zcontext) {
	// return vector<ZabbixItem>
	// format of ZabbixItem which is return in vector<ZabbixItem>:
	//     		zi.name = i["name"];
    //    		zi.item_conf["itemid"] = i["itemid"];
	//    		zi.item_conf["type"] = i["type"];
	//    		zi.item_conf["key_"] = i["key_"];
	//    		zi.item_conf["delay"] = i["delay"];
	//    		zi.item_conf["status"] = i["status"];
	//    		zi.item_conf["value_type"] = i["value_type"];
	//    		zi.item_conf["units"] = i["units"]
	vector<ZabbixItem> ziv;
	string log_file = zcontext.common_cfg["log_file"];
	string jsonData = R"(
	{{
	    "jsonrpc": "2.0",
	    "method": "item.get",
	    "params": {{
	        "hostids": "{}"
	    }},
	    "auth": "{}",
	    "id": 1
	}}
	)";
	jsonData = fmt::format(jsonData, host_id, zcontext.get_auth_token());
	string response = callZabbixAPI(zcontext, jsonData);
	json response_json = json::parse(response);
    if (response_json.contains("result")) {
    	for (auto i : response_json["result"]) {
    		ZabbixItem zi = ZabbixItem(i["name"]);
    		zi.item_conf["itemid"] = i["itemid"];
    		zi.item_conf["type"] = i["type"];
    		zi.item_conf["key_"] = i["key_"];
    		zi.item_conf["delay"] = i["delay"];
    		zi.item_conf["status"] = i["status"];
    		zi.item_conf["value_type"] = i["value_type"];
    		zi.item_conf["units"] = i["units"];
    		ziv.push_back(zi);
		}
        writeLog(log_file, "[zabbix-util.cpp->getAllItemOfHost()] Get item of " + to_string(host_id) + " successfully. Number of item is: " + to_string(ziv.size()) + "\n");
        return ziv;
    } else {
    	writeLog(log_file, "[zabbix-util.cpp->getAllItemOfHost()] Get item of " + to_string(host_id) + " failed" + "\n");
        cerr << "Get failed. Full response:\n" << response_json.dump(2) << endl;
        return ziv;
    }
}

// API item.create
int createItem(ZabbixItem item, int host_id, int mntr_type, ZabbixContext& zcontext) {
	// return item_id
	string log_file = zcontext.common_cfg["log_file"];
	string jsonData = R"(
	{{
	    "jsonrpc": "2.0",
	    "method": "item.create",
	    "params": {{
	        "name": "{}",
	        "key_": "{}",
	        "hostid": "{}",
	        "type": {},
	        "value_type": {},
	        "tags": [
	        ],
	        "delay": "{}"
	    }},
	    "auth": "{}",
	    "id": 1
    }}
	)";
	jsonData = fmt::format(jsonData, item.name, item.item_conf["key_"], host_id,
	                       mntr_type, item.item_conf["value_type"], item.item_conf["delay"],
						   zcontext.get_auth_token());
	string response = callZabbixAPI(zcontext, jsonData);
	json response_json = json::parse(response);
    if (response_json.contains("result")) {
        int item_id = stoi(response_json["result"]["itemids"][0].get<std::string>());
        writeLog(log_file, "[zabbix-util.cpp->createItem()] Create item successfully: item_name=" + item.name + ", item_id=" + to_string(item_id) + "\n");
        return item_id;
    } else {
    	writeLog(log_file, "[zabbix-util.cpp->createItem()] Create template fail: item_name=" + item.name + "\n");
        cerr << "Create failed. Full response:\n" << response_json.dump(2) << endl;
        return -1;
    }
}

// API item.update
int updateItem(string itemid, ZabbixItem item_update, ZabbixContext& zcontext) {
	// return 0 if update successfully
	// return 1 if update fail
	string log_file = zcontext.common_cfg["log_file"];
	string jsonData = R"(
	{{
	    "jsonrpc": "2.0",
	    "method": "item.update",
	    "params": {{
	        "itemid": "{}"{}
	    }},
	    "auth": "{}",
	    "id": 1
	}}
	)";
	string update_str = "";
	for (auto it = item_update.item_conf.begin(); it != item_update.item_conf.end(); ++it) {
		string k = it->first;
		string v = it->second;
		if (k == "itemid") continue;
		if (ZITEM_VALUE_TYPE_MAP.find(k) != ZITEM_VALUE_TYPE_MAP.end()) {
			update_str = update_str + ", " + makeKeyValueStr(k, v, ZITEM_VALUE_TYPE_MAP.at(k));
		} else {
			writeLog(log_file, "[zabbix-util.cpp->updateItem()] [WARNING] key " + k + " isn't defined in ZITEM_VALUE_TYPE_MAP so that it is't passed an update parameter" + "\n");
		}
	}
	jsonData = fmt::format(jsonData, itemid, update_str, zcontext.get_auth_token());
	string response = callZabbixAPI(zcontext, jsonData);
	json response_json = json::parse(response);
    if (response_json.contains("result")) {
        string response_item_id = response_json["result"]["itemids"][0].get<std::string>();
        if (response_item_id == itemid) {
        	writeLog(log_file, "[zabbix-util.cpp->updateItem()] Update item successfully: itemid=" + itemid + "\n");
        	return 0;
		} else {
			writeLog(log_file, "[zabbix-util.cpp->updateItem()] Update item fail: itemid=" + itemid + "\n");
        	return 1;
		}
    } else {
    	writeLog(log_file, "[zabbix-util.cpp->updateItem()] Update item fail: itemid=" + itemid + "\n");
        cerr << "Update failed. Full response:\n" << response_json.dump(2) << endl;
        return 1;
    }
}

// API trigger.get get all triggers of a host determined by host_id
vector<ZabbixTrigger> getAllTriggerOfHost(int host_id, ZabbixContext& zcontext) {
	// return vector<ZabbixTrigger>
	// format of ZabbixTrigger which is return in vector<ZabbixTrigger>:
	//     		zt.description = t["description"];
	//     		zt["triggerid"] = t["triggerid"];
    //    		zt["event_name"] = t["event_name"];
	//    		zt["expression"] = t["expression"];
	//    		zt["priority"] = t["priority"];
	//    		zt["status"] = t["status"];
	//    		zt["type"] = t["type"];
	vector<ZabbixTrigger> ztv;
	string log_file = zcontext.common_cfg["log_file"];
	string jsonData = R"(
	{{
	    "jsonrpc": "2.0",
	    "method": "trigger.get",
	    "params": {{
	        "hostids": "{}",
	        "expandExpression": true
	    }},
	    "auth": "{}",
	    "id": 1
	}}
	)";
	jsonData = fmt::format(jsonData, host_id, zcontext.get_auth_token());
	string response = callZabbixAPI(zcontext, jsonData);
	json response_json = json::parse(response);
    if (response_json.contains("result")) {
    	for (auto t : response_json["result"]) {
    		ZabbixTrigger zt = ZabbixTrigger(t["description"]);
     		zt.trigger_conf["triggerid"] = t["triggerid"];
    		zt.trigger_conf["event_name"] = t["event_name"];
    		zt.trigger_conf["expression"] = t["expression"];
    		zt.trigger_conf["priority"] = t["priority"];
    		zt.trigger_conf["status"] = t["status"];
    		zt.trigger_conf["type"] = t["type"];
    		ztv.push_back(zt);
		}
        writeLog(log_file, "[zabbix-util.cpp->getAllTriggerOfHost()] Get trigger of " + to_string(host_id) + " successfully. Number of trigger is: " + to_string(ztv.size()) + "\n");
        return ztv;
    } else {
    	writeLog(log_file, "[zabbix-util.cpp->getAllTriggerOfHost()] Get trigger of " + to_string(host_id) + " failed" + "\n");
        cerr << "Get failed. Full response:\n" << response_json.dump(2) << endl;
        return ztv;
    }
}

// API trigger.create
int createTrigger(ZabbixTrigger trigger, ZabbixContext& zcontext) {
	// return trigger_id, if fail return -1
	string log_file = zcontext.common_cfg["log_file"];
	string jsonData = R"(
	{{
	    "jsonrpc": "2.0",
	    "method": "trigger.create",
	    "params": [
	        {{
	            {}
	        }}
	    ],
	    "auth": "{}",
	    "id": 1
	}}
	)";
	string update_str = "";
    update_str = makeKeyValueStr("description", trigger.description, ZTRIGGER_VALUE_TYPE_MAP.at("description"));
	for (auto it = trigger.trigger_conf.begin(); it != trigger.trigger_conf.end(); ++it) {
		string k = it->first;
		string v = it->second;
		if (k == "triggerid") continue;
		if (ZTRIGGER_VALUE_TYPE_MAP.find(k) != ZTRIGGER_VALUE_TYPE_MAP.end()) {
            update_str = update_str + ", " + makeKeyValueStr(k, v, ZTRIGGER_VALUE_TYPE_MAP.at(k));
		} else {
			writeLog(log_file, "[zabbix-util.cpp->createTrigger()] [WARNING] key " + k + " isn't defined in ZTRIGGER_VALUE_TYPE_MAP so that it is't passed an update parameter" + "\n");
		}
	}
	string _desc;
	if (trigger.trigger_conf.find("desc") != trigger.trigger_conf.end()) {
		_desc = trigger.trigger_conf.at("desc");
	} else if (trigger.trigger_conf.find("description") != trigger.trigger_conf.end()) {
		_desc = trigger.trigger_conf.at("description");
	}
	jsonData = fmt::format(jsonData, update_str, zcontext.get_auth_token());
	string response = callZabbixAPI(zcontext, jsonData);
	json response_json = json::parse(response);
    if (response_json.contains("result")) {
        int trigger_id = stoi(response_json["result"]["triggerids"][0].get<std::string>());
        writeLog(log_file, "[zabbix-util.cpp->createTrigger()] Create trigger successfully: trigger_name='" + _desc + "', trigger_id=" + to_string(trigger_id) + "\n");
        return trigger_id;
    } else {
    	writeLog(log_file, "[zabbix-util.cpp->createTrigger()] Create trigger fail: trigger_name='" + _desc + "'\n");
        cerr << "Create failed. Full response:\n" << response_json.dump(2) << endl;
        return -1;
    }
}

// API trigger.update
int updateTrigger(string triggerid, ZabbixTrigger trigger_update, ZabbixContext& zcontext) {
	// return 0 if update successfully
	// return 1 if update fail
	string log_file = zcontext.common_cfg["log_file"];
	string jsonData = R"(
	{{
	    "jsonrpc": "2.0",
	    "method": "trigger.update",
	    "params": {{
	        "triggerid": "{}"{}
	    }},
	    "auth": "{}",
	    "id": 1
	}}
	)";
	string update_str = "";
	for (auto it = trigger_update.trigger_conf.begin(); it != trigger_update.trigger_conf.end(); ++it) {
		string k = it->first;
		string v = it->second;
//		cout << "DEBUG DH.HUNG: " << k << ": " << v << endl;
		if (k == "triggerid") continue;
		if (ZTRIGGER_VALUE_TYPE_MAP.find(k) != ZTRIGGER_VALUE_TYPE_MAP.end()) {
			if (k == "desc") {
				update_str = update_str + ", " + makeKeyValueStr("description", v, ZTRIGGER_VALUE_TYPE_MAP.at(k));
			} else {
				update_str = update_str + ", " + makeKeyValueStr(k, v, ZTRIGGER_VALUE_TYPE_MAP.at(k));
			}
		} else {
			writeLog(log_file, "[zabbix-util.cpp->updateTrigger()] [WARNING] key " + k + " isn't defined in ZTRIGGER_VALUE_TYPE_MAP so that it is't passed an update parameter" + "\n");
		}
	}
	jsonData = fmt::format(jsonData, triggerid, update_str, zcontext.get_auth_token());
//	cout << "DEBUG DH.HUNG: [zabbix-util.cpp->updateTrigger()] jsonData=" << jsonData << endl;
	string response = callZabbixAPI(zcontext, jsonData);
	json response_json = json::parse(response);
    if (response_json.contains("result")) {
        string response_trigger_id = response_json["result"]["triggerids"][0].get<std::string>();
        if (response_trigger_id == triggerid) {
        	writeLog(log_file, "[zabbix-util.cpp->updateTrigger()] Update trigger successfully: triggerid=" + triggerid + "\n");
        	return 0;
		} else {
			writeLog(log_file, "[zabbix-util.cpp->updateTrigger()] Update trigger fail: triggerid=" + triggerid + "\n");
        	return 1;
		}
    } else {
    	writeLog(log_file, "[zabbix-util.cpp->updateTrigger()] Update trigger fail: triggerid=" + triggerid + "\n");
        cerr << "Update failed. Full response:\n" << response_json.dump(2) << endl;
        return 1;
    }
}

int disableTrigger(string triggerid, ZabbixContext& zcontext) {
	string log_file = zcontext.common_cfg["log_file"];
	ZabbixTrigger update_zt = ZabbixTrigger("placeholder_desc");
	update_zt.trigger_conf["status"] = "1";
	if (updateTrigger(triggerid, update_zt, zcontext) == 0) {
		writeLog(log_file, "[zabbix-util.cpp->disableTrigger()] Disable trigger success: triggerid=" + triggerid + "\n");
		return 0;
	} else {
		writeLog(log_file, "[zabbix-util.cpp->disableTrigger()] Disable trigger failed: triggerid=" + triggerid + "\n");
		return 1;
	}
}

// API trigger.delete
int deleteTrigger(string triggerid, ZabbixContext& zcontext) {
	// return 0 if delete successfully, return 1 if failed
	string log_file = zcontext.common_cfg["log_file"];
	string jsonData = R"(
	{{
	    "jsonrpc": "2.0",
	    "method": "trigger.delete",
	    "params": [
	        "{}"
	    ],
	    "auth": "{}",
	    "id": 1
	}}
	)";
	jsonData = fmt::format(jsonData, triggerid, zcontext.get_auth_token());
	string response = callZabbixAPI(zcontext, jsonData);
	json response_json = json::parse(response);
    if (response_json.contains("result")) {
        string trigger_id = response_json["result"]["triggerids"][0].get<std::string>();
        if (triggerid == trigger_id) {
        	writeLog(log_file, "[zabbix-util.cpp->deleteTrigger()] Delete trigger successfully: triggerid=" + triggerid + "\n");
        	return 0;
		}
    }
	writeLog(log_file, "[zabbix-util.cpp->deleteTrigger()] Delete trigger fail: triggerid='" + triggerid + "'\n");
	cerr << "Delete failed. Full response:\n" << response_json.dump(2) << endl;
	return 1;
}

int createEvent(ZabbixEvent ze, ZabbixHost zh, ZabbixContext& zcontext) {
    // return 0 if create event successfully
    // return 1 if there is any error when creating event
    string log_file = zcontext.common_cfg["log_file"];
    writeLog(log_file, "Start create event: " + ze.event_name + "\n");
    int result = 0;
    for (ZabbixTrigger zt : ze.ztv) {
        zt.trigger_conf["expression"] = fmt::format(zt.trigger_conf["expression"], zh.host_name);
        int trigger_id = createTrigger(zt, zcontext);
        if (trigger_id == -1) result = 1;
    }
    if (result == 0) {
        writeLog(log_file, "Create event: " + ze.event_name + " successfully\n");
    } else {
        writeLog(log_file, "Create event: " + ze.event_name + " failed\n");
    }
    return result;
}

int updateEvent(int update_mode, ZabbixHost zh, ZabbixEvent& update_ze, vector<ZabbixTrigger> ztv2, ZabbixContext& zcontext) {
    string log_file = zcontext.common_cfg["log_file"];
    int result = 0;
    if (update_mode == 0) {
        writeLog(log_file, "[update_mode=0]Start update event: description=" + update_ze.description + ", event_name=" + update_ze.event_name + "\n");
        vector<bool> updated(1000, false);
        vector<ZabbixTrigger> ztv1 = update_ze.ztv;
        for (int k = 0; k < ztv1.size(); k++) {
            ZabbixTrigger zt = ztv1[k];
            zt.trigger_conf["expression"] = fmt::format(zt.trigger_conf["expression"], zh.host_name);
            ztv1[k] = zt;
        }

        for (ZabbixTrigger zt1 : ztv1) {
            int idx = 0;
            bool priority_match = false;
            for (ZabbixTrigger zt2 : ztv2) {
                if (updated[idx]) continue;
                if (zt1.trigger_conf["priority"] == zt2.trigger_conf["priority"]) {
                    int update_result = updateTrigger(zt2.trigger_conf["triggerid"], zt1, zcontext);
                    if (update_result == 0) {
                        updated[idx] = true;
                        priority_match = true;
                    } else {
                        result = 1;
                    }
                }
            }
            if (!priority_match) {
                if (createTrigger(zt1, zcontext) == -1) result = 1;
            }
        }
        for (int i = 0; i < ztv2.size(); i++) {
            if (updated[i]) continue;
            if (disableTrigger(ztv2[i].trigger_conf["triggerid"], zcontext) != 0) result = 1; 
        }
    } else if (update_mode == 1) {
        vector<ZabbixTrigger> ztv1 = update_ze.ztv;
        for (int k = 0; k < ztv1.size(); k++) {
            ZabbixTrigger zt1 = ztv1[k];
            zt1.trigger_conf["expression"] = fmt::format(zt1.trigger_conf["expression"], zh.host_name);
            ztv1[k] = zt1;
        }
        
        for (ZabbixTrigger zt2 : ztv2) {
            int del_result = deleteTrigger(zt2.trigger_conf["triggerid"], zcontext);
            if (del_result != 0) result = 1;
        }
        for (ZabbixTrigger zt1 : ztv1) {
            if (createTrigger(zt1, zcontext) == -1) result = 1;
        }
    }
    return result;
}

pair<int, pair<vector<ZabbixItem>, vector<ZabbixEvent>>> parseUpdateConf(string update_file) {
	vector<ZabbixItem> ziv;
	vector<ZabbixEvent> zev;
    YAML::Node root;
    try{
        root = loadYamlFile(update_file);
    }
    catch(const std::exception& e){
        throw;
    }
	
	int update_mode = 1;
	if (root["update_mode"]) {
		if (root["update_mode"].as<std::string>() != "null") {
			update_mode = root["update_mode"].as<int>();
		}
	}
	
	// Parse item update
	if (root["items"]) {
		const YAML::Node& items = root["items"];
		for (const auto& item : items) {
            ZabbixItem zi = ZabbixItem(item["name"].as<std::string>());
            zi.item_conf = parseJsonPair(item);
			ziv.push_back(zi);
		}
	}
	
	// Parse events update
	if (root["events"]) {
		const YAML::Node& events = root["events"];
		for (const auto& event : events) {
			string description = event["description"].as<std::string>();
			string event_name = event["event_name"].as<std::string>();
			ZabbixEvent ze = ZabbixEvent(description, event_name);
			
			const YAML::Node& triggers = event["triggers"];
			for (const auto& trigger : triggers) {
                ZabbixTrigger zt = ZabbixTrigger(description);
				zt.trigger_conf = parseJsonPair(trigger);
                zt.trigger_conf["event_name"] = event_name;
				ze.ztv.push_back(zt);
			}
			zev.push_back(ze);
		}
	}
	
	return make_pair(update_mode, make_pair(ziv, zev));
}

int update_mntr_conf(int update_mode, ZabbixHost zh, vector<ZabbixItem> update_ziv, vector<ZabbixEvent> update_zev, ZabbixContext& zcontext) {
	int result = 0;
    vector<string> failObjects;
	string log_file = zcontext.common_cfg["log_file"];
	vector<ZabbixItem> all_ziv = getAllItemOfHost(stoi(zh.host_id), zcontext);
	vector<ZabbixTrigger> all_ztv = getAllTriggerOfHost(stoi(zh.host_id), zcontext);
	unordered_map<string, vector<ZabbixTrigger>> all_events;
	
	for (ZabbixTrigger zt : all_ztv) {
		string k = zt.description + "@@@" + zt.trigger_conf["event_name"];
		if (all_events.find(k) != all_events.end()) {
			all_events[k].push_back(zt);
		} else {
			vector<ZabbixTrigger> a;
			a.push_back(zt);
			all_events[k] = a;
		}
	}
	
	// Update item for host
	for (ZabbixItem update_zi : update_ziv) {
		string itemid;
		for (ZabbixItem zi : all_ziv) {
			if (zi.item_conf["key_"] == update_zi.item_conf["key_"]) {
				itemid = zi.item_conf["itemid"];
				break;
			}
		}

        if (itemid.empty()) {
            int create_result = createItem(update_zi, stoi(zh.host_id), stoi(update_zi.item_conf["type"]), zcontext);
            if (create_result == -1) {
                failObjects.push_back("item: " + update_zi.name);
                writeLog(log_file, "Create item fail: host_name=" + zh.host_name + ", host_ip=" + zh.host_conf["ip"] + ", item=" + update_zi.name + "\n");
                result = 1;
            } else {
                writeLog(log_file, "Create item success: host_name=" + zh.host_name + ", host_ip=" + zh.host_conf["ip"] + ", item=" + update_zi.name + "\n");
            }
            continue;
        }

		int update_result = updateItem(itemid, update_zi, zcontext);
		if (update_result == 0) {
			writeLog(log_file, "Update item success: host_name=" + zh.host_name + ", host_ip=" + zh.host_conf["ip"] + ", item=" + update_zi.item_conf["name"] + "\n");
		} else {
            failObjects.push_back("item: " + update_zi.name);
			writeLog(log_file, "Update item fail: host_name=" + zh.host_name + ", host_ip=" + zh.host_conf["ip"] + ", item=" + update_zi.item_conf["name"] + "\n");
			result = 1;
		}
	}
	
	// Update event for host
	for (ZabbixEvent update_ze : update_zev) {
		string k = update_ze.description + "@@@" + update_ze.event_name;
		if (all_events.find(k) == all_events.end()) {
			if(createEvent(update_ze, zh, zcontext) == 1) {
                failObjects.push_back("event: " + update_ze.event_name);
                result = 1;
            }
		} else {
            if (updateEvent(update_mode, zh, update_ze, all_events.at(k), zcontext) == 1) {
                failObjects.push_back("event: " + update_ze.event_name);
                result = 1;
            }
		}
	}
    
    if (!failObjects.empty()) {
        cout << "Failed Objects:" << endl;
        for (string failobj : failObjects) {
            cout << failobj << endl;
        }
    }
	
	return result;
}

ZabbixRegex::ZabbixRegex(string n) {
    this->name = n;
}

// API regexp.create
int createRegexp(ZabbixRegex zr, ZabbixContext& zcontext) {
	// return int(regexpid) if delete successfully, return -1 if failed
	string log_file = zcontext.common_cfg["log_file"];
	string jsonData = R"(
	{{
		"jsonrpc": "2.0",
		"method": "regexp.create",
		"params": {{
			"name": "{}"{}
        }},
		"auth": "{}",
		"id": 1
    }}
	)";
	string param_str = "";
	if(!zr.test_string.empty()) {
		param_str += "," + makeKeyValueStr("test_string", zr.test_string, REGEX_EXPRESSION_VALUE_TYPE_MAP.at("test_string"));
	}
	string expressions_str = "[";
	int cnt = 0;
	for (RegexExpression expression : zr.expressions) {
		cnt++;
		string expression_str = "{";
		for (auto it = expression.begin(); it != expression.end(); it++) {
			string k = it->first;
			string v = it->second;
			if (it == expression.begin()) {
				expression_str += makeKeyValueStr(k, v, REGEX_EXPRESSION_VALUE_TYPE_MAP.at(k));
				continue;
			}
			expression_str += "," + makeKeyValueStr(k, v, REGEX_EXPRESSION_VALUE_TYPE_MAP.at(k));
		}
		expression_str += "}";
		if (cnt == zr.expressions.size()) {
			expressions_str += expression_str;
		} else {
			expressions_str += expression_str + ",";
		}
	}
	expressions_str += "]";
	param_str += "," + makeKeyValueStr("expressions", expressions_str, REGEX_EXPRESSION_VALUE_TYPE_MAP.at("expressions"));

	jsonData = fmt::format(jsonData, zr.name, param_str, zcontext.get_auth_token());
	string response = callZabbixAPI(zcontext, jsonData);
	json response_json = json::parse(response);
    if (response_json.contains("result")) {
        int regexpid = stoi(response_json["result"]["regexpids"][0].get<std::string>());
		writeLog(log_file, "[zabbix-util.cpp->createRegexp()] create regexp successfully: regexpid=" + to_string(regexpid) + "\n");
		return regexpid;
    } else {
    	writeLog(log_file, "[zabbix-util.cpp->createRegexp()] create regexp fail \n");
        cerr << "Create failed. Full response:\n" << response_json.dump(2) << endl;
        return -1;
    }
}