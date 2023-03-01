import argparse
from emu.avc_ipfix_generators import AVCGenerators
from emu.ipfix_profile import *
from trex.emu.api import *


class AVCProfiles:
    def __init__(self):
        self.plugins = {}

    def register_domain_id(
        self,
        domain_id,
        generators_names,
        dst_url = None,
        exporter_params = None,
        data_rate = None,
        template_rate = None,
        records_per_packet = None,
        max_template_records = 0,
        max_data_records = 0,
        max_time = None,
        auto_start = None
    ):
        if not dst_url and not exporter_params:
            raise TRexError(f"Failed to register domain id {domain_id} avc profile - dst url is missing")

        generators = AVCGenerators(generators_names)
        generators.set_data_rate_all(data_rate)
        generators.set_template_rate_all(template_rate)
        generators.set_records_per_packet_all(records_per_packet)

        if exporter_params is None:
            exporter_params = IpfixExporterParamsFactory().create_obj_from_dst_url(dst_url)

        ipfix_plugin = IpfixPlugin(exporter_params, generators)
        ipfix_plugin.set_domain_id(domain_id)
        ipfix_plugin.set_max_template_records(max_template_records)
        ipfix_plugin.set_max_data_records(max_data_records)
        ipfix_plugin.set_max_time(max_time)
        ipfix_plugin.set_auto_start(auto_start)

        self.plugins[domain_id] = ipfix_plugin

    def get_registered_domain_ids(self):
        return self.plugins.keys()

    def get_ipfix_plugin_json(self, domain_id):
        plugin = self.plugins.get(domain_id, None)
        if plugin is not None:
            return plugin.get_json()
        else:
            raise TRexError(f"Plugin with domain id {domain_id} not found in plugins database.")

    def get_profile(self, domain_id, **kwargs):
        plugin = self.plugins.get(domain_id, None)
        if plugin is None:
            raise TRexError(f"Profile with domain id {domain_id} not found in profiles database.")

        profile = IpfixProfile(ipfix_plugin=plugin, **kwargs)

        return profile

    def get_devices_auto_trigger_profile(self, domain_id, **kwargs):
        plugin = self.plugins.get(domain_id, None)
        if plugin is None:
            raise TRexError(f"Profile with domain id {domain_id} not found in profiles database.")


        profile = IpfixDevicesAutoTriggerProfile(ipfix_plugin=plugin, **kwargs)

        return profile


class Prof1:
    def __init__(self):
        self.avc_profiles = AVCProfiles()

    def register_profiles(self, dst_url, exporter_params, max_tmp_rec, max_data_rec):
        self.avc_profiles.register_domain_id(6,    ["256", "257", "258", "259", "260"], dst_url, exporter_params, None, None, None, max_tmp_rec, max_data_rec)
        self.avc_profiles.register_domain_id(256,  ["266"], dst_url, exporter_params, None, None, None, max_tmp_rec, max_data_rec)
        self.avc_profiles.register_domain_id(512,  ["267"], dst_url, exporter_params, None, None, None, max_tmp_rec, max_data_rec)
        self.avc_profiles.register_domain_id(768,  ["268"], dst_url, exporter_params, None, None, None, max_tmp_rec, max_data_rec)
        self.avc_profiles.register_domain_id(1024, ["269"], dst_url, exporter_params, None, None, None, max_tmp_rec, max_data_rec)

    def get_profile(self, tuneables):
        # Argparse for tunables
        parser = argparse.ArgumentParser(
            description="Argparser for IPFIX devices auto generation (triggering)",
            formatter_class=argparse.ArgumentDefaultsHelpFormatter,
        )

        parser.add_argument("--domain-id", type=int, default=256, dest="domain_id", help="Domain ID. Registered domain IDs are {6, 256, 512, 768, 1024}")
        parser.add_argument("--dst-url", type=str, default="udp://127.0.0.1:18088", dest="dst_url", help="Exporter destination URL")
        parser.add_argument("--device-mac", type=str, default="00:00:00:70:00:03", dest="device_mac", help="Mac address of the first device to be generated")
        parser.add_argument("--device-ipv4", type=str, default="1.1.1.3", dest="device_ipv4", help="IPv4 address of the first device to be generated")
        parser.add_argument("--devices-num", type=int, default=1, dest="devices_num", help="Number of devices to generate")
        parser.add_argument("--rampup-time", type=str, default=None, dest="rampup_time", help="The time duration over which the devices should be generated")
        parser.add_argument("--sites-per-tenant", type=str, default=None, dest="sites_per_tenant", help="Number of sites per tenant")
        parser.add_argument("--devices-per-site", type=str, default=None, dest="devices_per_site", help="Number of devices per site")
        parser.add_argument("--total-rate-pps", type=int, default=None, dest="total_rate_pps", help="The total rate (pps) in which all devices export IPFIX packets")
        parser.add_argument("--max-template-records", type=int, default=None, dest="max_template_records", help="Max template records to send. Value of 0 means no limit")
        parser.add_argument("--max-data-records", type=int, default=None, dest="max_data_records", help="Max data records to send. Value of 0 means no limit")
        parser.add_argument("--exporter-max-interval", type=str, default=None, dest="exporter_max_interval", help="HTTP exporter max interval to wait between two consecutive posts")
        parser.add_argument("--exporter-max-size", type=int, default=None, dest="exporter_max_size", help="HTTP exporter max size of files to export")
        parser.add_argument("--exporter-max-posts", type=int, default=None, dest="exporter_max_posts", help="HTTP exporter max posts to export")
        parser.add_argument('--exporter-compressed', default=None, dest="exporter_compressed", action=argparse.BooleanOptionalAction, help="HTTP exporter store exported files on disk")
        parser.add_argument('--exporter-store-exported-files-on-disk', default=None, dest="exporter_store_exported_files_on_disk", action=argparse.BooleanOptionalAction, help="HTTP exporter store exported files on disk")

        args = parser.parse_args(tuneables)

        exporter_params = IpfixExporterParamsFactory().create_obj_from_dst_url(args.dst_url)
        if exporter_params.get_type() == "http":
            exporter_params.set_max_interval(args.exporter_max_interval)
            exporter_params.set_max_size(args.exporter_max_size)
            exporter_params.set_max_posts(args.exporter_max_posts)
            exporter_params.set_compress(args.exporter_compressed)
            exporter_params.set_store_exported_files_on_disk(args.exporter_store_exported_files_on_disk)

        self.register_profiles(
            args.dst_url,
            exporter_params,
            args.max_template_records,
            args.max_data_records)

        domain_ids = self.avc_profiles.get_registered_domain_ids()

        if args.domain_id not in domain_ids:
            raise TRexError("Invalid domain id {}. Domain id should be in {}".format(args.domain_id, domain_ids))

        return self.avc_profiles.get_devices_auto_trigger_profile(
            args.domain_id, 
            device_mac = args.device_mac,
            device_ipv4 = args.device_ipv4,
            devices_num = args.devices_num,
            rampup_time = args.rampup_time,
            sites_per_tenant = args.sites_per_tenant,
            devices_per_site = args.devices_per_site,
            total_rate_pps = args.total_rate_pps
        ).get_profile()

def register():
    return Prof1()
