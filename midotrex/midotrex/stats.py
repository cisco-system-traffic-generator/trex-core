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
import time, datetime, argparse
import pprint, binascii, traceback

def percentiles(hist):
    keys = hist.keys()
    keys.sort()
    total = sum(hist.values())
    def calc(perc):
        count = 0
        for k in keys:
            count += hist[k]
            if float(count)/total >= perc:
                return (perc, k)
        return None
    return filter(lambda (x): x != None,
                  map(calc, [0.50, 0.95, 0.98, 0.99, 0.999, 0.9999]))

def get_latency_stats(profile, stats, stream_id, prefix):
    lstats = []
    try:
        stream_latency = stats['latency'][stream_id]
        if stream_latency == None:
            return lstats

        latency = stream_latency.get('latency')
        if latency != None:
            histogram = latency.get('histogram')
            if histogram != None:
                lstats += [to_measurement(profile,
                                          "%s_latency_p%f" % (prefix, p[0]),
                                            p[1])\
                             for p in percentiles(histogram)]
            lstats += [to_measurement(profile, "%s_latency_%s" % (prefix, k),
                                      latency[k])\
                       for k in filter(lambda (j): j != 'histogram',
                                       latency.keys())]
        errors = stream_latency.get('err_cntrs')
        if errors != None:
            lstats += [to_measurement(profile, "%s_err_%s" % (prefix, k),
                                      errors[k])\
                         for k in errors.keys()]
    except Exception, err:
        print "Error getting latency stats %s" % err
        traceback.print_exc()
    return lstats

def push_to_influx(influx, cfg, stats):
    allstats = []
    profile = cfg['profile']
    try:
        uplink_port_stats = stats[cfg['uplink_port_index']]
        allstats += [to_measurement(profile,
                                    "trex_ports_uplink_%s" % k,
                                    uplink_port_stats[k])\
                     for k in uplink_port_stats.keys()]
    except Exception, err:
        print "Error getting uplink port stats %s" % err

    try:
        tunnel_port_stats = stats[cfg['tunnel_port_index']]
        allstats += [to_measurement(profile,
                                    "trex_ports_tunnel_%s" % k,
                                    tunnel_port_stats[k])\
                     for k in tunnel_port_stats.keys()]
    except Exception, err:
        print "Error getting tunnel port stats %s" % err

    allstats += get_latency_stats(profile,
                                  stats, cfg['uplink_stream_id'],
                                  "trex_streams_uplink")
    allstats += get_latency_stats(profile,
                                  stats, cfg['tunnel_stream_id'],
                                  "trex_streams_tunnel")
    influx.write_points(allstats)


def maybe_to_float(value):
    try:
        return float(value)
    except Exception:
        return 0.0

def to_measurement(profile, name, value):
    return { 'measurement': name,
             'tags': { 'profile' : profile },
             'fields': {'value': maybe_to_float(value) }}

