import xml.etree.ElementTree as ET
from collections import defaultdict

from emu.avc_ipfix_fields import AvcIpfixFields
from emu.ipfix_profile import *
from trex.emu.api import *

PPACK_TAXONOMY_FILE = "./emu/taxonomy-pp63.xml"

class PpackTaxonomy:
    class ProtocolInfo:
        def __init__(
            self,
            name,
            engine_id,
            selector_id,
            help_string,
            attributes
        ):
            self.name = name
            self.engine_id = engine_id
            self.selector_id = selector_id
            self.help_string = help_string
            self.attributes = attributes
            self.application_id = int(engine_id) << 24 | int(selector_id)

        def __str__(self):
            s = f"name: {self.name}\n" + \
                f"application_id: {self.application_id} [{self.engine_id}:{self.selector_id}]\n" + \
                f"help_string: {self.help_string}\n"
            return s

    def __init__(self, ppack_taxonomy_file = PPACK_TAXONOMY_FILE):
        self._ppack_taxonomy_file = ppack_taxonomy_file
        self._protocols = defaultdict(None)
        self._br_protocols = defaultdict(None)
        self._parse()

    def get_protocol_info(self, name):
        return self._protocols[name]

    def get_protocols_num(self):
        return len(self._protocols)

    def get_br_protocols(self):
        return self._br_protocols

    def _parse(self):
        mytree = ET.parse(PPACK_TAXONOMY_FILE)
        myroot = mytree.getroot()
        for protocol in myroot.iter("protocol"):
            name = protocol.find("name").text
            br_attr = protocol.find("attributes").find("business-relevance").text
            protocolInfo = PpackTaxonomy.ProtocolInfo(
                name,
                engine_id = protocol.find("engine-id").text,
                selector_id = protocol.find("selector-id").text,
                help_string = protocol.find("help-string").text,
                attributes = protocol.find("attributes"))
            self._protocols[name] = protocolInfo
            if br_attr == "business-relevant":
                self._br_protocols[name] = protocolInfo

class DnacScaleTestGenerators(IpfixGenerators):
    def __init__(self, gen_names_list = None, apps_per_client = 41):
        fields = AvcIpfixFields()
        super(DnacScaleTestGenerators, self).__init__(fields, gen_names_list)
        self._ppackTaxonomy = PpackTaxonomy()
        # Number of business-relevant protocols
        num_br = len(self._ppackTaxonomy.get_br_protocols())
        if apps_per_client > num_br:
            apps_per_client = num_br
        self._apps_per_client = apps_per_client
        self._br_protocols = [protocol for protocol in self._ppackTaxonomy.get_br_protocols().values()][:self._apps_per_client]
        self._add_generators()

    def _add_generators(self):
        self._generators["256-data-tcp-5t"] = {
            "name": "256-data-tcp-5t",
            "auto_start": True,
            "rate_pps": 100,
            "data_records_num": 10,
            "template_id": 256,
            "fields": [
                self._fields.get("clientIPv4Address"),
                self._fields.get("serverIPv4Address"),
                self._fields.get("ipVersion"),
                self._fields.get("protocolIdentifier"),
                self._fields.get("serverTransportProtocol"),
                self._fields.get("ingressVRFID"),
                self._fields.get("biflowDirection"),
                self._fields.get("observationPointId"),
                self._fields.get("applicationId"),
                self._fields.get("flowDirection"),
                self._fields.get("flowStartMilliseconds"),
                self._fields.get("flowEndMilliseconds"),
                self._fields.get("newConnectionDeltaCount"),
                self._fields.get("numRespsCountDelta"),
                self._fields.get("sumServerNwkTime"),
                self._fields.get("retransPackets"),
                self._fields.get("sumNwkTime"),
                self._fields.get("sumServerRespTime"),
                self._fields.get("responderPackets"),
                self._fields.get("initiatorPackets"),
                self._fields.get("ARTServerRetransmissionsPackets"),
                self._fields.get("serverBytesL3"),
                self._fields.get("clientBytesL3"),
            ],
            "engines": [
                {
                    "engine_name": "clientIPv4Address",
                    "engine_type": "uint",
                    "params": {
                        "size": 2,
                        "offset": 2,
                        "op": "inc",
                        "repeat": 41,
                        "min": 1,
                        "max": 48,
                    },
                },
                {
                    "engine_name": "serverIPv4Address",
                    "engine_type": "uint",
                    "params": {
                        "size": 4,
                        "offset": 0,
                        "op": "inc",
                        "min": 1,
                        "max": 48,
                    },
                },
                {
                    "engine_name": "applicationId",
                    "engine_type": "uint_list",
                    "params": {
                        "size": 4,
                        "offset": 0,
                        "op": "inc",
                        "list": [protocol.application_id for protocol in self._br_protocols]
                    },
                },
                {
                    "engine_name": "protocolIdentifier",
                    "engine_type": "histogram_uint",
                    "params": {
                        "size": 1,
                        "offset": 0,
                        "entries": [
                            {"v": 17, "prob": 1},  # UDP
                        ],
                    },
                },
                {
                    "engine_name": "serverTransportProtocol",
                    "engine_type": "uint",
                    "params": {
                        "size": 2,
                        "offset": 0,
                        "op": "inc",
                        "min": 80,
                        "max": 80,
                    },
                },
                {
                    "engine_name": "observationPointId",
                    "engine_type": "histogram_uint64",
                    "params": {
                        "size": 8,
                        "offset": 0,
                        "entries": [{"v": 4294967299, "prob": 2}],
                    },
                },
                {
                    "engine_name": "flowStartMilliseconds",
                    "engine_type": "time_start",
                    "params": {
                        "size": 8,
                        "offset": 0,
                        "time_end_engine_name": "flowEndMilliseconds",
                        "ipg_min": 10000,
                        "ipg_max": 20000,
                        "time_offset": int(time.time() * 1000),  # Unix Time in milliseconds
                    },
                },
                {
                    "engine_name": "flowEndMilliseconds",
                    "engine_type": "time_end",
                    "params": {
                        "size": 8,
                        "offset": 0,
                        "time_start_engine_name": "flowStartMilliseconds",
                        "duration_min": 5000,
                        "duration_max": 10000,
                    },
                },
                {
                    "engine_name": "newConnectionDeltaCount",
                    "engine_type": "uint",
                    "params": {
                        "size": 1,
                        "offset": 3,
                        "op": "inc",
                        "min": 1,
                        "max": 2,
                        "step": 1,
                    },
                },
                {
                    "engine_name": "responderPackets",
                    "engine_type": "uint",
                    "params": {
                        "size": 1,
                        "offset": 7,
                        "op": "rand",
                        "min": 1,
                        "max": 255,
                    },
                },
                {
                    "engine_name": "initiatorPackets",
                    "engine_type": "uint",
                    "params": {
                        "size": 1,
                        "offset": 7,
                        "op": "rand",
                        "min": 1,
                        "max": 255,
                    },
                },
                {
                    "engine_name": "serverBytesL3",
                    "engine_type": "uint",
                    "params": {
                        "size": 2,
                        "offset": 6,
                        "op": "rand",
                        "min": 0,
                        "max": 0xFFFF,
                    },
                },
                {
                    "engine_name": "clientBytesL3",
                    "engine_type": "uint",
                    "params": {
                        "size": 2,
                        "offset": 6,
                        "op": "rand",
                        "min": 0,
                        "max": 0xFFFF,
                    },
                },
            ],
        }

        self._generators["257-options-applications"] = {
            "name": "257-options-template",
            "auto_start": True,
            "template_rate_pps": 0.01,
            "rate_pps": 0.01,
            "data_records_num": 50,
            "template_id": 257,
            "is_options_template": True,
            "scope_count": 1,
            "fields": [
                self._fields["applicationId"],
                self._fields["applicationName"],
                self._fields["applicationDescription"],
            ],
            "engines": [
                {
                    "engine_name": "applicationId",
                    "engine_type": "uint_list",
                    "params": {
                        "size": 4,
                        "offset": 0,
                        "op": "inc",
                        "list": [protocol.application_id for protocol in self._br_protocols]
                    },
                },
                {
                    "engine_name": "applicationName",
                    "engine_type": "string_list",
                    "params": {
                        "size": 24,
                        "offset": 0,
                        "op": "inc",
                        "list": [protocol.name for protocol in self._br_protocols]
                    }
                },
                {
                    "engine_name": "applicationDescription",
                    "engine_type": "string_list",
                    "params": {
                        "size": 55,
                        "offset": 0,
                        "op": "inc",
                        "list": [protocol.help_string[:55] for protocol in self._br_protocols]
                    },
                },
            ],
        }

        self._generators["258-options-attributes"] = {
            "name": "258-options-template",
            "auto_start": True,
            "template_rate_pps": 0.01,
            "rate_pps": 0.01,
            "template_id": 258,
            "data_records_num": 50,
            "is_options_template": True,
            "scope_count": 1,
            "fields": [
                self._fields["applicationId"],
                self._fields["applicationCategoryName"],
                self._fields["applicationSubCategoryName"],
                self._fields["applicationGroupName"],
                self._fields["applicationTrafficClass"],
                self._fields["applicationBusinessRelevance"],
                self._fields["p2pTechnology"],
                self._fields["tunnelTechnology"],
                self._fields["encryptedTechnology"],
                self._fields["applicationSetName"],
                self._fields["applicationFamilyName"],
            ],
            "engines": [
                {
                    "engine_name": "applicationId",
                    "engine_type": "uint_list",
                    "params": {
                        "size": 4,
                        "offset": 0,
                        "op": "inc",
                        "list": [protocol.application_id for protocol in self._br_protocols]
                    },
                },
                {
                    "engine_name": "applicationBusinessRelevance",
                    "engine_type": "string_list",
                    "params": {
                        "size": 32,
                        "offset": 0,
                        "op": "inc",
                        "list": [protocol.attributes.find("business-relevance").text for protocol in self._br_protocols]
                    },
                }
            ]
        }

