import unittest
import argparse
import json
from urllib.parse import urlparse
import re
from trex.emu.api import *

class Singleton(type):
    _instances = {}

    def __call__(cls, *args, **kwargs):
        if cls not in cls._instances:
            cls._instances[cls] = super(Singleton, cls).__call__(*args, **kwargs)
        return cls._instances[cls]


def add_to_json_if_not_none(json, field, value):
    if value != None:
        json[field] = value


class IpfixFields:
    def __init__(self):
        self._fields = {} # Field name to configuration
        self.CISCO_PEN = 9

    def __getitem__(self, field):
        return self._fields[field]

    def get(self, field, default = None):
        return self._fields.get(field, default)

class IpfixGenerators:
    def __init__(self, fields, gen_names_list = None):
        self._fields = fields
        self._gen_names_list = gen_names_list
        self._generators = {} # Generator name to configuration

    def __getitem__(self, gen_name):
        return self._generators[gen_name]

    def get(self, gen_name, default = None):
        return self._generators.get(gen_name, default)

    def get_all(self):
        if self._gen_names_list:
            gen_list = [self._generators[gen_name] for gen_name in self._gen_names_list]
        else:
            gen_names = sorted(self._generators.keys())
            gen_list = [self._generators[gen_name] for gen_name in gen_names]

        return gen_list

    def set_data_rate(self, gen_name, rate):
        if rate is None:
            return

        gen = self._generators.get(gen_name)
        if gen:
            gen["rate_pps"] = rate

    def set_data_rate_all(self, rate):
        for gen_name in self._generators:
            self.set_data_rate(gen_name, rate)

    def set_template_rate(self, gen_name, rate):
        if rate is None:
            return

        gen = self._generators.get(gen_name)
        if gen:
            gen["template_rate_pps"] = rate

    def set_template_rate_all(self, rate):
        for gen_name in self._generators:
                self.set_template_rate(gen_name, rate)

    def set_records_per_packet(self, gen_name, records):
        if records is None:
            return

        gen = self._generators.get(gen_name)
        if gen:
            gen["data_records_num"] = records

    def set_records_per_packet_all(self, records):
        for gen_name in self._generators:
                self.set_records_per_packet(gen_name, records)


class IpfixExporterParamsFactory(metaclass=Singleton):
    def __init__(self):
        self._create_obj_db()

    def create_obj_from_dst_url(self, dst_url):
        dst_url = self._escape_specifiers(dst_url)
        url = urlparse(dst_url)
        if url.scheme in self.obj_db:
            return self.obj_db[url.scheme](url)
        else:
            return None

    def _create_obj_db(self):
        self.obj_db = {}
        self.obj_db["emu-udp"] = self._create_emu_udp_obj
        self.obj_db["udp"] = self._create_udp_obj
        self.obj_db["file"] = self._create_file_obj
        self.obj_db["http"] = self._create_http_obj

    def _create_emu_udp_obj(self, dst_url):
        obj = IpfixUdpExporterParams(dst_url.hostname, dst_url.port)
        return obj

    def _create_udp_obj(self, dst_url):
        obj = IpfixUdpExporterParams(dst_url.hostname, dst_url.port)
        return obj

    def _create_file_obj(self, dst_url):
        re_pattern = r"^(.*/)([^/]*)$"
        match = re.search(re_pattern, dst_url.path)
        if not match:
            return None
        dir = match.groups()[0]
        name = match.groups()[1]

        obj = IpfixFileExporterParams(dir, name)
        return obj

    def _create_http_obj(self, dst_url):
        re_pattern = r"^/api/ipfixfilecollector/([\w-]+)/([\w-]+)/([\w-]+)/post_file$"
        match = re.search(re_pattern, dst_url.path)
        if not match:
            return None

        tenant_id = self._unescape_specifiers(match.groups()[0])
        site_id = self._unescape_specifiers(match.groups()[1])
        device_id = self._unescape_specifiers(match.groups()[2])

        obj = IpfixHttpExporterParams(dst_url.hostname, dst_url.port, tenant_id, site_id, device_id)
        return obj

    def _escape_specifiers(self, dst_url):
        dst_url = dst_url.replace("%t", "円")
        dst_url = dst_url.replace("%s", "冇")
        dst_url = dst_url.replace("%d", "冈")
        dst_url = dst_url.replace("%u", "冉")

        return dst_url

    def _unescape_specifiers(self, dst_url):
        dst_url = dst_url.replace("円", "%t")
        dst_url = dst_url.replace("冇", "%s")
        dst_url = dst_url.replace("冈", "%d")
        dst_url = dst_url.replace("冉", "%u")

        return dst_url


class IpfixExporterParams:
    def __init__(self):
        self._type = None

    def get_type(self):
        return self._type

    def get_json(self):
        return None

    def dump_json(self):
        return json.dumps(self.get_json(), indent=4)

    def get_dst_url(self):
        return None


class IpfixUdpExporterParams(IpfixExporterParams):
    def __init__(self, dst_ip_addr, dst_port):
        super(IpfixUdpExporterParams, self).__init__()
        self._dst_ip_addr = dst_ip_addr
        self._dst_port = dst_port
        self._type = "udp"
        self._use_emu_client_ip_addr = None
        self._raw_socket_interface_name = None

    def get_json(self):
        exporter_params = {}
        add_to_json_if_not_none(exporter_params, "use_emu_client_ip_addr", self._use_emu_client_ip_addr)
        add_to_json_if_not_none(exporter_params, "raw_socket_interface_name", self._raw_socket_interface_name)

        if len(exporter_params) == 0:
            return None
        else:
            return exporter_params

    def get_dst_url(self):
        return f"{self._type}://{self._dst_ip_addr}:{self._dst_port}/"

    def set_use_emu_client_ip_addr(self, use_emu_client_ip_addr):
        self._use_emu_client_ip_addr = use_emu_client_ip_addr

    def set_raw_socket_interface_name(self, raw_socket_interface_name):
        self._raw_socket_interface_name = raw_socket_interface_name


class IpfixFileExporterParams(IpfixExporterParams):
    def __init__(self, dir, name):
        super(IpfixFileExporterParams, self).__init__()
        self._dir = dir
        self._name = name
        self._type = "file"

        self._max_size = None
        self._max_interval = None
        self._max_files = None
        self._compress = None

    def get_type(self):
        return self._type

    def get_json(self):
        exporter_params = {}
        add_to_json_if_not_none(exporter_params, "max_size", self._max_size)
        add_to_json_if_not_none(exporter_params, "max_interval", self._max_interval)
        add_to_json_if_not_none(exporter_params, "max_files", self._max_files)
        add_to_json_if_not_none(exporter_params, "compress", self._compress)

        if len(exporter_params) == 0:
            return None
        else:
            return exporter_params

    def get_dst_url(self):
        return f"{self._type}://{self._dir}/{self._name}"

    def set_max_size(self, max_size):
        self._max_size = max_size

    # max_interval should be given in Golang duration string format (example: "12s", "5m")
    def set_max_interval(self, max_interval):
        self._max_interval = max_interval

    def set_max_files(self, max_files):
        self._max_files = max_files

    def set_compress(self, compress):
        self._compress = compress


class IpfixHttpExporterParams(IpfixFileExporterParams):
    def __init__(self, dst_ip_addr, dst_port, tenant_id, site_id, device_id):
        super(IpfixHttpExporterParams, self).__init__(None, None)
        self._dst_ip_addr = dst_ip_addr
        self._dst_port = dst_port
        self._tenant_id = tenant_id
        self._site_id = site_id
        self._device_id = device_id
        self._max_posts = None
        self._tls_cert_file = None
        self._tls_key_file = None
        self._store_exported_files_on_disk = None
        self._type = "http"

    def get_json(self):
        exporter_params = super(IpfixHttpExporterParams, self).get_json()
        if exporter_params is None:
            exporter_params = {}

        add_to_json_if_not_none(exporter_params, "max_posts", self._max_posts)
        add_to_json_if_not_none(exporter_params, "tls_cert_file", self._tls_cert_file)
        add_to_json_if_not_none(exporter_params, "tls_key_file", self._tls_key_file)
        add_to_json_if_not_none(exporter_params, "store_exported_files_on_disk", self._store_exported_files_on_disk)

        if len(exporter_params) == 0:
            return None
        else:
            return exporter_params

    def get_dst_url(self):
        return f"{self._type}://{self._dst_ip_addr}:{self._dst_port}/api/ipfixfilecollector/{self._tenant_id}/{self._site_id}/{self._device_id}/post_file"

    def set_max_posts(self, max_posts):
        self._max_posts = max_posts

    def set_tls_cert_file(self, tls_cert_file):
        self._tls_cert_file = tls_cert_file

    def set_tls_key_file(self, tls_key_file):
        self._tls_key_file = tls_key_file

    def set_store_exported_files_on_disk(self, store_exported_files_on_disk):
        self._store_exported_files_on_disk = store_exported_files_on_disk


class IpfixPlugin:
    def __init__(self, exporter_params, generators: IpfixGenerators):
        self._exporter_params = exporter_params
        self._generators = generators
        self._NETFLOW_VERSION = 10
        self._domain_id = 1
        self._max_data_records = None
        self._max_template_records = None
        self._max_time = None
        self._auto_start = None

    def get_json(self):
        json = {}
        json["netflow_version"] = self._NETFLOW_VERSION
        json["dst"] = self._exporter_params.get_dst_url()
        json["domain_id"] = self._domain_id
        add_to_json_if_not_none(json, "max_data_records", self._max_data_records)
        add_to_json_if_not_none(json, "max_template_records", self._max_template_records)
        add_to_json_if_not_none(json, "max_time", self._max_time)
        add_to_json_if_not_none(json, "auto_start", self._auto_start)
        exporter_params = self._exporter_params.get_json()
        add_to_json_if_not_none(json, "exporter_params", exporter_params)

        json["generators"] = self._generators.get_all()

        return json

    def dump_json(self):
        return json.dumps(self.get_json(), indent=4)

    def set_domain_id(self, domain_id):
        self._domain_id = domain_id

    def set_max_data_records(self, max_data_records):
        self._max_data_records = max_data_records

    def set_max_template_records(self, max_template_records):
        self._max_template_records = max_template_records

    # max_time should be given in Golang duration string format (example: "12s", "5m")
    def set_max_time(self, max_time):
        self._max_time = max_time

    def set_auto_start(self, auto_start):
        self._auto_start = auto_start


# Basic IPFIX profile with one namespace and one or more devices (EMU clients) with a given IPFIX plugin init JSON.
class IpfixProfile:
    def __init__(
        self,
        # IpfixPlugin object from which clients IPFIX init JSON is created
        ipfix_plugin,
        # Port number of the NS to be created
        ns_port = 0,
        # MAC address of the first device to be created
        device_mac = "00:00:00:70:00:03",
        # IPv4 address of the first device to be created
        device_ipv4 = "1.1.1.3",
        # Number of devices to create
        devices_num = 1
    ):
        self._ipfix_plugin = ipfix_plugin
        self._ns_port = ns_port
        self._device_mac = Mac(device_mac)
        self._device_ipv4 = Ipv4(device_ipv4)
        self._devices_num = devices_num

        self._clients_keys = []
        self._profile = None
        self._create_profile()

    def get_json(self):
        return self.get_profile().to_json()

    def dump_json(self):
        return json.dumps(self.get_json(), indent=4)

    def get_profile(self):
        return self._profile

    def get_devices_keys(self):
        return self._clients_keys

    def _create_profile(self):
        mac = self._device_mac
        ipv4 = self._device_ipv4
        ipv4_dg = Ipv4("1.1.1.1")
        ns_plugins = {}
        clients = []
        client_plugins = {"ipfix": self._ipfix_plugin.get_json()}
        ns_key = EMUNamespaceKey(vport=self._ns_port)
        for i in range(self._devices_num):
            client_key = EMUClientKey(ns_key, mac[i].V())
            client_obj = EMUClientObj(mac=mac[i].V(), ipv4=ipv4[i].V(), ipv4_dg=ipv4_dg.V(), plugs=client_plugins)
            clients.append(client_obj)
            self._clients_keys.append(client_key)
        ns_obj = EMUNamespaceObj(ns_key=ns_key, plugs = ns_plugins, clients=clients)
        profile = EMUProfile(ns=ns_obj, def_ns_plugs=ns_plugins)
        self._profile = profile

# IPFIX NS level profile to automatically trigger the creation of a given number of devices by the EMU server.
# The created devices will be created gradually over the provided ramp-up time.
class IpfixDevicesAutoTriggerProfile:
    def __init__(
        self,
        # IpfixPlugin template from which to create IPFIX plugin init JSON of the clients
        ipfix_plugin,
        # Port number of the NS to be created
        ns_port = 0,
        # MAC address of the first device to be created
        device_mac = "00:00:00:70:00:03",
        # IPv4 address of the first device to be created
        device_ipv4 = "1.1.1.3",
        # Number of devices to automatically create at the EMU server
        devices_num = 1,
        # rampup_time should be given in Golang duration string format (example: "12s", "5m")
        rampup_time = None,
        # Number of sites per tenant
        sites_per_tenant = None,
        # Number of devices per site
        devices_per_site = None,
        # Total rate of the [flows] data records from all devices
        total_rate_pps = None
    ):
        self._ipfix_plugin = ipfix_plugin
        self._ns_port = ns_port
        self._device_mac = Mac(device_mac)
        self._device_ipv4 = Ipv4(device_ipv4)
        self._devices_num = devices_num
        self._rampup_time = rampup_time
        self._sites_per_tenant = sites_per_tenant
        self._devices_per_site = devices_per_site
        self._total_rate_pps = total_rate_pps

        self._clients_generator = None
        self._clients_keys = []
        self._profile = None
        self._create_profile()

    def get_json(self):
        return self.get_profile().to_json()

    def dump_json(self):
        return json.dumps(self.get_json(), indent=4)

    def get_profile(self):
        return self._profile

    def get_devices_keys(self):
            return self._clients_keys

    def set_clients_generator(
        self,
        client_ipv4,
        clients_per_device,
        data_records_per_client,
        client_ipv4_field_name = None
    ):
        json = {}
        json["client_ipv4"] = Ipv4(client_ipv4)[0].V()
        json["clients_per_device"] = clients_per_device
        json["data_records_per_client"] = data_records_per_client
        add_to_json_if_not_none(json, "client_ipv4_field_name", client_ipv4_field_name)
        self._clients_generator = json

        self._create_profile()

    def _create_profile(self):
        ns_plugins = {"ipfix" : self._create_ns_ipfix_plugin_json()}
        ns_key = EMUNamespaceKey(vport=self._ns_port)
        ns_obj = EMUNamespaceObj(ns_key=ns_key, plugs = ns_plugins, clients=[])
        profile = EMUProfile(ns=ns_obj, def_ns_plugs=None)
        self._profile = profile

        # Create the list of device keys which will be auto triggered at the EMU server
        for i in range(self._devices_num):
            client_key = EMUClientKey(ns_key, self._device_mac[i].V())
            self._clients_keys.append(client_key)

    def _create_ns_ipfix_plugin_json(self):
        json = {}
        json["device_mac"] = self._device_mac[0].V()
        json["device_ipv4"] = self._device_ipv4[0].V()
        json["devices_num"] = self._devices_num
        add_to_json_if_not_none(json, "rampup_time", self._rampup_time)
        add_to_json_if_not_none(json, "sites_per_tenant", self._sites_per_tenant)
        add_to_json_if_not_none(json, "devices_per_site", self._devices_per_site)
        add_to_json_if_not_none(json, "total_rate_pps", self._total_rate_pps)
        add_to_json_if_not_none(json, "clients_generator", self._clients_generator)
        json["device_init"] = self._ipfix_plugin.get_json()
        ns_json = {"devices_auto_trigger" : json}
        return ns_json
