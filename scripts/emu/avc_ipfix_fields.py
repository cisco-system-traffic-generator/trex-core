from emu.ipfix_profile import IpfixFields


class AvcIpfixFields(IpfixFields):
    def __init__(self):
        super(AvcIpfixFields, self).__init__()
        self.__add_iana_fields()
        self.__add_cisco_fields()

    def __add_iana_fields(self):
        self._fields["protocolIdentifier"] = {
            "name": "protocolIdentifier",
            "type": 0x0004,
            "length": 1,
            "data": [17],  # UDP by default
        }

        self._fields["applicationId"] = {
            "name": "applicationId",
            "type": 0x005F,
            "length": 4,
            "data": [1, 0, 0, 1],
        }

        self._fields["sourceIPv4Address"] = {
            "name": "sourceIPv4Address",
            "type": 0x0008,
            "length": 4,
            "data": [48, 0, 0, 1],
        }

        self._fields["destinationIPv4Address"] = {
            "name": "destinationIPv4Address",
            "type": 0x000C,
            "length": 4,
            "data": [16, 0, 0, 1],
        }

        self._fields["applicationName"] = {
            "name": "applicationName",
            "type": 0x0060,
            "length": 24,
            "data": [0x69, 0x63, 0x6D, 0x70] + [0] * 20,
        }

        self._fields["applicationDescription"] = {
            "name": "applicationDescription",
            "type": 0x005E,
            "length": 55,
            "data": [
                0x49,
                0x6E,
                0x74,
                0x65,
                0x72,
                0x6E,
                0x65,
                0x74,
                0x20,
                0x43,
                0x6F,
                0x6E,
                0x74,
                0x72,
                0x6F,
                0x6C,
                0x20,
                0x4D,
                0x65,
                0x73,
                0x73,
                0x61,
                0x67,
                0x65,
                0x20,
                0x50,
                0x72,
                0x6F,
                0x74,
                0x6F,
                0x63,
                0x6F,
                0x6C,
                0x00,
                0x00,
                0x00,
                0x00,
                0x00,
                0x00,
                0x00,
                0x00,
                0x00,
                0x00,
                0x00,
                0x00,
                0x00,
                0x00,
                0x00,
                0x00,
                0x00,
                0x00,
                0x00,
                0x00,
                0x00,
                0x00,
            ],
        }

        self._fields["flowStartSysUpTime"] = {
            "name": "flowStartSysUpTime",
            "type": 0x0016,
            "length": 4,
            "data": [0, 0, 0, 1],
        }

        self._fields["flowEndSysUpTime"] = {
            "name": "flowEndSysUpTime",
            "type": 0x0015,
            "length": 4,
            "data": [0, 0, 0, 10],
        }

        self._fields["flowStartMilliseconds"] = {
            "name": "flowStartMilliseconds",
            "type": 0x0098,
            "length": 8,
            "data": [0, 0, 0, 0, 0, 0, 255, 0],
        }

        self._fields["responderPackets"] = {
            "name": "responderPackets",
            "type": 0x012B,
            "length": 8,
            "data": [0, 0, 0, 0, 0, 0, 0, 10],
        }

        self._fields["initiatorPackets"] = {
            "name": "initiatorPackets",
            "type": 0x012A,
            "length": 8,
            "data": [0, 0, 0, 0, 0, 0, 0, 10],
        }

        self._fields["sourceIPv6Address"] = {
            "name": "sourceIPv6Address",
            "type": 0x001B,
            "length": 16,
            "data": [32, 1, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1],
        }

        self._fields["destinationIPv6Address"] = {
            "name": "destinationIPv6Address",
            "type": 0x001C,
            "length": 16,
            "data": [32, 1, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2],
        }

        self._fields["ipVersion"] = {
            "name": "ipVersion",
            "type": 0x003C,
            "length": 1,
            "data": [4],  # Version 4
        }

        self._fields["sourceTransportPort"] = {
            "name": "sourceTransportPort",
            "type": 0x0007,
            "length": 2,
            "data": [5, 1],
        }

        self._fields["destinationTransportPort"] = {
            "name": "destinationTransportPort",
            "type": 0x000B,
            "length": 2,
            "data": [0, 53],
        }

        self._fields["ingressVRFID"] = {
            "name": "ingressVRFID",
            "type": 0x00EA,
            "length": 4,
            "data": [0, 0, 0, 0],
        }

        self._fields["VRFname"] = {
            "name": "VRFname",
            "type": 0x00EC,
            "length": 32,
            "data": [0] * 32,
        }

        self._fields["biflowDirection"] = {
            "name": "biflowDirection",
            "type": 0x00EF,
            "length": 1,
            "data": [1],
        }

        self._fields["observationPointId"] = {
            "name": "observationPointId",
            "type": 0x008A,
            "length": 8,
            "data": [0, 0, 0, 0, 7, 3, 1, 4],
        }

        self._fields["flowDirection"] = {
            "name": "flowDirection",
            "type": 0x003D,
            "length": 1,
            "data": [1],
        }

        self._fields["octetDeltaCount"] = {
            "name": "octetDeltaCount",
            "type": 0x0001,
            "length": 8,
            "data": [0, 0, 0, 0, 0, 5, 14, 4],
        }

        self._fields["packetDeltaCount"] = {
            "name": "packetDeltaCount",
            "type": 0x0002,
            "length": 8,
            "data": [0, 0, 0, 0, 0, 1, 14, 4],
        }

        self._fields["flowEndMilliseconds"] = {
            "name": "flowEndMilliseconds",
            "type": 0x00099,
            "length": 8,
            "data": [0, 0, 0, 0, 0, 0, 5, 250],
        }

        self._fields["sourceIPv4Address"] = {
            "name": "sourceIPv4Address",
            "type": 0x00008,
            "length": 4,
            "data": [192, 168, 0, 1],
        }

        self._fields["destinationIPv4Address"] = {
            "name": "destinationIPv4Address",
            "type": 0x0000C,
            "length": 4,
            "data": [192, 168, 0, 254],
        }

        self._fields["newConnectionDeltaCount"] = {
            "name": "newConnectionDeltaCount",
            "type": 0x0116,
            "length": 4,
            "data": [0, 0, 0, 20],
        }

        self._fields["ingressInterface"] = {
            "name": "ingressInterface",
            "type": 0x000A,
            "length": 4,
            "data": [0, 0, 0, 0],
        }

        self._fields["interfaceName"] = {
            "name": "interfaceName",
            "type": 0x052,
            "length": 32,
            "data": [0x54, 0x65, 0x30, 0x2F, 0x30, 0x2F, 0x30, 0x00] + [0x00] * 24,  # Te0/0/0
        }

        self._fields["interfaceDescription"] = {
            "name": "interfaceDescription",
            "type": 0x053,
            "length": 64,
            "data": [
                0x54,
                0x65,
                0x6E,
                0x47,
                0x69,
                0x67,
                0x61,
                0x62,
                0x69,
                0x74,
                0x45,
                0x74,
                0x68,
                0x65,
                0x72,
                0x6E,
                0x65,
                0x74,
                0x30,
                0x2F,
                0x30,
                0x2F,
                0x30,
            ]
            + [0x00] * 41,  # TenGigabitEthernet0/0/0
        }

        self._fields["samplerId"] = {
            "name": "samplerId",
            "type": 0x0030,
            "length": 4,
            "data": [0, 0, 0, 0],
        }

        self._fields["samplerName"] = {
            "name": "samplerName",
            "type": 0x0054,
            "length": 40,
            "data": [0x73, 0x61, 0x6D, 0x70, 0x6C, 0x65] + [0] * 34,
        }

        self._fields["samplerMode"] = {
            "name": "samplerMode",
            "type": 0x0031,
            "length": 1,
            "data": [0],
        }

        self._fields["samplerRandomInterval"] = {
            "name": "samplerRandomInterval",
            "type": 0x0032,
            "length": 2,
            "data": [0, 0],
        }

        self._fields["p2pTechnology"] = {
            "name": "p2pTechnology",
            "type": 0x0120,
            "length": 10,
            "data": [0x6E, 0x6F] + [0] * 8,  # no
        }

        self._fields["tunnelTechnology"] = {
            "name": "tunnelTechnology",
            "type": 0x0121,
            "length": 10,
            "data": [0x6E, 0x6F] + [0] * 8,  # no
        }

        self._fields["encryptedTechnology"] = {
            "name": "encryptedTechnology",
            "type": 0x0122,
            "length": 10,
            "data": [0x6E, 0x6F] + [0] * 8,  # no
        }

    def __add_cisco_fields(self):
        self._fields["clientIPv4Address"] = {
            "name": "clientIPv4Address",
            "type": 0xAFCC,
            "length": 4,
            "enterprise_number": self.CISCO_PEN,
            "data": [16, 0, 0, 1],  # 16.0.0.1
        }

        self._fields["serverIPv4Address"] = {
            "name": "serverIPv4Address",
            "type": 0xAFCD,
            "length": 4,
            "enterprise_number": self.CISCO_PEN,
            "data": [48, 0, 0, 1],  # 48.0.0.1
        }

        self._fields["clientTransportPort"] = {
            "name": "clientTransportPort",
            "type": 0xAFD0,
            "length": 2,
            "enterprise_number": self.CISCO_PEN,
            "data": [128, 232],  # Some random value
        }

        self._fields["serverTransportProtocol"] = {
            "name": "serverTransportProtocol",
            "type": 0xAFD1,
            "length": 2,
            "enterprise_number": self.CISCO_PEN,
            "data": [0, 53],  # DNS by default
        }

        self._fields["nbar2HttpHost"] = {
            "name": "nbar2HttpHost",
            "type": 0xAFCB,
            "length": 7,  # this should be variable length when we add support
            "enterprise_number": self.CISCO_PEN,
            "data": [115, 115, 115, 46, 101, 100, 117],
        }

        self._fields["nbar2HttpHostBlackMagic1"] = {
            "name": "nbar2HttpHostBlackMagic1",
            "type": 0xAFCB,
            "length": 7,  # this should be variable length when we add support
            "enterprise_number": self.CISCO_PEN,
            "data": [3, 0, 0, 53, 52, 4, 0],
        }

        self._fields["nbar2HttpHostBlackMagic2"] = {
            "name": "nbar2HttpHostBlackMagic2",
            "type": 0xAFCB,
            "length": 7,  # this should be variable length when we add support
            "enterprise_number": self.CISCO_PEN,
            "data": [3, 0, 0, 53, 52, 5, 133],
        }

        self._fields["serverBytesL3"] = {
            "name": "serverBytesL3",
            "type": 0xA091,
            "length": 8,
            "enterprise_number": self.CISCO_PEN,
            "data": [0, 0, 0, 0, 0, 0, 1, 0],
        }

        self._fields["clientBytesL3"] = {
            "name": "clientBytesL3",
            "type": 0xA092,
            "length": 8,
            "enterprise_number": self.CISCO_PEN,
            "data": [0, 0, 0, 0, 0, 0, 0, 240],
        }

        self._fields["transportRtpSsrc"] = {
            "name": "transportRtpSsrc",
            "type": 0x909E,
            "length": 4,
            "enterprise_number": self.CISCO_PEN,
            "data": [0, 0, 0, 0],
        }

        self._fields["numRespsCountDelta"] = {
            "name": "numRespsCountDelta",
            "type": 0xA44C,
            "length": 4,
            "enterprise_number": self.CISCO_PEN,
            "data": [0, 0, 0, 25],
        }

        self._fields["sumServerNwkTime"] = {
            "name": "sumServerNwkTime",
            "type": 0xA467,
            "length": 4,
            "enterprise_number": self.CISCO_PEN,
            "data": [0, 1, 2, 25],
        }

        self._fields["retransPackets"] = {
            "name": "retransPackets",
            "type": 0xA434,
            "length": 4,
            "enterprise_number": self.CISCO_PEN,
            "data": [0, 0, 2, 17],
        }

        self._fields["sumNwkTime"] = {
            "name": "sumNwkTime",
            "type": 0xA461,
            "length": 4,
            "enterprise_number": self.CISCO_PEN,
            "data": [0, 5, 3, 12],
        }

        self._fields["sumServerRespTime"] = {
            "name": "sumServerRespTime",
            "type": 0xA45A,
            "length": 4,
            "enterprise_number": self.CISCO_PEN,
            "data": [0, 0, 25, 12],
        }

        self._fields["clientIPv6Address"] = {
            "name": "clientIPv6Address",
            "type": 0xAFCE,
            "length": 16,
            "enterprise_number": self.CISCO_PEN,
            "data": [32, 1, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3],
        }

        self._fields["serverIPv6Address"] = {
            "name": "serverIPv6Address",
            "type": 0xAFCF,
            "length": 16,
            "enterprise_number": self.CISCO_PEN,
            "data": [32, 1, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4],
        }

        self._fields["collectTransportPacketsLostCounter"] = {
            "name": "collectTransportPacketsLostCounter",
            "type": 0x909B,
            "length": 4,
            "enterprise_number": self.CISCO_PEN,
            "data": [0, 0, 0, 67],
        }

        self._fields["ARTServerRetransmissionsPackets"] = {
            "name": "ARTServerRetransmissionsPackets",
            "type": 0xA436,
            "length": 4,
            "enterprise_number": self.CISCO_PEN,
            "data": [0, 0, 0, 5],
        }

        self._fields["collectTransportRtpPayloadType"] = {
            "name": "collectTransportRtpPayloadType",
            "type": 0x90B1,
            "length": 1,
            "enterprise_number": self.CISCO_PEN,
            "data": [3],
        }

        self._fields["applicationCategoryName"] = {
            "name": "applicationCategoryName",
            "type": 0xAFC8,
            "length": 32,
            "enterprise_number": self.CISCO_PEN,
            "data": [0x6F, 0x74, 0x68, 0x65] + [0] * 28,  # other
        }

        self._fields["applicationSubCategoryName"] = {
            "name": "applicationSubCategoryName",
            "type": 0xAFC9,
            "length": 32,
            "enterprise_number": self.CISCO_PEN,
            "data": [0x6F, 0x74, 0x68, 0x65] + [0] * 28,  # other
        }

        self._fields["applicationGroupName"] = {
            "name": "applicationGroupName",
            "type": 0xAFCA,
            "length": 32,
            "enterprise_number": self.CISCO_PEN,
            "data": [0x6F, 0x74, 0x68, 0x65] + [0] * 28,  # other
        }

        self._fields["unknownNameField"] = {
            "name": "unknownNameField",
            "type": 0xAFD4,
            "length": 32,
            "enterprise_number": self.CISCO_PEN,
            "data": [
                0x62,
                0x75,
                0x73,
                0x69,
                0x6E,
                0x65,
                0x73,
                0x73,
                0x2D,
                0x72,
                0x65,
                0x6C,
                0x65,
                0x76,
                0x61,
                0x6E,
                0x74,
            ]
            + [0] * 15,  # bussiness-relevant
        }

        self._fields["unknownNameField2"] = {
            "name": "unknownNameField2",
            "type": 0xAFC7,
            "length": 32,
            "enterprise_number": self.CISCO_PEN,
            "data": [
                0x6E,
                0x65,
                0x74,
                0x77,
                0x6F,
                0x72,
                0x6B,
                0x2D,
                0x63,
                0x6F,
                0x6E,
                0x74,
                0x72,
                0x6F,
                0x6C,
            ]
            + [0] * 17,  # network-control
        }

        self._fields["unknownNameField3"] = {
            "name": "unknownNameField2",
            "type": 0xAFC6,
            "length": 32,
            "enterprise_number": self.CISCO_PEN,
            "data": [0x72, 0x6F, 0x75, 0x74, 0x69, 0x6E, 0x67] + [0] * 25,  # routing
        }

        self._fields["unknownNameField4"] = {
            "name": "unknownNameField4",
            "type": 0x90E5,
            "length": 8,
            "enterprise_number": self.CISCO_PEN,
            "data": [0, 0, 0, 0, 0, 0, 0, 0],
        }
