import xml.etree.ElementTree as ET
from collections import defaultdict

from trex.emu.trex_emu_ipfix_fields import AvcIpfixFields
from trex.emu.trex_emu_ipfix_profile import *
from trex.emu.api import *

PPACK_TAXONOMY_FILE = "./automation/trex_control_plane/interactive/trex/emu/taxonomy-pp63.xml"

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
            s = "name: {1}\napplication_id: {2} [{3}}:{4}]\nhelp_string: {5}\n".format(
                self.name, self.application_id, self.engine_id,
                self.selector_id, self.help_string)
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
