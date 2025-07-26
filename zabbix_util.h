#ifndef ZABBIX_UTIL_H
#define ZABBIX_UTIL_H

#include <yaml-cpp/yaml.h>
#include "json.hpp"
#include <array>
#include <string>
#include <vector>
#include <unordered_map>

using namespace std;
using json = nlohmann::json;

typedef pair<string, string> Credentials;
typedef unordered_map<string, string> zinterface;
typedef unordered_map<string, string> RegexExpression;

extern const unordered_map<string, int> ZITEM_VALUE_TYPE_MAP;
extern const unordered_map<string, int> ZTRIGGER_VALUE_TYPE_MAP;

unordered_map<string, string> parseJsonPair(const YAML::Node& jp);
string getEpochTimeString();

YAML::Node loadYamlFile(const string& file_path);

string getExecutableDirectory();

class ZabbixContext{
	private:
		string zone_name;
		string host;
		int port;
		bool ssl;
		Credentials credential;
		string auth_token;
		unordered_map<string, string> mntr_cfg;
	
	public:
		unordered_map<string, string> common_cfg;
		unordered_map<string, string> supported_item_key;
		
		ZabbixContext(string zn);
		ZabbixContext(string zn, string h, int p, bool s, Credentials c);
		
		void update_mntr_cfg(string k, string v);
		void update_host(string h);
		void update_port(int p);
		void update_auth_token(string auth);
		string get_zone_name();
		string get_host();
		int get_port();
		bool get_ssl();
		Credentials get_credentials();
		string get_auth_token();
		string get_mntrcfg(string k);
};

vector<ZabbixContext> parseZabbixUtilConf(string conf_file);

class ZabbixHost {
	public:
		string host_name;
		string host_id;
		vector<zinterface> zinterfaces;
        unordered_map<string, string> host_conf;
		
        ZabbixHost(string hn);
		ZabbixHost(string hn, string hi);
};

vector<ZabbixHost> parseHostList(string host_list_file);

class ZabbixItem {
    public:
        string name;
        unordered_map<string, string> item_conf;

        ZabbixItem(string n);
};

class ZabbixTrigger {
    public:
        string description;
        unordered_map<string, string> trigger_conf;

        ZabbixTrigger(string desc);
};

class ZabbixEvent {
	public:
		string description;
		string event_name;
		vector<ZabbixTrigger> ztv;
		
		ZabbixEvent(string desc, string en);
};
int createEvent(ZabbixEvent ze, ZabbixHost zh, ZabbixContext& zcontext);
int updateEvent(int update_mode, ZabbixHost zh, ZabbixEvent& update_ze, vector<ZabbixTrigger> ztv2, ZabbixContext& zcontext);

class ZabbixTemplate{
	public:
		string template_name;
		int monitoring_type;
		vector<ZabbixItem> zitems;
		vector<ZabbixEvent> zevents;
		
		ZabbixTemplate(string temp_name);
};

ZabbixTemplate parseZabbixTemplate(string template_file);
int validateZabbixTemplate(ZabbixTemplate ztmpl, ZabbixContext& zcontext);

pair<int, pair<vector<ZabbixItem>, vector<ZabbixEvent>>> parseUpdateConf(string update_file);
int update_mntr_conf(int update_mode, ZabbixHost zh, vector<ZabbixItem> update_ziv, vector<ZabbixEvent> update_zev, ZabbixContext& zcontext);

vector<ZabbixHost> validateHostList(vector<ZabbixHost>& zhv, ZabbixContext& zcontext);
ZabbixHost validateHost(ZabbixHost& zh, ZabbixContext& zcontext);

int validateItemList(vector<ZabbixItem>& ziv, ZabbixContext& zcontext);
int validateEventList(vector<ZabbixItem>& valid_ziv, vector<ZabbixEvent>& zev, ZabbixContext& zcontext);

string callZabbixAPI(ZabbixContext& zcontext, string jsonData);
void check_default_mntr_cfg(ZabbixContext& zcontext);
string makeKeyValueStr(string k, string v, int value_type);

// API user.login
void userLogin(ZabbixContext& zcontext);

// API hostgroup.get
pair<int, string> getHostGroup(string group_name, ZabbixContext& zcontext);

// API hostgroup.create
int createHostGroup(string group_name, ZabbixContext& zcontext);

// API host.get
vector<ZabbixHost> getHost(string host_name, ZabbixContext& zcontext);
ZabbixHost findHost(string host_name, string ip_addr, ZabbixContext& zcontext);

// API template.create
int createTemplate(string tempplate_name, ZabbixContext& zcontext);

// API template.delete
int deleteTemplate(int template_id, ZabbixContext& zcontext, string template_name="");

// API item.get get all item of a host determined by host_id
vector<ZabbixItem> getAllItemOfHost(int host_id, ZabbixContext& zcontext);

// API item.create
int createItem(ZabbixItem item, int host_id, int mntr_type, ZabbixContext& zcontext);

// API item.update
int updateItem(string itemid, ZabbixItem item_update, ZabbixContext& zcontext);

// API trigger.get get all triggers of a host determined by host_id
vector<ZabbixTrigger> getAllTriggerOfHost(int host_id, ZabbixContext& zcontext);

// API trigger.create
int createTrigger(ZabbixTrigger trigger, ZabbixContext& zcontext);

// API trigger.update
int updateTrigger(string triggerid, ZabbixTrigger trigger_update, ZabbixContext& zcontext);
int disableTrigger(string triggerid, ZabbixContext& zcontext);

// API trigger.delete
int deleteTrigger(string triggerid, ZabbixContext& zcontext);

class ZabbixRegex{
    public:
	    string name;
		string regexpid;
		string test_string;
		vector<RegexExpression> expressions;

		ZabbixRegex(string n);

		string genExpressionsStr();
};

// API regexp.create
int createRegexp(ZabbixRegex zr, ZabbixContext& zcontext);

// API regexp.delete
int deleteRegexp(ZabbixRegex zr, ZabbixContext& zcontext);

// API regexp.get
ZabbixRegex getRegexp(ZabbixContext& zcontext, string name="");

// API regexp.update
int updateRegexp(ZabbixRegex update_zr, ZabbixContext& zcontext);

#endif