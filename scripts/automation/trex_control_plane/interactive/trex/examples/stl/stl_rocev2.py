"""Send one finite RoCEv2 profile burst and validate per-QP flow counters."""

import argparse
import json

import stl_path
from trex.stl.api import STLClient, STLProfile, rocev2_stats_expectations, validate_rocev2_stats


def run(args):
    native = getattr(args, 'native', False)
    if not native:
        tunables = ['--stream-mode', 'burst', '--total-pkts', str(args.packets),
                    '--pps', str(args.pps), '--flow-stats', '--qps', str(args.qps),
                    '--ip-version', str(args.ip_version), '--message-size', str(args.message_size),
                    '--mtu', str(args.mtu), '--src-mac', args.src_mac, '--dst-mac', args.dst_mac]
        streams = STLProfile.load_py(args.profile, port_id=args.tx_port, tunables=tunables).get_streams()
        expected = rocev2_stats_expectations(streams)
    ports = sorted(set([args.tx_port, args.rx_port]))
    client = STLClient(server=args.server)
    try:
        client.connect()
        client.acquire(ports=ports)
        client.reset(ports=ports)
        if native:
            installed = client.add_rocev2_streams({
                'stream_mode': 'burst', 'total_pkts': args.packets, 'pps': args.pps,
                'qps': args.qps, 'ip_version': args.ip_version, 'message_size': args.message_size,
                'mtu': args.mtu, 'src_mac': args.src_mac, 'dst_mac': args.dst_mac,
                'flow_stats': True, 'direction': 0,
            }, ports=[args.tx_port])
            expected = installed['expected_stats']
        else:
            client.add_streams(streams, ports=[args.tx_port])
        client.clear_stats(ports=ports)
        client.start(ports=[args.tx_port])
        client.wait_on_traffic(ports=[args.tx_port], timeout=args.timeout, rx_delay_ms=args.rx_delay_ms)
        if native:
            result = client.validate_rocev2_stats(expected, args.tx_port, args.rx_port,
                                                  args.max_loss_pct, args.check_rx_bytes)
        else:
            stats = client.get_rocev2_stats(list(expected))
            result = validate_rocev2_stats(stats, expected, args.tx_port, args.rx_port,
                                          args.max_loss_pct, args.check_rx_bytes)
        warnings = client.get_warnings()
        if warnings:
            result['errors'].extend(str(warning) for warning in warnings)
            result['passed'] = False
        print(json.dumps(result, indent=2, sort_keys=True))
        return 0 if result['passed'] else 1
    finally:
        client.disconnect()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server', default='localhost')
    parser.add_argument('--profile', default='stl/rocev2_gpu.py', help='Path relative to the current directory')
    parser.add_argument('--native', action='store_true', help='Build/install packets through the native server API; no profile file needed')
    parser.add_argument('--tx-port', type=int, default=0)
    parser.add_argument('--rx-port', type=int, default=1)
    parser.add_argument('--packets', type=int, default=100, help='Packets per segment stream')
    parser.add_argument('--pps', type=float, default=100)
    parser.add_argument('--qps', type=int, default=1)
    parser.add_argument('--ip-version', type=int, choices=[4, 6], default=4)
    parser.add_argument('--message-size', type=int, default=4096)
    parser.add_argument('--mtu', type=int, default=9000, help='Frame size including Ethernet FCS')
    parser.add_argument('--src-mac', default='00:00:00:00:01:00')
    parser.add_argument('--dst-mac', default='00:00:00:00:02:00')
    parser.add_argument('--timeout', type=float, default=60)
    parser.add_argument('--rx-delay-ms', type=int, default=1000)
    parser.add_argument('--max-loss-pct', type=float, default=0.0)
    parser.add_argument('--check-rx-bytes', action='store_true')
    args = parser.parse_args()
    try:
        return run(args)
    except Exception as error:
        print(json.dumps({'passed': False, 'errors': [str(error)]}))
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
