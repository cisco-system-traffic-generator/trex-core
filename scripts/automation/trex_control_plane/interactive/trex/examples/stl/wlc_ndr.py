import stl_path
from trex.stl.api import *
from trex.stl.trex_stl_wlc import AP_Manager
from pprint import pprint
import sys
import yaml
import time

with open(sys.argv[1]) as f:
    config_yaml = yaml.load(f.read())

base_data = config_yaml['base']
ports_data = config_yaml['ports']
result_accuracy = config_yaml['result_acc']
total_streams = config_yaml['flows']
drop_sensitivity = 0.1

total_clients = sum([port_data['clients'] for port_data in ports_data.values()])
dst_count = int(total_streams / total_clients)

ap_ports = list(ports_data.keys())

c = STLClient(server = config_yaml['server'])
print('Connecting to TRex')
c.connect()
c.acquire(force = True)
c.stop()
c.remove_all_streams()
#c.remove_all_captures()

m = AP_Manager(c)

def establish_setup():
    m.set_base_values(
        mac = base_data['ap_mac'],
        ip = base_data['ap_ip'],
        client_mac = base_data['client_mac'],
        client_ip = base_data['client_ip'],
        )

    for port_id, port_data in ports_data.items():
        m.init(port_id)
        ap_params = m._gen_ap_params()
        m.create_ap(port_id, *ap_params)
        m.aps[-1].profile_dst_ip = port_data['dst_ip'] # dirty hack
        for _ in range(port_data['clients']):
            client_params = m._gen_client_params()
            m.create_client(*client_params, ap_id = ap_params[0])

    print('Joining APs')
    m.join_aps()

    print('Associating clients')
    m.join_clients()

    #pprint(m.get_info())

def clear_stats():
    c.clear_stats(clear_flow_stats = False, clear_latency_stats = False, clear_xstats = False)

def diff_percent(sent, recv):
    assert sent, 'nothing is sent!'
    return min(100.0, 100.0 * recv / sent)

# returns percent of drops, queue_full and opackets
def get_stats():
    stats = c.get_stats()
    pass_rate = 100
    for port_id, port_data in ports_data.items():
        #print('port%s, o: %s, i: %s' % (port_id, stats[port_id]['opackets'], stats[port_data['dst_port']]['ipackets']))
        pass_rate = min(pass_rate, diff_percent(stats[port_id]['opackets'], stats[port_data['dst_port']]['ipackets']))
    opackets = stats['total']['opackets']
    queuefulls = stats['global']['queue_full']
    return (100 - pass_rate), queuefulls, opackets


def run_iteration(rate, fast_time = 5, slow_time = 55):
    clear_stats()
    c.start(ports = ap_ports, mult = '%s%%' % rate, force = True)
    time.sleep(fast_time) # quick check
    c.stop()
    drop_rate, queuefulls, opackets = get_stats()
    queuefull_rate = queuefulls * 100.0 / opackets
    #print('Stats fast check: %s' % drop_rate)
    if drop_rate > 50 or queuefull_rate > 10:
        return drop_rate, queuefull_rate

    # no failures, slow "deep" check
    clear_stats()
    c.start(ports = ap_ports, mult = '%s%%' % rate, force = True)
    time.sleep(slow_time)
    c.stop()
    drop_rate, queuefulls, opackets = get_stats()
    queuefull_rate = queuefulls * 100.0 / opackets
    #print('Stats slow check: %s' % drop_rate)
    return drop_rate, queuefull_rate


def run_ndr(min_rate, max_rate):
    print('Starting NDR (based on binary search)')
    iteration = 0
    rate = max_rate
    effective_diff = 0.8 # start at 80%
    while max_rate - min_rate > result_accuracy:
        iteration += 1

        # next point is based on drop rate
        rate = min_rate + (max_rate - min_rate) * effective_diff
        print('Iteration #%s: running at rate %g%% (limits - min: %g%%, max: %g%%)' % (iteration, round(rate, 2), round(min_rate, 2), round(max_rate, 2)))
        drop_rate, queuefull_rate = run_iteration(rate)
        if drop_rate > drop_sensitivity or queuefull_rate > 10:
            max_rate = rate # failure, lower max rate
            if drop_rate > 63:
                effective_diff = 0.25
            else:
                effective_diff = 0.5
        else:
            min_rate = rate # success, raise min rate
            effective_diff = 0.5
        print('    ...drop: %g%%' % round(drop_rate, 2))
        if queuefull_rate > 10:
            print('    ...queue full: %g%%' % round(queuefull_rate, 2))
        time.sleep(2)
    return rate

try:

    start_time = time.time()
    establish_setup()
    print('Establish wireless duration: %g(s)' % round(time.time() - start_time, 1))

    # add streams
    for client in m.clients:
        m.add_profile(client, '../../../../stl/imix_wlc.py', src = client.ip, dst = client.ap.profile_dst_ip, dst_count = dst_count)

    # warmup
    start_time = time.time()
    warmup_rate = 1
    clear_stats()
    c.start(ports = ap_ports, mult = '%s%%' % warmup_rate, force = True)
    print('Warmup')
    while True:
        time.sleep(0.5)
        _, _, opackets = get_stats()
        if opackets > total_streams * 1.1:
            break
    c.stop()
    drop_rate, queuefulls, opackets = get_stats()
    drop_rate *= warmup_rate / 100.0
    queuefull_rate = queuefulls * 100.0 / opackets
    assert drop_rate < result_accuracy, 'There is droprate (%s) even at %g%% of linerate!' % (drop_rate, warmup_rate)
    assert queuefull_rate < 10, 'There are too much queue full (%s%%) even at %g%% of linerate!' % (queuefull_rate, warmup_rate)
    print('Warmup done (%gs)' % round(time.time() - start_time, 1))

    start_time = time.time()
    ndr_rate = run_ndr(0.1, 99.9)
    print('----------------\nNDR is at: %g%% (duration: %gs)' % (round(ndr_rate, 2), round(time.time() - start_time, 1)))

except KeyboardInterrupt:
    pass

finally:
    c.stop(ports = ap_ports)
    m.close()
    time.sleep(0.1)
    c.disconnect()



