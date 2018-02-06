import sys
import os
import json
import datetime
import time

ext_path = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir, 'scripts', 'external_libs'))
elk_path = os.path.join(ext_path, 'elasticsearch')
urllib_path = os.path.join(ext_path, 'urllib3')

if elk_path not in sys.path:
    sys.path.append(elk_path)
if urllib_path not in sys.path:
    sys.path.append(urllib_path)

import elasticsearch
import elasticsearch.helpers


class ELKManager:
    def __init__(self, hostname, index='trex_perf-*', port=9200):
        self.hostname = hostname
        self.index = index
        self.port = port
        self.setup_names = ['trex07', 'trex08', 'trex09', 'trex11', 'kiwi02','trex19']
        self.es = elasticsearch.Elasticsearch([{"host": hostname, "port": port}])
        self.all_data_raw = {}
        self.all_data_parsed = {}

    @staticmethod
    def time_res_calculation():
        seconds_since_epoch = int(time.time())
        time_2w_ago = datetime.date.timetuple(datetime.datetime.utcnow() - datetime.timedelta(weeks=2))
        two_w_ago_epoch_second = int(time.mktime(time_2w_ago))
        return seconds_since_epoch, two_w_ago_epoch_second

    def fetch_all_data(self):
        res = {}
        seconds_since_epoch, two_w_ago_epoch_second = self.time_res_calculation()
        for setup_name in self.setup_names:
            query = {
                "_source": ["info.setup.name", "test.name", "test.mpps_pc", "timestamp", "build_id"],
                "size": 10000,
                "query": {
                    "bool": {
                        "filter": [
                            {"term": {"info.setup.name": setup_name}},
                            {"term": {"test.type": "stateless"}},
                            {"range": {"timestamp": {"gte": two_w_ago_epoch_second, "lte": seconds_since_epoch,
                                                     "format": "epoch_second"}}}
                        ]
                    }
                }
            }

            res[setup_name] = list(elasticsearch.helpers.scan(self.es, index=self.index, query=query, size=10000))
        self.all_data_raw = res

    def parse_raw_data(self):
        for setup_name in self.all_data_raw:
            for query in self.all_data_raw[setup_name]:
                setup_name = query['_source']['info']['setup']['name']
                test_name = query['_source']['test']['name']
                test_result = query['_source']['test']['mpps_pc']
                timestamp = query['_source']['timestamp']
                build_id = query['_source']['build_id']
                if setup_name not in self.all_data_parsed.keys():
                    self.all_data_parsed[setup_name] = {}
                if test_name not in self.all_data_parsed[setup_name].keys():
                    self.all_data_parsed[setup_name][test_name] = []
                self.all_data_parsed[setup_name][test_name].append(tuple((test_name, timestamp, test_result, build_id)))
            self.all_data_parsed = self.sorted(self.all_data_parsed)

    @staticmethod
    def sorted(parsed_data):
        sorted_tests_data = {}
        for setup_name in parsed_data:
            setup_tests_data = parsed_data[setup_name]
            sorted_tests_data[setup_name] = {}
            for test_name in setup_tests_data:
                sorted_tests_data[setup_name][test_name] = sorted(setup_tests_data[test_name],
                                                                  key=lambda (_, timestamp, __, ___): timestamp)
        return sorted_tests_data

    def fetch_and_parse(self):
        self.fetch_all_data()
        self.parse_raw_data()
        return self.all_data_parsed
