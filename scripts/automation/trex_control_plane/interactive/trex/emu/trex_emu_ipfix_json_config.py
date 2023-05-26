import json
import re
from collections import defaultdict
import os
from trex.emu.trex_emu_ipfix_generators import *
from trex.emu.trex_emu_ipfix_profile import *
from trex.emu.api import *

class IpfixProfileJsonConfig:
    def __init__(self, config_file):
        self._config_file = config_file
        self._config_json = defaultdict()
        self._profile = IpfixDevicesAutoTriggerListProfile()
        self._load_config_file()
        self._create_ipfix_profile()

    def get_config_json(self):
        return self._config_json

    def get_profile_json(self):
        return self._profile.get_profile().to_json()

    def dump_profile_json(self):
        return json.dumps(self.get_profile_json(), indent=4)

    def get_profile(self):
        return self._profile.get_profile()

    def _load_config_file(self):
        if not os.path.exists(self._config_file):
            raise TRexError("config file does not exist - {1}".format(self._config_file))

        with open(self._config_file, "r" ) as f:
            config_json = json.load(f)
            self._config_json = config_json

    def _create_ipfix_profile(self):
        if not "device_groups" in self._config_json:
            raise ValueError('"device_groups" list does not exist in config file')

        device_groups = self._config_json["device_groups"]
        for device_group in device_groups:
            if not "exporter_params" in device_group:
                raise ValueError('"exporter_params" does not exist in config file')
            _exporter_params = device_group["exporter_params"]

            if not "dst_url" in _exporter_params:
                raise ValueError('"dst_url" does not exist in config file')

            exporter_params = IpfixExporterParamsFactory().create_obj_from_dst_url(_exporter_params["dst_url"])
            if exporter_params.get_type() == "http":
                exporter_params.set_max_posts(_exporter_params.get("max_posts", None))
                exporter_params.set_tls_cert_file(_exporter_params.get("tls_cert_file", None))
                exporter_params.set_tls_key_file(_exporter_params.get("tls_key_file", None))
                exporter_params.set_store_exported_files_on_disk(_exporter_params.get("store_exported_files_on_disk", None))
                exporter_params.set_input_channel_capacity(_exporter_params.get("input_channel_capacity", None))
                exporter_params.set_repeats_num(_exporter_params.get("repeats_num", None))
                exporter_params.set_repeats_wait_time(_exporter_params.get("repeats_wait_time", None))
            elif exporter_params.get_type() == "udp":
                exporter_params.set_use_emu_client_ip_addr(_exporter_params.get("use_emu_client_ip_addr", None))
                exporter_params.set_raw_socket_interface_name(_exporter_params.get("raw_socket_interface_name", None))
            else:
                raise ValueError("unsupported exporter type - {1}".format(exporter_params.get_type()))

            if not "generators_params" in device_group:
                raise ValueError('"generators_params" does not exist in config file')
            _generators_params = device_group["generators_params"]
            if not "generators" in _generators_params:
                raise ValueError('"generators" list does not exist in config file')
            data_rate = _generators_params.get("data_rate", None)
            template_rate = _generators_params.get("template_rate", None)
            records_per_packet = _generators_params.get("records_per_packet", None)
            generators = AVCGenerators(_generators_params["generators"])
            generators.set_data_rate_all(data_rate)
            generators.set_template_rate_all(template_rate)
            generators.set_records_per_packet_all(records_per_packet)

            if not "plugin_params" in device_group:
                raise ValueError('plugin_params in config file')
            _plugin_params = device_group["plugin_params"]
            domain_id = _plugin_params.get("domain_id", None)
            max_template_records = _plugin_params.get("max_template_records", None)
            max_data_records = _plugin_params.get("max_data_records", None)
            max_time = _plugin_params.get("max_time", None)
            auto_start = _plugin_params.get("auto_start", None)
            plugin = IpfixPlugin(exporter_params, generators)
            plugin.set_domain_id(domain_id)
            plugin.set_max_template_records(max_template_records)
            plugin.set_max_data_records(max_data_records)
            plugin.set_max_time(max_time)
            plugin.set_auto_start(auto_start)

            dat_params = defaultdict()
            dat_params["device_mac"] = device_group.get("device_mac", None)
            dat_params["device_ipv4"] = device_group.get("device_ipv4", None)
            dat_params["devices_num"] = device_group.get("devices_num", None)
            dat_params["rampup_time"] = device_group.get("rampup_time", None)
            dat_params["total_rate_pps"] = device_group.get("total_rate_pps", None)
            apps_per_client = device_group.get("apps_per_client", None)
            client_ipv4 = device_group.get("client_ipv4", None)
            clients_per_device = device_group.get("clients_per_device", None)

            dat_profile = IpfixDevicesAutoTriggerProfile(ipfix_plugin=plugin , **dat_params)
            dat_profile.set_clients_generator(
                client_ipv4 = client_ipv4,
                clients_per_device = clients_per_device,
                data_records_per_client = apps_per_client)

            self._profile.add_devices_auto_trigger_profile(dat_profile)
