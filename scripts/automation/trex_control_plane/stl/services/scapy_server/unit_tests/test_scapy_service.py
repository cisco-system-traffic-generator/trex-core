#
# run with 'nosetests' utility

import tempfile
import re
from basetest import *

RE_MAC = "^([0-9A-Fa-f]{2}:){5}([0-9A-Fa-f]{2})$"
ether_chksum_sz = 4 # this chksum will be added automatically(outside of scapy/packet editor)

TEST_MAC_1 = "10:10:10:10:10:10"
# Test scapy structure
TEST_PKT = Ether(dst=TEST_MAC_1)/IP(src='127.0.0.1')/TCP(sport=443)

# Corresponding JSON-like structure
TEST_PKT_DEF = [
        layer_def("Ether", dst=TEST_MAC_1),
        layer_def("IP", dst="127.0.0.1"),
        layer_def("TCP", sport="443")
        ]

def test_build_pkt():
    pkt = build_pkt_get_scapy(TEST_PKT_DEF)
    assert(pkt[TCP].sport == 443)

def test_build_invalid_structure_pkt():
    ether_fields = {"dst": TEST_MAC_1, "type": "LOOP"}
    pkt = build_pkt_get_scapy([
        layer_def("Ether", **ether_fields),
        layer_def("IP"),
        layer_def("TCP", sport=8080)
        ])
    assert(pkt[Ether].dst == TEST_MAC_1)
    assert(isinstance(pkt[Ether].payload, Raw))

def test_reconstruct_pkt():
    res = reconstruct_pkt(base64.b64encode(bytes(TEST_PKT)), None)
    pkt = build_pkt_to_scapy(res)
    assert(pkt[TCP].sport == 443)

def test_layer_del():
    modif = [
            {"id": "Ether"},
            {"id": "IP"},
            {"id": "TCP", "delete": True},
    ]
    res = reconstruct_pkt(base64.b64encode(bytes(TEST_PKT)), modif)
    pkt = build_pkt_to_scapy(res)
    assert(not pkt[IP].payload)

def test_layer_field_edit():
    modif = [
            {"id": "Ether"},
            {"id": "IP"},
            {"id": "TCP", "fields": [{"id": "dport", "value": 777}]},
    ]
    res = reconstruct_pkt(base64.b64encode(bytes(TEST_PKT)), modif)
    pkt = build_pkt_to_scapy(res)
    assert(pkt[TCP].dport == 777)
    assert(pkt[TCP].sport == 443)

def test_layer_add():
    modif = [
            {"id": "Ether"},
            {"id": "IP"},
            {"id": "TCP"},
            {"id": "Raw", "fields": [{"id": "load", "value": "GET /helloworld HTTP/1.0\n\n"}]},
    ]
    res = reconstruct_pkt(base64.b64encode(bytes(TEST_PKT)), modif)
    pkt = build_pkt_to_scapy(res)
    assert("GET /helloworld" in str(pkt[TCP].payload.load))

def test_build_Raw():
    pkt = build_pkt_get_scapy([
        layer_def("Ether"),
        layer_def("IP"),
        layer_def("TCP"),
        layer_def("Raw", load={"vtype": "BYTES", "base64": bytes_to_b64(b"hi")})
        ])
    assert(str(pkt[Raw].load == "hi"))

def test_build_fixed_pkt_size_template_gen_64():
    pkt = build_pkt_get_scapy([
        layer_def("Ether"),
        layer_def("IP"),
        layer_def("TCP"),
        layer_def("Raw", load={
            "vtype": "BYTES",
            "generate": "template",
            "total_size": 64,
            "template_base64": bytes_to_b64(b"hi")
        })
        ])
    print(len(pkt))
    assert(len(pkt) == 64 - ether_chksum_sz)

def test_build_fixed_pkt_size_bytes_gen_256():
    pkt = build_pkt_get_scapy([
        layer_def("Ether"),
        layer_def("IP"),
        layer_def("TCP"),
        layer_def("Raw", load={
            "vtype": "BYTES",
            "generate": "random_ascii",
            "total_size": 256
        })
        ])
    print(len(pkt))
    assert(len(pkt) == 256 - ether_chksum_sz)

def test_get_all():
    service.get_all(v_handler)

def test_get_definitions_all():
    get_definitions(None)
    defs = get_definitions(None)
    def_classnames = [pdef['id'] for pdef in defs['protocols']]
    assert("IP" in def_classnames)
    assert("Dot1Q" in def_classnames)
    assert("TCP" in def_classnames)

    # All instructions should have a help description.
    fe_instructions = defs['feInstructions']
    for instruction in fe_instructions:
        print(instruction['help'])
        assert("help" in instruction)
    assert(len(defs['feInstructionParameters']) > 0)
    assert(len(defs['feParameters']) > 0)
    assert(len(defs['feTemplates']) > 0)

def test_get_definitions_ether():
    res = get_definitions(["Ether"])
    protocols = res['protocols']
    assert(len(protocols) == 1)
    assert(protocols[0]['id'] == "Ether")

def test_get_payload_classes():
    eth_payloads = get_payload_classes([{"id":"Ether"}])
    assert("IP" in eth_payloads)
    assert("Dot1Q" in eth_payloads)
    assert("TCP" not in eth_payloads)
    assert(eth_payloads[0] == "IP") # order(based on prococols.json)

def test_get_tcp_payload_classes():
    payloads = get_payload_classes([{"id":"TCP"}])
    assert("Raw" in payloads)

def test_get_dot1q_payload_classes():
    payloads = get_payload_classes([{"id":"Dot1Q"}])
    assert("Dot1Q" in payloads)
    assert("IP" in payloads)

def test_pcap_read_and_write():
    pkts_to_write = [bytes_to_b64(bytes(TEST_PKT))]
    pcap_b64 = service.write_pcap(v_handler, pkts_to_write)
    array_pkt = service.read_pcap(v_handler, pcap_b64)
    pkt = build_pkt_to_scapy(array_pkt[0])
    assert(pkt[Ether].dst == TEST_MAC_1)

def test_layer_default_value():
    res = build_pkt([
        layer_def("Ether", src={"vtype": "UNDEFINED"})
        ])
    ether_fields = fields_to_map(res['data'][0]['fields'])
    assert(re.match(RE_MAC, ether_fields['src']['value']))

def test_layer_random_value():
    res = build_pkt([
        layer_def("Ether", src={"vtype": "RANDOM"})
        ])
    ether_fields = fields_to_map(res['data'][0]['fields'])
    assert(re.match(RE_MAC, ether_fields['src']['value']))

def test_IP_options():
    options_expr = "[IPOption_SSRR(copy_flag=0, routers=['1.2.3.4', '5.6.7.8'])]"
    res = build_pkt([
        layer_def("Ether"),
        layer_def("IP", options={"vtype": "EXPRESSION", "expr": options_expr}),
        ])
    pkt = build_pkt_to_scapy(res)
    options = pkt[IP].options
    assert(options[0].__class__.__name__ == 'IPOption_SSRR')
    assert(options[0].copy_flag == 0)
    assert(options[0].routers == ['1.2.3.4', '5.6.7.8'])

def test_TCP_options():
    options_expr = "[('MSS', 1460), ('NOP', None), ('NOP', None), ('SAckOK', b'')]"
    pkt = build_pkt_get_scapy([
        layer_def("Ether"),
        layer_def("IP"),
        layer_def("TCP", options={"vtype": "EXPRESSION", "expr": options_expr}),
        ])
    options = pkt[TCP].options
    assert(options[0] == ('MSS', 1460) )

def test_layer_wrong_structure():
    payload = [
            layer_def("Ether"),
            layer_def("IP"),
            layer_def("Raw", load="dummy"),
            layer_def("Ether"),
            layer_def("IP"),
            ]
    res = build_pkt(payload)
    pkt = build_pkt_to_scapy(res)
    assert(type(pkt[0]) is Ether)
    assert(type(pkt[1]) is IP)
    assert(isinstance(pkt[2], Raw))
    assert(not pkt[2].payload)
    model = res["data"]
    assert(len(payload) == len(model))
    # verify same protocol structure as in abstract model
    # and all fields defined
    for depth in range(len(payload)):
        layer_model = model[depth]
        layer_fields = fields_to_map(layer_model["fields"])
        assert(payload[depth]["id"] == model[depth]["id"])
        for field in layer_model["fields"]:
            required_field_properties = ["value", "hvalue", "offset"]
            for field_property in required_field_properties:
                assert(field[field_property] is not None)
        if (model[depth]["id"] == "Ether"):
            assert(layer_fields["type"]["hvalue"] == "IPv4")
    real_structure = [layer["real_id"] for layer in model]
    valid_structure_flags = [layer["valid_structure"] for layer in model]
    assert(real_structure == ["Ether", "IP", "Raw", None, None])
    assert(valid_structure_flags == [True, True, True, False, False])

def test_ether_definitions():
    etherDef = get_definition_of("Ether")
    assert(etherDef['name'] == "Ethernet II")
    etherFields = etherDef['fields']
    assert(etherFields[0]['id'] == 'dst')
    assert(etherFields[0]['name'] == 'Destination')
    assert(etherFields[1]['id'] == 'src')
    assert(etherFields[1]['name'] == 'Source')
    assert(etherFields[2]['id'] == 'type')
    assert(etherFields[2]['name'] == 'Type')

def test_ether_definitions():
    pdef = get_definition_of("ICMP")
    assert(pdef['id'] == "ICMP")
    assert(pdef['name'])
    assert(pdef['fields'])

def test_ip_definitions():
    pdef = get_definition_of("IP")
    fields = pdef['fields']
    assert(fields[0]['id'] == 'version')

    assert(fields[1]['id'] == 'ihl')
    assert(fields[1]['auto'] == True)

    assert(fields[3]['id'] == 'len')
    assert(fields[3]['auto'] == True)

    assert(fields[5]['id'] == 'flags')
    assert(fields[5]['type'] == 'BITMASK')
    assert(fields[5]['bits'][0]['name'] == 'Reserved')

    assert(fields[9]['id'] == 'chksum')
    assert(fields[9]['auto'] == True)

def test_generate_vm_instructions():
    ip_pkt_model = [
        layer_def("Ether"),
        layer_def("IP", src="16.0.0.1", dst="48.0.0.1")
    ]
    ip_instructions_model = {"field_engine": {"instructions": [{"id": "STLVmFlowVar",
                                                                "parameters": {"op": "inc", "min_value": "192.168.0.10",
                                                                               "size": "1", "name": "ip_src",
                                                                               "step": "1",
                                                                               "max_value": "192.168.0.100"}},
                                                               {"id": "STLVmWrFlowVar",
                                                                "parameters": {"pkt_offset": "IP.src", "is_big": "true",
                                                                               "add_val": "0", "offset_fixup": "0",
                                                                               "fv_name": "ip_src"}},
                                                               {"id": "STLVmFlowVar",
                                                                "parameters": {"op": "dec", "min_value": "32",
                                                                               "size": "1", "name": "ip_ttl",
                                                                               "step": "4", "max_value": "64"}},
                                                               {"id": "STLVmWrFlowVar",
                                                                "parameters": {"pkt_offset": "IP.ttl", "is_big": "true",
                                                                               "add_val": "0", "offset_fixup": "0",
                                                                               "fv_name": "ip_ttl"}}],
                                              "global_parameters": {}}}
    res = build_pkt_ex(ip_pkt_model, ip_instructions_model)
    src_instruction = res['field_engine']['instructions']['instructions'][0]
    assert(src_instruction['min_value'] == 3232235530)
    assert(src_instruction['max_value'] == 3232235620)

    ttl_instruction = res['field_engine']['instructions']['instructions'][2]
    assert(ttl_instruction['min_value'] == 32)
    assert(ttl_instruction['max_value'] == 64)


def test_list_templates_hierarchy():
    ids = []
    for template_info in get_templates():
        assert(template_info["meta"]["name"])
        assert("description" in template_info["meta"])
        ids.append(template_info['id'])
    assert('IPv4/TCP' in ids)
    assert('IPv4/UDP' in ids)
    assert('TCP-SYN' in ids)
    assert('ICMP echo request' in ids)

def test_get_template_root():
    obj = json.loads(get_template_by_id('TCP-SYN'))
    assert(obj['packet'][0]['id'] == 'Ether')
    assert(obj['packet'][1]['id'] == 'IP')
    assert(obj['packet'][2]['id'] == 'TCP')

def test_get_template_IP_ICMP():
    obj = json.loads(get_template_by_id('IPv4/ICMP'))
    assert(obj['packet'][0]['id'] == 'Ether')
    assert(obj['packet'][1]['id'] == 'IP')
    assert(obj['packet'][2]['id'] == 'ICMP')

def test_get_template_IPv6_UDP():
    obj = json.loads(get_template_by_id('IPv6/UDP'))
    assert(obj['packet'][0]['id'] == 'Ether')
    assert(obj['packet'][1]['id'] == 'IPv6')
    assert(obj['packet'][2]['id'] == 'UDP')

def test_templates_no_relative_path():
    res = get_template_by_id("../templates/IPv6/UDP")
    assert(res == "")

