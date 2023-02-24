from trex.emu.api import *
import argparse
import time


class AVCFields():
    def __init__(self):
        self.fields_dict = {}
        self.CISCO_PEN = 9
        self.__add_iana_fields()
        self.__add_cisco_fields()

    def __add_iana_fields(self):
        self.fields_dict["protocolIdentifier"] = {
            "name": "protocolIdentifier",
            "type": 0x0004,
            "length": 1,
            "data": [17] # UDP by default
        }

        self.fields_dict["applicationId"] = {
            "name": "applicationId",
            "type": 0x005f,
            "length": 4,
            "data": [1, 0, 0, 1]
        }

        self.fields_dict["sourceIPv4Address"] = {
            "name": "sourceIPv4Address",
            "type": 0x0008,
            "length": 4,
            "data": [48, 0, 0, 1]
        }

        self.fields_dict["destinationIPv4Address"] = {
            "name": "destinationIPv4Address",
            "type": 0x000c,
            "length": 4,
            "data": [16, 0, 0, 1]
        }

        self.fields_dict["applicationName"] = {
            "name": "applicationName",
            "type": 0x0060,
            "length": 24,
            "data": [0x69, 0x63, 0x6d, 0x70] + [0] * 20
        }

        self.fields_dict["applicationDescription"] = {
            "name": "applicationDescription",
            "type": 0x005e,
            "length": 55,
            "data": [0x49, 0x6e, 0x74, 0x65, 0x72, 0x6e, 0x65, 0x74, 0x20, 0x43, 0x6f, 0x6e, 0x74, 0x72, 0x6f, 0x6c,
                     0x20, 0x4d, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65, 0x20, 0x50, 0x72, 0x6f, 0x74, 0x6f, 0x63, 0x6f,
                     0x6c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]
        }

        self.fields_dict["flowStartSysUpTime"] = {
            "name": "flowStartSysUpTime",
            "type": 0x0016,
            "length": 4,
            "data": [0, 0, 0, 1]
        }

        self.fields_dict["flowEndSysUpTime"] = {
            "name": "flowEndSysUpTime",
            "type": 0x0015,
            "length": 4,
            "data": [0, 0, 0, 10]
        }

        self.fields_dict["flowStartMilliseconds"] = {
            "name": "flowStartMilliseconds",
            "type": 0x0098,
            "length": 8,
            "data": [0, 0, 0, 0, 0, 0, 255, 0]
        }

        self.fields_dict["responderPackets"] = {
            "name": "responderPackets",
            "type": 0x012b,
            "length": 8,
            "data": [0, 0, 0, 0, 0, 0, 0, 10]
        }

        self.fields_dict["initiatorPackets"] = {
            "name": "initiatorPackets",
            "type": 0x012a,
            "length": 8,
            "data": [0, 0, 0, 0, 0, 0, 0, 10]
        }

        self.fields_dict["sourceIPv6Address"] = {
            "name": "sourceIPv6Address",
            "type": 0x001b,
            "length": 16,
            "data": [32, 1, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1]
        }

        self.fields_dict["destinationIPv6Address"] = {
            "name": "destinationIPv6Address",
            "type": 0x001c,
            "length": 16,
            "data": [32, 1, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2]
        }

        self.fields_dict["ipVersion"] = {
            "name": "ipVersion",
            "type": 0x003c,
            "length": 1,
            "data": [4] # Version 4
        }

        self.fields_dict["sourceTransportPort"] = {
            "name": "sourceTransportPort",
            "type": 0x0007,
            "length": 2,
            "data": [5, 1]
        }

        self.fields_dict["destinationTransportPort"] = {
            "name": "destinationTransportPort",
            "type": 0x000b,
            "length": 2,
            "data": [0, 53]
        }

        self.fields_dict["ingressVRFID"] = {
            "name": "ingressVRFID",
            "type": 0x00ea,
            "length": 4,
            "data": [0, 0, 0, 0]
        }

        self.fields_dict["VRFname"] = {
            "name": "VRFname",
            "type": 0x00ec,
            "length": 32,
            "data": [0] * 32
        }

        self.fields_dict["biflowDirection"] = {
            "name": "biflowDirection",
            "type": 0x00ef,
            "length": 1,
            "data": [1]
        }

        self.fields_dict["observationPointId"] = {
            "name": "observationPointId",
            "type": 0x008a,
            "length": 8,
            "data": [0, 0, 0, 0, 7, 3, 1, 4]
        }

        self.fields_dict["flowDirection"] = {
            "name": "flowDirection",
            "type": 0x003d,
            "length": 1,
            "data": [1]
        }

        self.fields_dict["octetDeltaCount"] = {
            "name": "octetDeltaCount",
            "type": 0x0001,
            "length": 8,
            "data": [0, 0, 0, 0, 0, 5, 14, 4]
        }

        self.fields_dict["packetDeltaCount"] = {
            "name": "packetDeltaCount",
            "type": 0x0002,
            "length": 8,
            "data": [0, 0, 0, 0, 0, 1, 14, 4]
        }

        self.fields_dict["flowEndMilliseconds"] = {
            "name": "flowEndMilliseconds",
            "type": 0x00099,
            "length": 8,
            "data": [0, 0, 0, 0, 0, 0, 5, 250]
        }

        self.fields_dict["sourceIPv4Address"] = {
            "name": "sourceIPv4Address",
            "type": 0x00008,
            "length": 4,
            "data": [192, 168, 0, 1]
        }

        self.fields_dict["destinationIPv4Address"] = {
            "name": "destinationIPv4Address",
            "type": 0x0000c,
            "length": 4,
            "data": [192, 168, 0, 254]
        }

        self.fields_dict["newConnectionDeltaCount"] = {
            "name": "newConnectionDeltaCount",
            "type": 0x0116,
            "length": 4,
            "data": [0, 0, 0, 20]
        }

        self.fields_dict["ingressInterface"] = {
            "name": "ingressInterface",
            "type": 0x000a,
            "length": 4,
            "data": [0, 0, 0, 0]
        }

        self.fields_dict["interfaceName"] = {
            "name": "interfaceName",
            "type": 0x052,
            "length": 32,
            "data": [0x54, 0x65, 0x30, 0x2f, 0x30, 0x2f, 0x30, 0x00] + [0x00]*24 # Te0/0/0
        }

        self.fields_dict["interfaceDescription"] = {
            "name": "interfaceDescription",
            "type": 0x053,
            "length": 64,
            "data": [0x54, 0x65, 0x6e, 0x47, 0x69, 0x67, 0x61, 0x62, 0x69, 0x74, 0x45, 0x74, 0x68, 0x65, 0x72, 0x6e, 0x65, 0x74, 0x30, 
                     0x2f, 0x30, 0x2f, 0x30]+ [0x00]*41 # TenGigabitEthernet0/0/0
        }

        self.fields_dict["samplerId"] = {
            "name": "samplerId",
            "type": 0x0030,
            "length": 4,
            "data": [0, 0, 0, 0]
        }

        self.fields_dict["samplerName"] = {
            "name": "samplerName",
            "type": 0x0054,
            "length": 40,
            "data": [0x73, 0x61, 0x6d, 0x70, 0x6c, 0x65] + [0] * 34
        }

        self.fields_dict["samplerMode"] = {
            "name": "samplerMode",
            "type": 0x0031,
            "length": 1,
            "data": [0]
        }

        self.fields_dict["samplerRandomInterval"] = {
            "name": "samplerRandomInterval",
            "type": 0x0032,
            "length": 2,
            "data": [0, 0]
        }

        self.fields_dict["p2pTechnology"] = {
            "name": "p2pTechnology",
            "type": 0x0120,
            "length": 10,
            "data": [0x6e, 0x6f] + [0] * 8 # no 
        }

        self.fields_dict["tunnelTechnology"] = {
            "name": "tunnelTechnology",
            "type": 0x0121,
            "length": 10,
            "data": [0x6e, 0x6f] + [0] * 8 # no 
        }

        self.fields_dict["encryptedTechnology"] = {
            "name": "encryptedTechnology",
            "type": 0x0122,
            "length": 10,
            "data": [0x6e, 0x6f] + [0] * 8 # no 
        }

    def __add_cisco_fields(self):
        self.fields_dict["clientIPv4Address"] = {
            "name": "clientIPv4Address",
            "type": 0xafcc,
            "length": 4,
            "enterprise_number": self.CISCO_PEN,
            "data": [16, 0, 0, 1] # 16.0.0.1
        }

        self.fields_dict["serverIPv4Address"] = {
            "name": "serverIPv4Address",
            "type": 0xafcd,
            "length": 4,
            "enterprise_number": self.CISCO_PEN,
            "data": [48, 0, 0, 1] # 48.0.0.1
        }

        self.fields_dict["clientTransportPort"] = {
            "name": "clientTransportPort",
            "type": 0xafd0,
            "length": 2,
            "enterprise_number": self.CISCO_PEN,
            "data": [128, 232] # Some random value
        }

        self.fields_dict["serverTransportProtocol"] = {
            "name": "serverTransportProtocol",
            "type": 0xafd1,
            "length": 2,
            "enterprise_number": self.CISCO_PEN,
            "data": [0, 53] # DNS by default
        }

        self.fields_dict["nbar2HttpHost"] = {
            "name": "nbar2HttpHost",
            "type": 0xafcb,
            "length": 7, # this should be variable length when we add support
            "enterprise_number": self.CISCO_PEN,
            "data": [115, 115, 115, 46, 101, 100, 117]
        }

        self.fields_dict["nbar2HttpHostBlackMagic1"] = {
            "name": "nbar2HttpHostBlackMagic1",
            "type": 0xafcb,
            "length": 7, # this should be variable length when we add support
            "enterprise_number": self.CISCO_PEN,
            "data": [3, 0, 0, 53, 52, 4, 0]
        }

        self.fields_dict["nbar2HttpHostBlackMagic2"] = {
            "name": "nbar2HttpHostBlackMagic2",
            "type": 0xafcb,
            "length": 7, # this should be variable length when we add support
            "enterprise_number": self.CISCO_PEN,
            "data": [3, 0, 0, 53, 52, 5, 133]
        }

        self.fields_dict["serverBytesL3"] = {
            "name": "serverBytesL3",
            "type": 0xa091,
            "length": 8,
            "enterprise_number": self.CISCO_PEN,
            "data": [0, 0, 0, 0, 0, 0, 1, 0]
        }

        self.fields_dict["clientBytesL3"] = {
            "name": "clientBytesL3",
            "type": 0xa092,
            "length": 8,
            "enterprise_number": self.CISCO_PEN,
            "data": [0, 0, 0, 0, 0, 0, 0, 240]
        }

        self.fields_dict["transportRtpSsrc"] = {
            "name": "transportRtpSsrc",
            "type": 0x909e,
            "length": 4,
            "enterprise_number": self.CISCO_PEN,
            "data": [0, 0, 0, 0]
        }

        self.fields_dict["numRespsCountDelta"] = {
            "name": "numRespsCountDelta",
            "type": 0xa44c,
            "length": 4,
            "enterprise_number": self.CISCO_PEN,
            "data": [0, 0, 0, 25]
        }

        self.fields_dict["sumServerNwkTime"] = {
            "name": "sumServerNwkTime",
            "type": 0xa467,
            "length": 4,
            "enterprise_number": self.CISCO_PEN,
            "data": [0, 1, 2, 25]
        }

        self.fields_dict["retransPackets"] = {
            "name": "retransPackets",
            "type": 0xa434,
            "length": 4,
            "enterprise_number": self.CISCO_PEN,
            "data": [0, 0, 2, 17]
        }

        self.fields_dict["sumNwkTime"] = {
            "name": "sumNwkTime",
            "type": 0xa461,
            "length": 4,
            "enterprise_number": self.CISCO_PEN,
            "data": [0, 5, 3, 12]
        }

        self.fields_dict["sumServerRespTime"] = {
            "name": "sumServerRespTime",
            "type": 0xa45a,
            "length": 4,
            "enterprise_number": self.CISCO_PEN,
            "data": [0, 0, 25, 12]
        }

        self.fields_dict["clientIPv6Address"] = {
            "name": "clientIPv6Address",
            "type": 0xafce,
            "length": 16,
            "enterprise_number": self.CISCO_PEN,
            "data": [32, 1, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3]
        }

        self.fields_dict["serverIPv6Address"] = {
            "name": "serverIPv6Address",
            "type": 0xafcf,
            "length": 16,
            "enterprise_number": self.CISCO_PEN,
            "data": [32, 1, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4]
        }

        self.fields_dict["collectTransportPacketsLostCounter"] = {
            "name": "collectTransportPacketsLostCounter",
            "type": 0x909b,
            "length": 4,
            "enterprise_number": self.CISCO_PEN,
            "data": [0, 0, 0, 67]
        }

        self.fields_dict["ARTServerRetransmissionsPackets"] = {
            "name": "ARTServerRetransmissionsPackets",
            "type": 0xa436,
            "length": 4,
            "enterprise_number": self.CISCO_PEN,
            "data": [0, 0, 0, 5]
        }

        self.fields_dict["collectTransportRtpPayloadType"] = {
            "name": "collectTransportRtpPayloadType",
            "type": 0x90b1,
            "length": 4,
            "enterprise_number": self.CISCO_PEN,
            "data": [0, 0, 0, 3]
        }

        self.fields_dict["applicationCategoryName"] = {
            "name": "applicationCategoryName",
            "type": 0xafc8,
            "length": 32,
            "enterprise_number": self.CISCO_PEN,
            "data": [0x6f, 0x74, 0x68, 0x65] + [0] * 28 # other
        }

        self.fields_dict["applicationSubCategoryName"] = {
            "name": "applicationSubCategoryName",
            "type": 0xafc9,
            "length": 32,
            "enterprise_number": self.CISCO_PEN,
            "data": [0x6f, 0x74, 0x68, 0x65] + [0] * 28 # other
        }

        self.fields_dict["applicationGroupName"] = {
            "name": "applicationGroupName",
            "type": 0xafca,
            "length": 32,
            "enterprise_number": self.CISCO_PEN,
            "data": [0x6f, 0x74, 0x68, 0x65] + [0] * 28 # other
        }

        self.fields_dict["unknownNameField"] = {
            "name": "unknownNameField",
            "type": 0xafd4,
            "length": 32,
            "enterprise_number": self.CISCO_PEN,
            "data": [0x62, 0x75, 0x73, 0x69, 0x6e, 0x65, 0x73, 0x73, 0x2d, 0x72, 0x65, 0x6c, 0x65, 0x76, 0x61, 0x6e, 0x74] + [0] * 15 # bussiness-relevant
        }

        self.fields_dict["unknownNameField2"] = {
            "name": "unknownNameField2",
            "type": 0xafc7,
            "length": 32,
            "enterprise_number": self.CISCO_PEN,
            "data": [0x6e, 0x65, 0x74, 0x77, 0x6f, 0x72, 0x6b, 0x2d, 0x63, 0x6f, 0x6e, 0x74, 0x72, 0x6f, 0x6c] + [0] * 17 # network-control
        }

        self.fields_dict["unknownNameField3"] = {
            "name": "unknownNameField2",
            "type": 0xafc6,
            "length": 32,
            "enterprise_number": self.CISCO_PEN,
            "data": [0x72, 0x6f, 0x75, 0x74, 0x69, 0x6e, 0x67] + [0] * 25 # routing
        }

        self.fields_dict["unknownNameField4"] = {
            "name": "unknownNameField4",
            "type": 0x90e5,
            "length": 8,
            "enterprise_number": self.CISCO_PEN,
            "data": [0, 0, 0, 0, 0, 0, 0 ,0]
        }

    def get_field(self, field):
        field = self.fields_dict.get(field, None)
        if field is not None:
            return field
        else:
            raise TRexError("Field {} not found in fields database.".format(field))

class AVCGenerators():
    def __init__(self):
        self.fields = AVCFields()
        self.generators = {} # map template ids to values
        self.__add_generators()

    def __add_generators(self):
        self.generators[256] = {
            "name": "256",
            "auto_start": True,
            "rate_pps": 0.1,
            "data_records_num": 10,
            "template_id": 256,
            "is_options_template": True,
            "scope_count": 1,
            "fields": [
                self.fields.get_field("ingressInterface"),
                self.fields.get_field("interfaceName"),
                self.fields.get_field("interfaceDescription")
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
                        "op": "inc"
                    }
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
                    }
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
                        "init": 0x30
                    }
                }
            ]
        }

        self.generators[257] = {
            "name": "257",
            "auto_start": True,
            "rate_pps": 0.1,
            "data_records_num": 3,
            "template_id": 257,
            "is_options_template": True,
            "scope_count": 1,
            "fields": [
                self.fields.get_field("ingressVRFID"),
                self.fields.get_field("VRFname") # when we support string engines we shall add an engine here.
            ],
            "engines": [
                {
                    "engine_name": "ingressVRFID",
                    "engine_type": "uint_list",
                    "params": {
                        "size": 4,
                        "offset": 0,
                        "op": "inc",
                        "list": [1, 2, 8191]
                    }
                },
                {
                    "engine_name": "VRFname",
                    "engine_type": "string_list",
                    "params": {
                        "size": 32,
                        "offset": 0,
                        "op": "inc",
                        "list": ["Mgmt-Intf", "FNF", "__Platform_iVRF:_ID00_"]
                    }
                }
            ]
        }

        self.generators[258] = {
            "name": "258",
            "auto_start": True,
            "rate_pps": 0.1,
            "data_records_num": 2,
            "template_id": 258,
            "is_options_template": True,
            "scope_count": 1,
            "fields": [
                self.fields.get_field("samplerId"),
                self.fields.get_field("samplerName"),
                self.fields.get_field("samplerMode"),
                self.fields.get_field("samplerRandomInterval")
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
                        "op": "inc"
                    }
                },
                {
                    "engine_name": "samplerMode",
                    "engine_type": "uint",
                    "params": {
                        "size": 1,
                        "offset": 0,
                        "min": 1,
                        "max": 2,
                        "op": "rand"
                    }
                },
                {
                    "engine_name": "samplerRandomInterval",
                    "engine_type": "uint",
                    "params": {
                        "size": 2,
                        "offset": 0,
                        "min": 0,
                        "max": 0xff,
                        "op": "rand"
                    }
                }
            ]
        }

        self.generators[259] = {
            "name": "259",
            "auto_start": True,
            "rate_pps": 90,
            "data_records_num": 16,
            "template_id": 259,
            "is_options_template": True,
            "scope_count": 1,
            "fields": [
                self.fields.get_field("applicationId"),
                self.fields.get_field("applicationName"),
                self.fields.get_field("applicationDescription"),
            ],
            "engines": [
                {
                    "engine_name": "applicationId",
                    "engine_type": "uint_list",
                    "params":
                        {
                            "size": 1,
                            "offset": 3,
                            "op": "inc",
                            "list": [0x22, 0x6b, 0x3d, 0x0d, 0x68, 0x5d, 0x0a, 0x31, 0x4c, 0x07, 0x3e, 0x10, 0x6e, 0x49, 0x48, 0x7e]
                        }
                },
                {
                    "engine_name": "applicationName",
                    "engine_type": "string_list",
                    "params":
                        {
                            "size": 24,
                            "offset": 0,
                            "op": "inc",
                            "list": ["3pc", "an", "any-host-internal", "argus", "aris", "ax25", "bbnrccmon", "bna", "br-sat-mon", "cbt", "cftp", "chaos", "compaq-peer", "cphb", "cpnx", "crtp"]
                        }
                },
                {
                    "engine_name": "applicationDescription",
                    "engine_type": "string_list",
                    "params":
                        {
                            "size": 55,
                            "offset": 0,
                            "op": "inc",
                            "list": ["Third Party Connect Protocol", "Active Networks", "any host internal protocol", "ARGUS", "ARIS", "AX.25 Frames", "BBN RCC Monitoring", "BNA", "Backroom SATNET Monitoring",
                                     "CBT", "CFTP", "CHAOS", "Compaq Peer Protocol", "Computer Protocol Heart Beat", "Computer Protocol Network Executive", "Combat Radio Protocol Transport Protocol"]
                        }
                }
            ]
        }

        self.generators[260] = {
            "name": "260",
            "auto_start": True,
            "rate_pps": 290,
            "data_records_num": 5,
            "template_id": 260,
            "is_options_template": True,
            "scope_count": 1,
            "fields": [
                self.fields.get_field("applicationId"),
                self.fields.get_field("applicationCategoryName"),
                self.fields.get_field("applicationSubCategoryName"),
                self.fields.get_field("applicationGroupName"),
                self.fields.get_field("unknownNameField"),
                self.fields.get_field("p2pTechnology"),
                self.fields.get_field("tunnelTechnology"),
                self.fields.get_field("encryptedTechnology"),
                self.fields.get_field("unknownNameField2"),
                self.fields.get_field("unknownNameField3"),
            ],
            "engines":[
                {
                    "engine_name": "applicationId",
                    "engine_type": "uint",
                    "params":
                        {
                            "size": 1,
                            "offset": 3,
                            "min": 1,
                            "max": 255,
                            "op": "rand"
                        }
                },
                {
                    "engine_name": "applicationCategoryName",
                    "engine_type": "string_list",
                    "params":
                        {
                            "size": 32,
                            "offset": 0,
                            "op": "rand",
                            "list": ["other", "file-sharing", "browsing", "general-browsing", "net-admin", "database"]
                        }
                },
                {
                    "engine_name": "tunnelTechnology",
                    "engine_type": "histogram_uint",
                    "params": {
                        "size": 4,
                        "offset": 0,
                        "entries": [
                            {
                                "v": 0x79657300, # yes
                                "prob": 1
                            },
                            {
                                "v": 0x6e6f0000, # no
                                "prob": 1
                            }
                        ]
                    }
                }
            ]
        }

        self.generators[266] = {
            "name": "266",
            "auto_start": True,
            "rate_pps": 100,
            "data_records_num": 10,
            "template_id": 266,
            "fields": [
                self.fields.get_field("clientIPv4Address"),
                self.fields.get_field("serverIPv4Address"),
                self.fields.get_field("ipVersion"),
                self.fields.get_field("protocolIdentifier"),
                self.fields.get_field("serverTransportProtocol"),
                self.fields.get_field("ingressVRFID"),
                self.fields.get_field("biflowDirection"),
                self.fields.get_field("observationPointId"),
                self.fields.get_field("applicationId"),
                self.fields.get_field("flowDirection"),
                self.fields.get_field("flowStartMilliseconds"),
                self.fields.get_field("flowEndMilliseconds"),
                self.fields.get_field("newConnectionDeltaCount"),
                self.fields.get_field("numRespsCountDelta"),
                self.fields.get_field("sumServerNwkTime"),
                self.fields.get_field("retransPackets"),
                self.fields.get_field("sumNwkTime"),
                self.fields.get_field("sumServerRespTime"),
                self.fields.get_field("responderPackets"),
                self.fields.get_field("initiatorPackets"),
                self.fields.get_field("ARTServerRetransmissionsPackets"),
                self.fields.get_field("serverBytesL3"),
                self.fields.get_field("clientBytesL3")
            ],
            "engines":[
                {
                    "engine_name": "clientIPv4Address",
                    "engine_type": "uint",
                    "params": {
                        "size": 2,
                        "offset": 2,
                        "op": "rand",
                        "min": 1,
                        "max": 0xFFFF - 1,
                    }
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
                    }
                },
                {
                    "engine_name": "protocolIdentifier",
                    "engine_type": "histogram_uint",
                    "params": {
                        "size": 1,
                        "offset": 0,
                        "entries": [
                            {
                                "v": 6, # TCP
                                "prob": 2
                            },
                            {
                                "v": 17, # UDP
                                "prob": 1
                            }
                        ]
                    }
                },
                {
                    "engine_name": "serverTransportProtocol",
                    "engine_type": "histogram_uint",
                    "params": {
                        "size": 2,
                        "offset": 0,
                        "entries": [
                            {
                                "v": 53, # DNS
                                "prob": 2
                            },
                            {
                                "v": 80, # HTTPS
                                "prob": 3
                            }
                        ]
                    }
                },
                {
                    "engine_name": "observationPointId",
                    "engine_type": "histogram_uint64",
                    "params": {
                        "size": 8,
                        "offset": 0,
                        "entries": [
                            {
                                "v": 4294967299,
                                "prob": 2
                            }
                        ]
                    }
                },
                {
                    "engine_name": "flowStartMilliseconds",
                    "engine_type": "time_start",
                    "params":
                        {
                            "size": 8,
                            "offset": 0,
                            "time_end_engine_name": "flowEndMilliseconds",
                            "ipg_min": 10000,
                            "ipg_max": 20000,
                            "time_offset": int(time.time() * 1000) # Unix Time in milliseconds
                        }
                },
                {
                    "engine_name": "flowEndMilliseconds",
                    "engine_type": "time_end",
                    "params":
                        {
                            "size": 8,
                            "offset": 0,
                            "time_start_engine_name": "flowStartMilliseconds",
                            "duration_min": 5000,
                            "duration_max": 10000,
                        }
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
                        "step": 1
                    }
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
                    }
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
                    }
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
                    }
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
                    }
                }
            ]
        }

        self.generators[267] = {
            "name": "267",
            "auto_start": True,
            "rate_pps": 0.1,
            "data_records_num": 10,
            "template_id": 267,
            "fields": [
                self.fields.get_field("clientIPv6Address"),
                self.fields.get_field("serverIPv6Address"),
                self.fields.get_field("ipVersion"),
                self.fields.get_field("protocolIdentifier"),
                self.fields.get_field("serverTransportProtocol"),
                self.fields.get_field("ingressVRFID"),
                self.fields.get_field("biflowDirection"),
                self.fields.get_field("observationPointId"),
                self.fields.get_field("applicationId"),
                self.fields.get_field("flowDirection"),
                self.fields.get_field("flowStartMilliseconds"),
                self.fields.get_field("flowEndMilliseconds"),
                self.fields.get_field("newConnectionDeltaCount"),
                self.fields.get_field("numRespsCountDelta"),
                self.fields.get_field("sumServerNwkTime"),
                self.fields.get_field("retransPackets"),
                self.fields.get_field("sumNwkTime"),
                self.fields.get_field("sumServerRespTime"),
                self.fields.get_field("responderPackets"),
                self.fields.get_field("initiatorPackets"),
                self.fields.get_field("ARTServerRetransmissionsPackets"),
                self.fields.get_field("serverBytesL3"),
                self.fields.get_field("clientBytesL3")
            ],
            "engines":[
                {
                    "engine_name": "clientIPv6Address",
                    "engine_type": "uint",
                    "params": {
                        "size": 8,
                        "offset": 8,
                        "op": "rand",
                        "min": 1,
                        "max": 0xFFFFFFFFFFFFFFFF,
                    }
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
                    }
                },
                {
                    "engine_name": "protocolIdentifier",
                    "engine_type": "histogram_uint",
                    "params": {
                        "size": 1,
                        "offset": 0,
                        "entries": [
                            {
                                "v": 6, # TCP
                                "prob": 2
                            },
                            {
                                "v": 17, # UDP
                                "prob": 1
                            }
                        ]
                    }
                },
                {
                    "engine_name": "serverTransportProtocol",
                    "engine_type": "histogram_uint",
                    "params": {
                        "size": 2,
                        "offset": 0,
                        "entries": [
                            {
                                "v": 53, # DNS
                                "prob": 2
                            },
                            {
                                "v": 80, # HTTPS
                                "prob": 3
                            }
                        ]
                    }
                },
                {
                    "engine_name": "observationPointId",
                    "engine_type": "histogram_uint64",
                    "params": {
                        "size": 8,
                        "offset": 0,
                        "entries": [
                            {
                                "v": 4294967299,
                                "prob": 2
                            }
                        ]
                    }
                },
                {
                    "engine_name": "flowStartMilliseconds",
                    "engine_type": "time_start",
                    "params":
                        {
                            "size": 8,
                            "offset": 0,
                            "time_end_engine_name": "flowEndMilliseconds",
                            "ipg_min": 1000,
                            "ipg_max": 2000,
                            "time_offset": int(time.time() * 1000) # Unix Time in milliseconds
                        }
                },
                {
                    "engine_name": "flowEndMilliseconds",
                    "engine_type": "time_end",
                    "params":
                        {
                            "size": 8,
                            "offset": 0,
                            "time_start_engine_name": "flowStartMilliseconds",
                            "duration_min": 10000,
                            "duration_max": 20000,
                        }
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
                        "step": 1
                    }
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
                    }
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
                    }
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
                    }
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
                    }
                }
            ]
        }

        self.generators[268] = {
            "name": "268",
            "auto_start": True,
            "rate_pps": 6,
            "data_records_num": 10,
            "template_id": 268,
            "fields": [
                self.fields.get_field("sourceIPv4Address"),
                self.fields.get_field("destinationIPv4Address"),
                self.fields.get_field("ipVersion"),
                self.fields.get_field("protocolIdentifier"),
                self.fields.get_field("sourceTransportPort"),
                self.fields.get_field("destinationTransportPort"),
                self.fields.get_field("transportRtpSsrc"),
                self.fields.get_field("collectTransportRtpPayloadType"),
                self.fields.get_field("ingressVRFID"),
                self.fields.get_field("biflowDirection"),
                self.fields.get_field("observationPointId"),
                self.fields.get_field("applicationId"),
                self.fields.get_field("flowDirection"),
                self.fields.get_field("collectTransportPacketsLostCounter"),
                self.fields.get_field("octetDeltaCount"),
                self.fields.get_field("packetDeltaCount"),
                self.fields.get_field("flowStartMilliseconds"),
                self.fields.get_field("flowEndMilliseconds"),
                self.fields.get_field("unknownNameField4"),
            ],
            "engines":[
                {
                    "engine_name": "sourceIPv4Address",
                    "engine_type": "uint",
                    "params": {
                        "size": 4,
                        "offset": 0,
                        "min": 0x300b017f, # 48.11.1.128
                        "max": 0x300b017f, # 48.11.1.128
                        "op": "inc"
                    }
                },
                {
                    "engine_name": "destinationIPv4Address",
                    "engine_type": "uint",
                    "params": {
                        "size": 4,
                        "offset": 0,
                        "min": 0x100000d1, # 16.0.0.209
                        "max": 0x100000d1, # 16.0.0.209
                        "op": "inc"
                    }
                },
                {
                    "engine_name": "sourceTransportPort",
                    "engine_type": "uint",
                    "params": {
                        "size": 2,
                        "offset": 0,
                        "min": 1024,
                        "max": 0xffff,
                        "op": "rand"
                    }
                },
                {
                    "engine_name": "destinationTransportPort",
                    "engine_type": "uint",
                    "params": {
                        "size": 2,
                        "offset": 0,
                        "min": 1024,
                        "max": 0xffff,
                        "op": "rand"
                    }
                },
                {
                    "engine_name": "transportRtpSsrc",
                    "engine_type": "uint",
                    "params": {
                        "size": 4,
                        "offset": 0,
                        "min": 0xb57a6710,
                        "max": 0xb57a6710,
                        "op": "inc"
                    }
                },
                {
                    "engine_name": "observationPointId",
                    "engine_type": "histogram_uint64",
                    "params": {
                        "size": 8,
                        "offset": 0,
                        "entries": [
                            {
                                "v": 4294967299,
                                "prob": 2
                            }
                        ]
                    }
                },
                {
                    "engine_name": "applicationId",
                    "engine_type": "uint",
                    "params": {
                        "size": 4,
                        "offset": 0,
                        "min": 0x0d00003d,
                        "max": 0x0d00003d,
                        "op": "inc"
                    }
                },
                {
                    "engine_name": "flowDirection",
                    "engine_type": "uint",
                    "params": {
                        "size": 1,
                        "offset": 0,
                        "min": 0,
                        "max": 1,
                        "op": "inc"
                    }
                },
                {
                    "engine_name": "octetDeltaCount",
                    "engine_type": "uint",
                    "params": {
                        "size": 4,
                        "offset": 4,
                        "op": "rand",
                        "min": 0,
                        "max": 0xFFFFFFFF
                    }
                },
                {
                    "engine_name": "packetDeltaCount",
                    "engine_type": "uint",
                    "params": {
                        "size": 2,
                        "offset": 6,
                        "op": "rand",
                        "min": 0,
                        "max": 0xFFFF
                    }
                },
                {
                    "engine_name": "flowStartMilliseconds",
                    "engine_type": "time_start",
                    "params":
                        {
                            "size": 8,
                            "offset": 0,
                            "time_end_engine_name": "flowEndMilliseconds",
                            "ipg_min": 10000,
                            "ipg_max": 20000,
                            "time_offset": int(time.time() * 1000) # Unix Time in milliseconds
                        }
                },
                {
                    "engine_name": "flowEndMilliseconds",
                    "engine_type": "time_end",
                    "params":
                        {
                            "size": 8,
                            "offset": 0,
                            "time_start_engine_name": "flowStartMilliseconds",
                            "duration_min": 5000,
                            "duration_max": 10000,
                        }
                }
            ]
        }

        self.generators[269] = {
            "name": "269",
            "auto_start": True,
            "rate_pps": 0.1,
            "data_records_num": 10,
            "template_id": 269,
            "fields": [
                self.fields.get_field("sourceIPv6Address"),
                self.fields.get_field("destinationIPv6Address"),
                self.fields.get_field("ipVersion"),
                self.fields.get_field("protocolIdentifier"),
                self.fields.get_field("sourceTransportPort"),
                self.fields.get_field("destinationTransportPort"),
                self.fields.get_field("transportRtpSsrc"),
                self.fields.get_field("collectTransportRtpPayloadType"),
                self.fields.get_field("ingressVRFID"),
                self.fields.get_field("biflowDirection"),
                self.fields.get_field("observationPointId"),
                self.fields.get_field("applicationId"),
                self.fields.get_field("flowDirection"),
                self.fields.get_field("collectTransportPacketsLostCounter"),
                self.fields.get_field("octetDeltaCount"),
                self.fields.get_field("packetDeltaCount"),
                self.fields.get_field("flowStartMilliseconds"),
                self.fields.get_field("flowEndMilliseconds"),
                self.fields.get_field("unknownNameField4"),
            ],
            "engines":[
                # simple bi dir
                {
                    "engine_name": "sourceIPv6Address",
                    "engine_type": "uint_list",
                    "params": {
                        "size": 8,
                        "offset": 8,
                        "list": [1, 2],
                        "op": "inc"
                    }
                },
                {
                    "engine_name": "destinationIPv6Address",
                    "engine_type": "uint_list",
                    "params": {
                        "size": 8,
                        "offset": 8,
                        "list": [2, 1],
                        "op": "inc"
                    }
                },
                {
                    "engine_name": "ipVersion",
                    "engine_type": "uint",
                    "params": {
                        "size": 1,
                        "offset": 0,
                        "min": 6,
                        "max": 6,
                        "op": "inc"
                    }
                },
                {
                    "engine_name": "sourceTransportPort",
                    "engine_type": "uint_list",
                    "params": {
                        "size": 2,
                        "offset": 0,
                        "list": [50302, 500],
                        "op": "inc"
                    }
                },
                {
                    "engine_name": "destinationTransportPort",
                    "engine_type": "uint_list",
                    "params": {
                        "size": 2,
                        "offset": 0,
                        "list": [500, 50302],
                        "op": "inc"
                    }
                },
                {
                    "engine_name": "transportRtpSsrc",
                    "engine_type": "uint",
                    "params": {
                        "size": 4,
                        "offset": 0,
                        "min": 0xb57a6710,
                        "max": 0xb57a6710,
                        "op": "inc"
                    }
                },
                {
                    "engine_name": "observationPointId",
                    "engine_type": "histogram_uint64",
                    "params": {
                        "size": 8,
                        "offset": 0,
                        "entries": [
                            {
                                "v": 4294967299,
                                "prob": 2
                            }
                        ]
                    }
                },
                {
                    "engine_name": "applicationId",
                    "engine_type": "uint",
                    "params": {
                        "size": 4,
                        "offset": 0,
                        "min": 0x0d00003d,
                        "max": 0x0d00003d,
                        "op": "inc"
                    }
                },
                {
                    "engine_name": "flowDirection",
                    "engine_type": "uint",
                    "params": {
                        "size": 1,
                        "offset": 0,
                        "min": 0,
                        "max": 1,
                        "op": "inc"
                    }
                },
                {
                    "engine_name": "octetDeltaCount",
                    "engine_type": "uint",
                    "params": {
                        "size": 4,
                        "offset": 4,
                        "op": "rand",
                        "min": 0,
                        "max": 0xFFFFFFFF
                    }
                },
                {
                    "engine_name": "packetDeltaCount",
                    "engine_type": "uint",
                    "params": {
                        "size": 2,
                        "offset": 6,
                        "op": "rand",
                        "min": 0,
                        "max": 0xFFFF
                    }
                },
                {
                    "engine_name": "flowStartMilliseconds",
                    "engine_type": "time_start",
                    "params":
                        {
                            "size": 8,
                            "offset": 0,
                            "time_end_engine_name": "flowEndMilliseconds",
                            "ipg_min": 10000,
                            "ipg_max": 20000,
                            "time_offset": int(time.time() * 1000) # Unix Time in milliseconds
                        }
                },
                {
                    "engine_name": "flowEndMilliseconds",
                    "engine_type": "time_end",
                    "params":
                        {
                            "size": 8,
                            "offset": 0,
                            "time_start_engine_name": "flowStartMilliseconds",
                            "duration_min": 5000,
                            "duration_max": 10000,
                        }
                }
            ]
        }

    def get_generator(self, template_id):
        gen = self.generators.get(template_id, None)
        if gen is not None:
            return gen
        else:
            raise TRexError("Generator with template id {} not found in generators database.".format(template_id))


class AVCProfiles():
    def __init__(self):
        self.generators = AVCGenerators()
        self.profiles = {} # mapping domain ids with profiles.

    def register_profile(self, netflow_version, dst_url, domain_id, generators):
        self.profiles[domain_id] = {
            "netflow_version": netflow_version,
            "dst": dst_url,
            "domain_id": domain_id,
            "generators": [self.generators.get_generator(template_id) for template_id in generators]
        }

    def get_profile(self, domain_id):
        profile = self.profiles.get(domain_id, None)
        if profile is not None:
            return profile
        else:
            raise TRexError("Profile with domain id {} not found in profiles database.".format(domain_id))

    def get_registered_domain_ids(self):
        return self.profiles.keys()


class Prof1():
    def __init__(self):
        self.avc_profiles = AVCProfiles()
        self.def_ns_plugs  = {'arp': {'enable': True}}
        self.def_c_plugs  = {'arp': {'enable': True}}

    def register_profiles(self, dst_url):
        self.avc_profiles.register_profile(10, dst_url, 6, [256, 257, 258, 259, 260])
        self.avc_profiles.register_profile(10, dst_url, 256, [266])
        self.avc_profiles.register_profile(10, dst_url, 512, [267])
        self.avc_profiles.register_profile(10, dst_url, 768, [268])
        self.avc_profiles.register_profile(10, dst_url, 1024, [269])

    def create_profile(self, mac, ipv4, dgv4, domain_id):
        mac      = Mac(mac)
        ipv4     = Ipv4(ipv4)
        dgv4     = Ipv4(dgv4)

        ns_key = EMUNamespaceKey(vport = 0, tci = 0,tpid = 0)
        ns = EMUNamespaceObj(ns_key = ns_key, def_c_plugs = self.def_c_plugs)

        c = EMUClientObj(mac=mac.V(),
                         ipv4=ipv4.V(),
                         ipv4_dg=dgv4.V(),
                         plugs={'ipfix': self.avc_profiles.get_profile(domain_id=domain_id),
                                'arp': {'enable': True}})
        ns.add_clients(c)
        return EMUProfile(ns = [ns], def_ns_plugs = self.def_ns_plugs)

    def get_profile(self, tuneables):

        
        # Argparse for tunables
        parser = argparse.ArgumentParser(description='Argparser for AVC IPFix', formatter_class=argparse.ArgumentDefaultsHelpFormatter)

        # client args
        parser.add_argument('--mac', type = str, default = '00:00:00:70:00:03',
            help='Mac address of the client')
        parser.add_argument('--4', type = str, default = '1.1.1.3', dest = 'ipv4',
            help='IPv4 address of the client')
        parser.add_argument('--dg', type = str, default = '1.1.1.1',
            help='Default Gateway address of the clients.')
        parser.add_argument('--dst-4', type = str, default = '10.56.29.228', dest = 'dst_4',
            help='Ipv4 address of collector')
        parser.add_argument('--dst-port', type = str, default = '2055', dest = 'dst_port',
            help='Destination port')
        parser.add_argument('--dst-url', type = str, default = None, dest = 'dst_url',
            help='Destination URL for HTTP/HTTPS exporter')
        parser.add_argument('--domain-id', type = int, default = 256, dest = 'domain_id',
            help='Domain ID. Registered domain IDs are {6, 256, 512, 768, 1024}')

        args = parser.parse_args(tuneables)

        dst_url = args.dst_url
        if dst_url == None:
            dst_url = HostPort(args.dst_4, args.dst_port).encode()

        self.register_profiles(dst_url)

        domain_ids = self.avc_profiles.get_registered_domain_ids()

        if args.domain_id not in domain_ids:
            raise TRexError("Invalid domain id {}. Domain id should be in {}".format(args.domain_id, domain_ids))

        return self.create_profile(args.mac, args.ipv4, args.dg, args.domain_id)

def register():
    return Prof1()
