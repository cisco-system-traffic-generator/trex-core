import argparse
import stf_path
from trex_stf_lib.trex_client import CTRexClient
from pprint import pprint

# sample TRex stateful run
# assuming server daemon is running.

def minimal_stateful_test(server):
    print('Connecting to %s' % server)
    trex_client = CTRexClient(server)

    print('Connected, start TRex')
    trex_client.start_trex(
            c = 1,
            m = 700,
            f = 'cap2/http_simple.yaml',
            d = 30,
            l = 1000,
            )

    print('Sample until end')
    result = trex_client.sample_to_run_finish()

    print('Test results:')
    print(result)

    print('TX by ports:')
    tx_ptks_dict = result.get_last_value('trex-global.data', 'opackets-*')
    print('  |  '.join(['%s: %s' % (k.split('-')[-1], tx_ptks_dict[k]) for k in sorted(tx_ptks_dict.keys())]))

    print('RX by ports:')
    rx_ptks_dict = result.get_last_value('trex-global.data', 'ipackets-*')
    print('  |  '.join(['%s: %s' % (k.split('-')[-1], rx_ptks_dict[k]) for k in sorted(rx_ptks_dict.keys())]))

    print('CPU utilization:')
    print(result.get_value_list('trex-global.data.m_cpu_util'))

    #Dump of *latest* result sample, uncomment to see it all
    #print('Latest result dump:')
    #pprint(result.get_latest_dump())


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Example for TRex Stateful, assuming server daemon is running.")
    parser.add_argument('-s', '--server',
                        dest='server',
                        help='Remote trex address',
                        default='127.0.0.1',
                        type = str)
    args = parser.parse_args()

    minimal_stateful_test(args.server)

