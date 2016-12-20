import os
import sys
import json
import base64
import inspect
from inspect import getcallargs
# add paths to scapy_service and trex_stl_lib.api
sys.path.append(os.path.abspath(os.pardir))
sys.path.append(os.path.abspath(os.path.join(os.pardir, os.pardir, os.pardir)))

from scapy_service import *
from scapy.all import *

service = Scapy_service()
v_handler = service.get_version_handler('1','01')

def pretty_json(obj):
    return json.dumps(obj, indent=4)

def pprint(obj):
    print(pretty_json(obj))

def is_verbose():
    return True

def pass_result(result, *args):
    # returns result unchanged, but can display debug info if enabled
    if is_verbose():
        fargs = (inspect.stack()[-1][4])
        print(fargs[0])
        pprint(result)
    return result

def pass_pkt(result):
    # returns packet unchanged, but can display debug info if enabled
    if is_verbose() and result is not None:
        result.show2()
    return result

# utility functions for tests

def layer_def(layerId, **layerfields):
    # test helper method to generate JSON-like protocol definition object for scapy
    # ex. { "id": "Ether", "fields": [ { "id": "dst", "value": "10:10:10:10:10:10" } ] }
    res = { "id": layerId }
    if layerfields:
        res["fields"] = [ {"id": k, "value": v} for k,v in layerfields.items() ]
    return res

def get_version_handler():
    return pass_result(service.get_version_handler("1", "01"))

def build_pkt(model_def):
    return pass_result(service.build_pkt(v_handler, model_def))

def build_pkt_ex(model_def, instructions_def):
    return pass_result(service.build_pkt_ex(v_handler, model_def, instructions_def))

def build_pkt_get_scapy(model_def):
    return build_pkt_to_scapy(build_pkt(model_def))

def reconstruct_pkt(bytes_b64, model_def):
    return pass_result(service.reconstruct_pkt(v_handler, bytes_b64, model_def))

def get_definitions(def_filter):
    return pass_result(service.get_definitions(v_handler, def_filter))

def get_definition_of(scapy_classname):
    return pass_result(service.get_definitions(v_handler, [scapy_classname]))['protocols'][0]

def get_payload_classes(def_filter):
    return pass_result(service.get_payload_classes(v_handler, def_filter))

def build_pkt_to_scapy(buildpkt_result):
    return pass_pkt(Ether(b64_to_bytes(buildpkt_result['binary'])))

def fields_to_map(field_array):
    # [{id, value, hvalue, offset}, ...] to map id -> {value, hvalue, offset}
    res = {}
    if field_array:
        for f in field_array:
            res[ f["id"] ] = f
    return res

def adapt_json_protocol_fields(protocols_array):
    # replaces layer.fields(array) with map for easier access in tests
    for protocol in protocols_array:
        # change structure for easier
        if protocol.get("fields"):
            protocol["fields"] = fields_to_map(protocol["fields"])

def get_templates():
    return pass_result(service.get_templates(v_handler))



def get_template(t):
    return pass_result(service.get_template(v_handler, t))

