import time

from emu.avc_ipfix_fields import AvcIpfixFields
from emu.ipfix_profile import IpfixGenerators


class AVCGenerators(IpfixGenerators):
    def __init__(self, gen_names_list = None):
        fields = AvcIpfixFields()
        super(AVCGenerators, self).__init__(fields, gen_names_list)
        self.__add_generators()

    def __add_generators(self):
        self._generators["256"] = {
            "name": "256",
            "auto_start": True,
            "rate_pps": 0.1,
            "data_records_num": 0,
            "template_id": 256,
            "is_options_template": True,
            "scope_count": 1,
            "fields": [
                #self._fields.get("ingressInterface"),
                self._fields.get("interfaceName"),
                self._fields.get("interfaceDescription"),
            ],
            "engines": [
                {
                    "engine_name": "ingressInterface",
                    "engine_type": "uint",
                    "params": {
                        "size": 1,
                        "offset": 3,
                        "min": 1,
                        "max": 10,
                        "step": 1,
                        "op": "inc",
                    },
                },
                {
                    "engine_name": "interfaceName",
                    "engine_type": "uint",
                    "params": {
                        "size": 1,
                        "offset": 6,
                        "min": 0x30,
                        "max": 0x39,
                        "step": 1,
                        "op": "inc",
                        "init": 0x30,
                    },
                },
                {
                    "engine_name": "interfaceDescription",
                    "engine_type": "uint",
                    "params": {
                        "size": 1,
                        "offset": 22,
                        "min": 0x30,
                        "max": 0x39,
                        "step": 1,
                        "op": "inc",
                        "init": 0x30,
                    },
                },
            ],
        }

        self._generators["257"] = {
            "name": "257",
            "auto_start": True,
            "rate_pps": 0.1,
            "data_records_num": 3,
            "template_id": 257,
            "is_options_template": True,
            "scope_count": 1,
            "fields": [
                self._fields.get("ingressVRFID"),
                self._fields.get("VRFname"),  # when we support string engines we shall add an engine here.
            ],
            "engines": [
                {
                    "engine_name": "ingressVRFID",
                    "engine_type": "uint_list",
                    "params": {
                        "size": 4,
                        "offset": 0,
                        "op": "inc",
                        "list": [1, 2, 8191],
                    },
                },
                {
                    "engine_name": "VRFname",
                    "engine_type": "string_list",
                    "params": {
                        "size": 32,
                        "offset": 0,
                        "op": "inc",
                        "list": ["Mgmt-Intf", "FNF", "__Platform_iVRF:_ID00_"],
                    },
                },
            ],
        }

        self._generators["258"] = {
            "name": "258",
            "auto_start": True,
            "rate_pps": 0.1,
            "data_records_num": 2,
            "template_id": 258,
            "is_options_template": True,
            "scope_count": 1,
            "fields": [
                self._fields.get("samplerId"),
                self._fields.get("samplerName"),
                self._fields.get("samplerMode"),
                self._fields.get("samplerRandomInterval"),
            ],
            "engines": [
                {
                    "engine_name": "samplerId",
                    "engine_type": "uint",
                    "params": {
                        "size": 1,
                        "offset": 3,
                        "min": 2,
                        "max": 4,
                        "step": 2,
                        "op": "inc",
                    },
                },
                {
                    "engine_name": "samplerMode",
                    "engine_type": "uint",
                    "params": {
                        "size": 1,
                        "offset": 0,
                        "min": 1,
                        "max": 2,
                        "op": "rand",
                    },
                },
                {
                    "engine_name": "samplerRandomInterval",
                    "engine_type": "uint",
                    "params": {
                        "size": 2,
                        "offset": 0,
                        "min": 0,
                        "max": 0xFF,
                        "op": "rand",
                    },
                },
            ],
        }

        self._generators["259"] = {
            "name": "259",
            "auto_start": True,
            "rate_pps": 90,
            "data_records_num": 16,
            "template_id": 259,
            "is_options_template": True,
            "scope_count": 1,
            "fields": [
                self._fields.get("applicationId"),
                self._fields.get("applicationName"),
                self._fields.get("applicationDescription"),
            ],
            "engines": [
                {
                    "engine_name": "applicationId",
                    "engine_type": "uint_list",
                    "params": {
                        "size": 1,
                        "offset": 3,
                        "op": "inc",
                        "list": [
                            0x22,
                            0x6B,
                            0x3D,
                            0x0D,
                            0x68,
                            0x5D,
                            0x0A,
                            0x31,
                            0x4C,
                            0x07,
                            0x3E,
                            0x10,
                            0x6E,
                            0x49,
                            0x48,
                            0x7E,
                        ],
                    },
                },
                {
                    "engine_name": "applicationName",
                    "engine_type": "string_list",
                    "params": {
                        "size": 24,
                        "offset": 0,
                        "op": "inc",
                        "list": [
                            "3pc",
                            "an",
                            "any-host-internal",
                            "argus",
                            "aris",
                            "ax25",
                            "bbnrccmon",
                            "bna",
                            "br-sat-mon",
                            "cbt",
                            "cftp",
                            "chaos",
                            "compaq-peer",
                            "cphb",
                            "cpnx",
                            "crtp",
                        ],
                    },
                },
                {
                    "engine_name": "applicationDescription",
                    "engine_type": "string_list",
                    "params": {
                        "size": 55,
                        "offset": 0,
                        "op": "inc",
                        "list": [
                            "Third Party Connect Protocol",
                            "Active Networks",
                            "any host internal protocol",
                            "ARGUS",
                            "ARIS",
                            "AX.25 Frames",
                            "BBN RCC Monitoring",
                            "BNA",
                            "Backroom SATNET Monitoring",
                            "CBT",
                            "CFTP",
                            "CHAOS",
                            "Compaq Peer Protocol",
                            "Computer Protocol Heart Beat",
                            "Computer Protocol Network Executive",
                            "Combat Radio Protocol Transport Protocol",
                        ],
                    },
                },
            ],
        }

        self._generators["260"] = {
            "name": "260",
            "auto_start": True,
            "rate_pps": 290,
            "data_records_num": 5,
            "template_id": 260,
            "is_options_template": True,
            "scope_count": 1,
            "fields": [
                self._fields.get("applicationId"),
                self._fields.get("applicationCategoryName"),
                self._fields.get("applicationSubCategoryName"),
                self._fields.get("applicationGroupName"),
                self._fields.get("unknownNameField"),
                self._fields.get("p2pTechnology"),
                self._fields.get("tunnelTechnology"),
                self._fields.get("encryptedTechnology"),
                self._fields.get("unknownNameField2"),
                self._fields.get("unknownNameField3"),
            ],
            "engines": [
                {
                    "engine_name": "applicationId",
                    "engine_type": "uint",
                    "params": {
                        "size": 1,
                        "offset": 3,
                        "min": 1,
                        "max": 255,
                        "op": "rand",
                    },
                },
                {
                    "engine_name": "applicationCategoryName",
                    "engine_type": "string_list",
                    "params": {
                        "size": 32,
                        "offset": 0,
                        "op": "rand",
                        "list": [
                            "other",
                            "file-sharing",
                            "browsing",
                            "general-browsing",
                            "net-admin",
                            "database",
                        ],
                    },
                },
                {
                    "engine_name": "tunnelTechnology",
                    "engine_type": "histogram_uint",
                    "params": {
                        "size": 4,
                        "offset": 0,
                        "entries": [
                            {"v": 0x79657300, "prob": 1},  # yes
                            {"v": 0x6E6F0000, "prob": 1},  # no
                        ],
                    },
                },
            ],
        }

        # TCP IPv4
        self._generators["266"] = {
            "name": "266",
            "auto_start": True,
            "rate_pps": 100,
            "data_records_num": 10,
            "template_id": 266,
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
                        "op": "rand",
                        "min": 1,
                        "max": 0xFFFF - 1,
                    },
                },
                {
                    "engine_name": "serverIPv4Address",
                    "engine_type": "uint",
                    "params": {
                        "size": 4,
                        "offset": 0,
                        "op": "rand",
                        "min": 0x30000000,
                        "max": 0x3005FFFF,
                    },
                },
                {
                    "engine_name": "protocolIdentifier",
                    "engine_type": "histogram_uint",
                    "params": {
                        "size": 1,
                        "offset": 0,
                        "entries": [
                            {"v": 6, "prob": 2},  # TCP
                            {"v": 17, "prob": 1},  # UDP
                        ],
                    },
                },
                {
                    "engine_name": "serverTransportProtocol",
                    "engine_type": "histogram_uint",
                    "params": {
                        "size": 2,
                        "offset": 0,
                        "entries": [
                            {"v": 53, "prob": 2},  # DNS
                            {"v": 80, "prob": 3},  # HTTPS
                        ],
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

        # TCP IPv6
        self._generators["267"] = {
            "name": "267",
            "auto_start": True,
            "rate_pps": 0.1,
            "data_records_num": 0,
            "template_id": 267,
            "fields": [
                self._fields.get("clientIPv6Address"),
                self._fields.get("serverIPv6Address"),
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
                    "engine_name": "clientIPv6Address",
                    "engine_type": "uint",
                    "params": {
                        "size": 8,
                        "offset": 8,
                        "op": "rand",
                        "min": 1,
                        "max": 0xFFFFFFFFFFFFFFFF,
                    },
                },
                {
                    "engine_name": "serverIPv6Address",
                    "engine_type": "uint",
                    "params": {
                        "size": 8,
                        "offset": 8,
                        "op": "rand",
                        "min": 0,
                        "max": 0xFFFFFFFFFFFFFFFF,
                    },
                },
                {
                    "engine_name": "protocolIdentifier",
                    "engine_type": "histogram_uint",
                    "params": {
                        "size": 1,
                        "offset": 0,
                        "entries": [
                            {"v": 6, "prob": 2},  # TCP
                            {"v": 17, "prob": 1},  # UDP
                        ],
                    },
                },
                {
                    "engine_name": "serverTransportProtocol",
                    "engine_type": "histogram_uint",
                    "params": {
                        "size": 2,
                        "offset": 0,
                        "entries": [
                            {"v": 53, "prob": 2},  # DNS
                            {"v": 80, "prob": 3},  # HTTPS
                        ],
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
                        "ipg_min": 1000,
                        "ipg_max": 2000,
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
                        "duration_min": 10000,
                        "duration_max": 20000,
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

        # RTP IPv4
        self._generators["268"] = {
            "name": "268",
            "auto_start": True,
            "rate_pps": 6,
            "data_records_num": 10,
            "template_id": 268,
            "fields": [
                self._fields.get("sourceIPv4Address"),
                self._fields.get("destinationIPv4Address"),
                self._fields.get("ipVersion"),
                self._fields.get("protocolIdentifier"),
                self._fields.get("sourceTransportPort"),
                self._fields.get("destinationTransportPort"),
                self._fields.get("transportRtpSsrc"),
                self._fields.get("collectTransportRtpPayloadType"),
                self._fields.get("ingressVRFID"),
                self._fields.get("biflowDirection"),
                self._fields.get("observationPointId"),
                self._fields.get("applicationId"),
                self._fields.get("flowDirection"),
                self._fields.get("collectTransportPacketsLostCounter"),
                self._fields.get("octetDeltaCount"),
                self._fields.get("packetDeltaCount"),
                self._fields.get("flowStartMilliseconds"),
                self._fields.get("flowEndMilliseconds"),
                self._fields.get("unknownNameField4"),
            ],
            "engines": [
                {
                    "engine_name": "sourceIPv4Address",
                    "engine_type": "uint",
                    "params": {
                        "size": 4,
                        "offset": 0,
                        "min": 0x300B017F,  # 48.11.1.128
                        "max": 0x300B017F,  # 48.11.1.128
                        "op": "inc",
                    },
                },
                {
                    "engine_name": "destinationIPv4Address",
                    "engine_type": "uint",
                    "params": {
                        "size": 4,
                        "offset": 0,
                        "min": 0x100000D1,  # 16.0.0.209
                        "max": 0x100000D1,  # 16.0.0.209
                        "op": "inc",
                    },
                },
                {
                    "engine_name": "sourceTransportPort",
                    "engine_type": "uint",
                    "params": {
                        "size": 2,
                        "offset": 0,
                        "min": 1024,
                        "max": 0xFFFF,
                        "op": "rand",
                    },
                },
                {
                    "engine_name": "destinationTransportPort",
                    "engine_type": "uint",
                    "params": {
                        "size": 2,
                        "offset": 0,
                        "min": 1024,
                        "max": 0xFFFF,
                        "op": "rand",
                    },
                },
                {
                    "engine_name": "transportRtpSsrc",
                    "engine_type": "uint",
                    "params": {
                        "size": 4,
                        "offset": 0,
                        "min": 0xB57A6710,
                        "max": 0xB57A6710,
                        "op": "inc",
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
                    "engine_name": "applicationId",
                    "engine_type": "uint",
                    "params": {
                        "size": 4,
                        "offset": 0,
                        "min": 0x0D00003D,
                        "max": 0x0D00003D,
                        "op": "inc",
                    },
                },
                {
                    "engine_name": "flowDirection",
                    "engine_type": "uint",
                    "params": {"size": 1, "offset": 0, "min": 0, "max": 1, "op": "inc"},
                },
                {
                    "engine_name": "octetDeltaCount",
                    "engine_type": "uint",
                    "params": {
                        "size": 4,
                        "offset": 4,
                        "op": "rand",
                        "min": 0,
                        "max": 0xFFFFFFFF,
                    },
                },
                {
                    "engine_name": "packetDeltaCount",
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
            ],
        }

        # RTP IPv6
        self._generators["269"] = {
            "name": "269",
            "auto_start": True,
            "rate_pps": 0.1,
            "data_records_num": 10,
            "template_id": 269,
            "fields": [
                self._fields.get("sourceIPv6Address"),
                self._fields.get("destinationIPv6Address"),
                self._fields.get("ipVersion"),
                self._fields.get("protocolIdentifier"),
                self._fields.get("sourceTransportPort"),
                self._fields.get("destinationTransportPort"),
                self._fields.get("transportRtpSsrc"),
                self._fields.get("collectTransportRtpPayloadType"),
                self._fields.get("ingressVRFID"),
                self._fields.get("biflowDirection"),
                self._fields.get("observationPointId"),
                self._fields.get("applicationId"),
                self._fields.get("flowDirection"),
                self._fields.get("collectTransportPacketsLostCounter"),
                self._fields.get("octetDeltaCount"),
                self._fields.get("packetDeltaCount"),
                self._fields.get("flowStartMilliseconds"),
                self._fields.get("flowEndMilliseconds"),
                self._fields.get("unknownNameField4"),
            ],
            "engines": [
                # simple bi dir
                {
                    "engine_name": "sourceIPv6Address",
                    "engine_type": "uint_list",
                    "params": {"size": 8, "offset": 8, "list": [1, 2], "op": "inc"},
                },
                {
                    "engine_name": "destinationIPv6Address",
                    "engine_type": "uint_list",
                    "params": {"size": 8, "offset": 8, "list": [2, 1], "op": "inc"},
                },
                {
                    "engine_name": "ipVersion",
                    "engine_type": "uint",
                    "params": {"size": 1, "offset": 0, "min": 6, "max": 6, "op": "inc"},
                },
                {
                    "engine_name": "sourceTransportPort",
                    "engine_type": "uint_list",
                    "params": {
                        "size": 2,
                        "offset": 0,
                        "list": [50302, 500],
                        "op": "inc",
                    },
                },
                {
                    "engine_name": "destinationTransportPort",
                    "engine_type": "uint_list",
                    "params": {
                        "size": 2,
                        "offset": 0,
                        "list": [500, 50302],
                        "op": "inc",
                    },
                },
                {
                    "engine_name": "transportRtpSsrc",
                    "engine_type": "uint",
                    "params": {
                        "size": 4,
                        "offset": 0,
                        "min": 0xB57A6710,
                        "max": 0xB57A6710,
                        "op": "inc",
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
                    "engine_name": "applicationId",
                    "engine_type": "uint",
                    "params": {
                        "size": 4,
                        "offset": 0,
                        "min": 0x0D00003D,
                        "max": 0x0D00003D,
                        "op": "inc",
                    },
                },
                {
                    "engine_name": "flowDirection",
                    "engine_type": "uint",
                    "params": {"size": 1, "offset": 0, "min": 0, "max": 1, "op": "inc"},
                },
                {
                    "engine_name": "octetDeltaCount",
                    "engine_type": "uint",
                    "params": {
                        "size": 4,
                        "offset": 4,
                        "op": "rand",
                        "min": 0,
                        "max": 0xFFFFFFFF,
                    },
                },
                {
                    "engine_name": "packetDeltaCount",
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
            ],
        }

        self._generators["dnac-scale-test"] = {
            "name": "dnac-scale-test",
            "auto_start": True,
            "rate_pps": 100,
            "data_records_num": 10,
            "template_id": 270,
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
                    "engine_type": "uint",
                    "params": {
                        "size": 4,
                        "offset": 0,
                        "op": "inc",
                        "min": 0xD0000002,
                        "max": 0xD000002A,
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
                    "engine_type": "histogram_uint",
                    "params": {
                        "size": 2,
                        "offset": 0,
                        "entries": [
                            {"v": 53, "prob": 2},  # DNS
                            {"v": 80, "prob": 3},  # HTTPS
                        ],
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

