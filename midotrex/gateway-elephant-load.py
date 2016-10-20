#
# Copyright 2016 Midokura SARL
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
try:
    from trex_stl_lib.api import *
except ImportError:
    print "You need to add <TREX_ROOT>/scripts/automation/trex_control_plane/stl to PYTHONPATH"
    sys.exit(-1)

import yaml
import time, argparse
import pprint

from influxdb import InfluxDBClient

from midotrex.streams import *
from midotrex.stats import *

def run_load(cfg):
    c = STLClient()
    c.connect()

    uplink = uplink_stream_elephant(stream_id=cfg['uplink_stream_id'],
                                    trex_uplink_mac=cfg['trex_uplink_mac'],
                                    agent_uplink_mac=cfg['agent_uplink_mac'],
                                    uplink_stream_target_ip=cfg['uplink_stream_target_ip'],
                                    bit_rate=cfg['bit_rate'])
    tunnel = tunnel_stream_elephant(stream_id=cfg['tunnel_stream_id'],
                                    trex_underlay_mac=cfg['trex_underlay_mac'],
                                    agent_underlay_mac=cfg['agent_underlay_mac'],
                                    agent_underlay_ip=cfg['agent_underlay_ip'],
                                    agent_uplink_tunnelkey=cfg['agent_uplink_tunnelkey'],
                                    agent_uplink_mac=cfg['agent_uplink_mac'],
                                    trex_uplink_mac=cfg['trex_uplink_mac'],
                                    bit_rate=cfg['bit_rate'])
    c.reset([cfg['tunnel_port_index'], cfg['uplink_port_index']])
    c.add_streams([uplink], ports=[cfg['uplink_port_index']])
    c.add_streams([tunnel], ports=[cfg['tunnel_port_index']])
    c.clear_stats()

    c.start(ports=[cfg['uplink_port_index'], cfg['tunnel_port_index']],
            duration=cfg['duration'] + cfg['warmup'])

    print "Running warmup (%d secs)" % cfg['warmup']
    time.sleep(cfg['warmup'])
    c.clear_stats()

    print "Running workload (%d bps for %d seconds)" % (cfg['bit_rate'],
                                                        cfg['duration'])
    influx = InfluxDBClient(host=cfg['influxdb_host'], port=cfg['influxdb_port'],
                            database=cfg['influxdb_database'])

    for i in range(0, cfg['duration']):
        stats = c.get_stats(None, True)
        try:
            push_to_influx(influx, cfg, stats)
        except Exception, err:
            print "Error parsing stats %s" % err
        time.sleep(1)

    stats = c.get_stats(None, True)
    pprint.pprint(stats)


parser = argparse.ArgumentParser(description="Run midonet trex workload")
parser.add_argument('--duration', dest='duration', default=60, type=int)
parser.add_argument('--bitrate', dest='bitrate', default=1000, type=int)
parser.add_argument('--gateways', dest='gateways', default=3, type=int)
parser.add_argument('--warmup', dest='warmup', default=10, type=int)
parser.add_argument('--profile', dest='profile', default='sa5-5.2.yaml', type=str)

args = parser.parse_args()

cfg = yaml.load(file(args.profile, 'r'))

cfg['duration'] = args.duration
cfg['bit_rate'] = args.bitrate
cfg['gateway_count'] = args.gateways
cfg['warmup'] = args.warmup
cfg['profile'] = args.profile


run_load(cfg)
