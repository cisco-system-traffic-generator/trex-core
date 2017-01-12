import os
import outer_packages
import json
import pprint
from elasticsearch import Elasticsearch
from pprint import pprint
from elasticsearch import helpers
import random
import datetime

# one object example for perf 
def create_one_object (build_id):
    d={};

    sim_date=datetime.datetime.now()-datetime.timedelta(hours=random.randint(0,24*30));
    info = {};


    img={}
    img['sha'] = random.choice(["v2.11","v2.10","v2.12","v2.13","v2.14"])
    img['build_time'] = sim_date.strftime("%Y-%m-%d %H:%M:%S")
    img['version'] = random.choice(["v2.11","v2.10","v2.12","v2.13","v2.14"])
    img['formal'] = False

    setup={}

    setup['distro']='Ubunto14.03'
    setup['kernel']='2.6.12'
    setup['baremetal']=True
    setup['hypervisor']='None'
    setup['name']='trex07'
    setup['cpu-sockets']=2
    setup['cores']=16
    setup['cpu-speed']=3.5

    setup['dut'] ='loopback'
    setup['drv-name']='mlx5'
    setup['nic-ports']=2 
    setup['total-nic-ports']=2
    setup['nic-speed'] ="40GbE"



    info['image'] = img
    info['setup'] = setup

    d['info'] =info;

    d['timestamp']=sim_date.strftime("%Y-%m-%d %H:%M:%S")
    d['build_id']=str("build-%d" %(build_id))
    d['test']={ "name" : "test1",
                "type"  : "stateless",
                "cores" : random.randint(1,10),
                "cpu%" : random.randint(60,99),
                "mpps" :  random.randint(9,32),
                "mpps_pc" :  random.randint(9,32),
                "gbps_pc" :  random.randint(9,32),
                "gbps" :  random.randint(9,32),
                "avg-pktsize" : random.randint(60,1500),
                "latecny" : { "min" : random.randint(1,10),
                              "max" : random.randint(100,120),
                              "avr" : random.randint(1,60)
                             }
        };


    return(d)


class EsHelper(object):

    def __init__ (self, es,
                        alias,
                        index_name,
                        mapping):
        self.es      = es
        self.alias   = alias 
        self.index_name = index_name
        self.mapping = mapping
        self.setting = { "index.mapper.dynamic":"false"};

    def delete (self):
        es=self.es;
        es.indices.delete(index=self.alias, ignore=[400, 404]);

    def is_exists (self):
        es=self.es;
        return es.indices.exists(index=self.alias, ignore=[400, 404])

    def create_first_fime (self):
        es=self.es;
        index_name=self.index_name
        es.indices.create(index=index_name, ignore=[],body = {               
           "aliases": { self.alias : {} }, 
           "mappings" : { "data": self.mapping },
           "settings" : self.setting
           });

    def update(self):
        es=self.es;
        es.indices.put_mapping(index=self.alias, doc_type="data",body=self.mapping);
        es.indices.rollover(alias=self.alias,body={
              "conditions": {
                            "max_age":   "30d",
                            "max_docs":  100000
                            },
               "mappings" : { "data": self.mapping },
               "settings" : self.setting
               }
              );

    def open(self):
        if not self.is_exists():
            self.create_first_fime ()
        else:
            self.update()

    def close(self):
        pass;

    def push_data(self,data):
        es=self.es;
        es.index(index=self.alias,doc_type="data", body=data);




def create_reg_object (build_id):
    d={};

    sim_date=datetime.datetime.now()-datetime.timedelta(hours=random.randint(0,24*30));
    info = {};


    img={}
    img['sha'] = random.choice(["v2.11","v2.10","v2.12","v2.13","v2.14"])
    img['build_time'] = sim_date.strftime("%Y-%m-%d %H:%M:%S")
    img['version'] = random.choice(["v2.11","v2.10","v2.12","v2.13","v2.14"])
    img['formal'] = False

    setup={}

    setup['distro']='Ubunto14.03'
    setup['kernel']='2.6.12'
    setup['baremetal']=True
    setup['hypervisor']='None'
    setup['name']='trex07'
    setup['cpu-sockets']=2
    setup['cores']=16
    setup['cpu-speed']=3.5

    setup['dut'] ='loopback'
    setup['drv-name']='mlx5'
    setup['nic-ports']=2 
    setup['total-nic-ports']=2
    setup['nic-speed'] ="40GbE"



    info['image'] = img
    info['setup'] = setup

    d['info'] =info;

    d['timestamp']=sim_date.strftime("%Y-%m-%d %H:%M:%S")
    d['build_id']=str("build-%d" %(build_id))
    d['test']= { "name" : "stateful_tests.trex_imix_test.CTRexIMIX_Test.test_routing_imix" ,
                "type"  : "stateless",
                "duration_sec" : random.uniform(1,10),
                "result" : random.choice(["PASS","SKIP","FAIL"]),
                "stdout" : """
                            LATEST RESULT OBJECT:
                            Total ARP received : 16 pkts 
                            maximum-latency : 300 usec 
                            average-latency : 277 usec 
                            latency-any-error : ERROR 
                """
        };

    return(d)



# how to add new keyword 
# you can add a new field but you can't remove old field
class TRexEs(object):

    def __init__ (self, host,
                        port,
                        ):
        self.es = Elasticsearch([{"host": host, "port": port}])
        es=self.es;
        res=es.info()
        es_version=res["version"]["number"];
        l=es_version.split('.');
        if not(len(l)==3 and int(l[0])>=5):
            print("NOT valid ES version should be at least 5.0.x",es_version);
            raise RuntimeError

        setup_info = { # constant per setup
                         "properties": {

                          "image" : { 
                              "properties": {
                                "sha"          : { "type": "keyword" },   # git sha
                                "build_time"   : { "type": "date",        # build time 
                                                  "format": "yyyy-MM-dd HH:mm:ss||yyyy-MM-dd||epoch_millis"},
                                "version"      : { "type": "keyword" },   # version name like 'v2.12'
                                "formal"       : { "type": "boolean" },   # true for formal release  
                               }
                          },

                          "setup" : { 
                              "properties": {
                               "distro"         : { "type": "keyword" },   # 'ubuntu'
                               "kernel"         : { "type": "keyword" },   # 2.3.19
                               "baremetal"      : { "type": "boolean" },   # true or false for 
                               "hypervisor"     : { "type": "keyword" },   # kvm,esxi , none
                               "name"           : { "type": "keyword" },   # setup name , e.g. kiwi02
                               "cpu-sockets"    : { "type": "long" },      # number of socket   
                               "cores"          : { "type": "long" },      # total cores
                               "cpu-speed"      : { "type": "double" },    # 3.5 in ghz
                               "dut"            : { "type": "keyword" },   # asr1k, loopback
                               "drv-name"       : { "type": "keyword" },   # vic, mlx5,599,xl710,x710
                               "nic-ports"      : { "type": "long" },      #2,1,4
                               "total-nic-ports"  : { "type": "long" },    #8
                               "nic-speed"      : { "type": "keyword" },   #40Gb
                           }
                         }
                       }
                   }


        perf_mapping = {
              "dynamic": "strict",
              "properties": {

                   "scenario"  : { "type": "keyword" },
                   "build_id"  : { "type": "keyword" },
                   "timestamp" : { "type": "date",
                             "format": "yyyy-MM-dd HH:mm:ss||yyyy-MM-dd||epoch_millis"},

                  "info"     : setup_info,

                  "test"     : {
                                "properties": {
                                    "name"        : { "type": "keyword" }, # name of the test 
                                    "type"        : { "type": "keyword" }, # stateless,stateful, other
                                    "cores"       : { "type": "long" },
                                    "cpu%"        : { "type": "double" },
                                    "mpps"        : { "type": "double" },
                                    "streams_count" : { "type": "long" },
                                    "mpps_pc"     : { "type": "double" },
                                    "gbps_pc"     : { "type": "double" },
                                    "gbps"        : { "type": "double" },
                                    "avg-pktsize" : { "type": "long" },
                                    "kcps"        : { "type": "double" },
                                    "latecny"     : {
                                                     "properties": {
                                                       "min"        : { "type": "double" },
                                                       "max"        : { "type": "double" },
                                                       "avr"        : { "type": "double" },
                                                       "max-win"    : { "type": "double" },
                                                       "drop-rate"  : { "type": "double" },
                                                       "jitter"     : { "type": "double" },
                                                      }
                                                    }

                                }
                           }       
            }
        }

        self.perf = EsHelper(es=es,
                             alias="perf",
                             index_name='trex_perf-000001',
                             mapping=perf_mapping)



        reg_mapping = {
              "dynamic": "strict",
              "properties": {

                   "scenario"  : { "type": "keyword" },
                   "build_id"  : { "type": "keyword" },
                   "timestamp" : { "type": "date",
                             "format": "yyyy-MM-dd HH:mm:ss||yyyy-MM-dd||epoch_millis"},

                  "info"     : setup_info,

                  "test"     : {
                                "properties": {
                                    "name"        : { "type": "text" }, # name of the test 
                                    "type"        : { "type": "keyword" }, # stateless,stateful, other
                                    "duration_sec" : { "type": "double" }, # sec
                                    "result"      : { "type": "keyword" }, # PASS,FAIL,SKIP
                                    "stdout"      : { "type": "text" },  # output in case of faliue
                                }
                           }       
            }
        }


        self.reg = EsHelper(es=es,
                             alias="reg",
                             index_name='trex_reg-000001',
                             mapping=reg_mapping)


        self.perf.open();
        self.reg.open();








