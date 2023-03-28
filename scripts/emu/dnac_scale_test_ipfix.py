import argparse
from emu.dnac_scale_test_generators import DnacScaleTestGenerators
from trex.emu.trex_emu_ipfix_profile import *
from trex.emu.api import *

DEBUG = False

class Prof1:
    def __init__(self):
        None

    def get_profile(self, tuneables):
        # Argparse for tunables
        parser = argparse.ArgumentParser(
            description="Argparser for IPFIX DNAC scale test",
            formatter_class=argparse.ArgumentDefaultsHelpFormatter,
        )

        parser.add_argument('--dst-ipv4',
                            type=str,
                            default = '127.0.0.1',
                            dest = 'dst_ipv4',
                            help='Ipv4 address of collector')
        parser.add_argument('--dst-port',
                            type=str,
                            default = '18088',
                            dest = 'dst_port',
                            help='Destination port')
        parser.add_argument("--device-mac",
                            type=str,
                            default="00:00:00:70:00:01",
                            dest="device_mac",
                            help="Mac address of the first device to be generated")
        parser.add_argument("--device-ipv4",
                            type=str,
                            default="128.1.1.1",
                            dest="device_ipv4", help="IPv4 address of the first device to be generated")
        parser.add_argument("--devices-num",
                            type=int,
                            default=4000,
                            dest="devices_num",
                            help="Number of devices to generate")
        parser.add_argument("--rampup-time",
                            type=str,
                            default="1s",
                            dest="rampup_time",
                            help="The time duration over which the devices should be generated")
        parser.add_argument("--total-rate-pps",
                            type=int,
                            default=25000,
                            dest="total_rate_pps",
                            help="The total rate (pps) in which all devices export IPFIX packets")
        parser.add_argument("--client-ipv4",
                            type=str,
                            default="1.1.1.1",
                            dest="client_ipv4", 
                            help="The total rate (pps) in which all devices export IPFIX packets")
        parser.add_argument("--clients-per-device",
                            type=int,
                            default=48,
                            dest="clients_per_device",
                            help="The total rate (pps) in which all devices export IPFIX packets")
        parser.add_argument("--apps-per-client",
                            type=int,
                            default=41,
                            dest="apps_per_client",
                            help="The total rate (pps) in which all devices export IPFIX packets")
        parser.add_argument("--exporter-use-emu-client-ip-addr",
                            type=bool,
                            default=None,
                            dest="use_emu_client_ip_addr",
                            help="UDP exporter use EMU client IP addr")
        parser.add_argument("--exporter-raw-socket-interface-name",
                            type=str,
                            default=None,
                            dest="raw_socket_interface_name",
                            help="UDP exporter raw socket interface name to use if use-emu-client-ip-addr is set")

        args = parser.parse_args(tuneables)

        exporter_params = IpfixUdpExporterParams(args.dst_ipv4, args.dst_port)
        if exporter_params is None:
            raise TRexError("Failed to create UDP exporter")

        exporter_params.set_use_emu_client_ip_addr(args.use_emu_client_ip_addr)
        exporter_params.set_raw_socket_interface_name(args.raw_socket_interface_name)

        generators = DnacScaleTestGenerators(["256-data-tcp-5t", "257-options-applications","258-options-attributes"], args.apps_per_client)

        ipfix_plugin = IpfixPlugin(exporter_params, generators)
        ipfix_plugin.set_domain_id(270)

        profile = IpfixDevicesAutoTriggerProfile(
            ipfix_plugin=ipfix_plugin,
            device_mac = args.device_mac,
            device_ipv4 = args.device_ipv4,
            device_domain_id = 270,
            devices_num = args.devices_num,
            rampup_time = args.rampup_time,
            total_rate_pps = args.total_rate_pps)
        profile.set_clients_generator(
            client_ipv4 = args.client_ipv4,
            clients_per_device = args.clients_per_device,
            data_records_per_client = args.apps_per_client)

        if DEBUG:
            print(profile.dump_json())

        return profile.get_profile()

def register():
    return Prof1()
