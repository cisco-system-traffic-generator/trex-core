
import os
import sys
stl_pathname = os.path.abspath(os.path.join(os.pardir, os.pardir))
sys.path.append(stl_pathname)

from trex_stl_lib.api import *
import tempfile
import hashlib
import base64
import numbers
import random
import inspect
import json
from pprint import pprint

# add some layers as an example
# need to test more 
from scapy.layers.dns import *
from scapy.layers.dhcp import *
from scapy.layers.ipsec import *
from scapy.layers.netflow import *
from scapy.layers.sctp import *
from scapy.layers.tftp import *

from scapy.contrib.mpls import *
from scapy.contrib.igmp import *
from scapy.contrib.igmpv3 import *




#additional_stl_udp_pkts = os.path.abspath(os.path.join(os.pardir,os.pardir,os.pardir,os.pardir, os.pardir,'stl'))
#sys.path.append(additional_stl_udp_pkts)
#from udp_1pkt_vxlan import VXLAN
#sys.path.remove(additional_stl_udp_pkts)

try:
    from cStringIO import StringIO
except ImportError:
    from io import StringIO




class Scapy_service_api():

    def get_version_handler(self,client_v_major,client_v_minor):
        """ get_version_handler(self,client_v_major,client_v_minor)
            
            Gives a handler to client to connect and use server api

            Parameters
            ----------
            client_v_major - major number of api version on the client side

            Returns
            -------
            Handler(string) to provide when using server api
        """
        pass
    def get_all(self,client_v_handler):
        """ get_all(self,client_v_handler) 

        Sends all the protocols and fields that Scapy Service supports.
        also sends the md5 of the Protocol DB and Fields DB used to check if the DB's are up to date

        Parameters
        ----------
        None

        Returns
        -------
        Dictionary (of protocol DB and scapy fields DB)

        Raises
        ------
        Raises an exception when a DB error occurs (i.e a layer is not loaded properly and has missing components)
        """
        pass

    def check_update_of_dbs(self,client_v_handler,db_md5,field_md5):        
        """ check_update_of_dbs(self,client_v_handler,db_md5,field_md5) 
        Checks if the Scapy Service running on the server has a newer version of the databases that the client has

        Parameters
        ----------
        db_md5 - The md5 that was delivered with the protocol database that the client owns, when first received at the client
        field_md5 - The md5 that was delivered with the fields database that the client owns, when first received at the client

        Returns
        -------
        True/False according the Databases version(determined by their md5)

        Raises
        ------
        Raises an exception (ScapyException) when protocol DB/Fields DB is not up to date
        """
        pass


    def build_pkt(self,client_v_handler,pkt_model_descriptor):
        """ build_pkt(self,client_v_handler,pkt_model_descriptor) -> Dictionary (of Offsets,Show2 and Buffer)
        
        Performs calculations on the given packet and returns results for that packet.
    
        Parameters
        ----------
        pkt_descriptor - An array of dictionaries describing a network packet

        Returns
        -------
        - The packets offsets: each field in every layer is mapped inside the Offsets Dictionary
        - The Show2: A description of each field and its value in every layer of the packet
        - The Buffer: The Hexdump of packet encoded in base64

        Raises
        ------
        will raise an exception when the Scapy string format is illegal, contains syntax error, contains non-supported
        protocl, etc.
        """
        pass


    def get_tree(self,client_v_handler):
        """ get_tree(self) -> Dictionary describing an example of hierarchy in layers

        Scapy service holds a tree of layers that can be stacked to a recommended packet
        according to the hierarchy

        Parameters
        ----------
        None

        Returns
        -------
        Returns an example hierarchy tree of layers that can be stacked to a packet

        Raises
        ------
        None
        """
        pass
    
    def reconstruct_pkt(self,client_v_handler,binary_pkt,model_descriptor):
        """ reconstruct_pkt(self,client_v_handler,binary_pkt)

        Makes a Scapy valid packet by applying changes to binary packet and returns all information returned in build_pkt

        Parameters
        ----------
        Source packet in binary_pkt, formatted in "base64" encoding
        List of changes in model_descriptor

        Returns
        -------
        All data provided in build_pkt:
        show2 - detailed description of the packet
        buffer - the packet presented in binary
        offsets - the offset[in bytes] of each field in the packet

        """
        pass

    def read_pcap(self,client_v_handler,pcap_base64):
        """ read_pcap(self,client_v_handler,pcap_base64)

        Parses pcap file contents and returns an array with build_pkt information for each packet

        Parameters
        ----------
        binary pcap file in base64 encoding

        Returns
        -------
        Array of build_pkt(packet)
        """
        pass

    def write_pcap(self,client_v_handler,packets_base64):
        """ write_pcap(self,client_v_handler,packets_base64)

        Writes binary packets to pcap file

        Parameters
        ----------
        array of binary packets in base64 encoding

        Returns
        -------
        binary pcap file in base64 encoding
        """
        pass

    def get_definitions(self,client_v_handler, def_filter):
        """ get_definitions(self,client_v_handler, def_filter)

        Returns protocols and fields metadata of scapy service

        Parameters
        ----------
        def_filter - array of protocol names

        Returns
        -------
        definitions for protocols
        """
        pass

    def get_payload_classes(self,client_v_handler, pkt_model_descriptor):
        """ get_payload_classes(self,client_v_handler, pkt_model_descriptor)

        Returns an array of protocol classes, which normally can be used as a payload

        Parameters
        ----------
        pkt_model_descriptor - see build_pkt

        Returns
        -------
        array of supported protocol classes
        """
        pass

def is_python(version):
    return version == sys.version_info[0]

def is_number(obj):
    return isinstance(obj, numbers.Number)

def is_string(obj):
    return type(obj) == str or type(obj).__name__ == 'unicode' # python3 doesn't have unicode type

def is_ascii_str(strval):
    return strval and all(ord(ch) < 128 for ch in strval)

def is_ascii_bytes(buf):
    return buf and all(byte < 128 for byte in buf)

def is_ascii(obj):
    if is_bytes3(obj):
        return is_ascii_bytes(obj)
    else:
        return is_ascii_str(obj)

def is_bytes3(obj):
    # checks if obj is exactly bytes(always false for python2)
    return is_python(3) and type(obj) == bytes

def str_to_bytes(strval):
    return strval.encode("utf8")

def bytes_to_str(buf):
    return buf.decode("utf8")

def b64_to_bytes(payload_base64):
    # get bytes from base64 string(unicode)
    return base64.b64decode(payload_base64)

def bytes_to_b64(buf):
    # bytes to base64 string(unicode)
    return base64.b64encode(buf).decode('ascii')

def get_sample_field_val(scapy_layer, fieldId):
    # get some sample value for the field, to determine the value type
    # use random or serialized value if default value is None
    field_desc, current_val = scapy_layer.getfield_and_val(fieldId)
    if current_val is not None:
        return current_val
    try:
        # try to get some random value to determine type
        return field_desc.randval()._fix()
    except:
        pass
    try:
        # try to serialize/deserialize
        ltype = type(scapy_layer)
        pkt = ltype(bytes(ltype()))
        return pkt.getfieldval(fieldId)
    except:
        pass

def generate_random_bytes(sz, seed, start, end):
    # generate bytes of specified range with a fixed seed and size
    rnd = random.Random()
    n = end - start + 1
    if is_python(2):
        rnd = random.Random(seed)
        res = [start + int(rnd.random()*n) for _i in range(sz)]
        return ''.join(chr(x) for x in res)
    else:
        rnd = random.Random()
        # to generate same random sequence as 2.x
        rnd.seed(seed, version=1)
        res = [start + int(rnd.random()*n) for _i in range(sz)]
        return bytes(res)

def generate_bytes_from_template(sz, template):
    # generate bytes by repeating a template
    res = str_to_bytes('') # new bytes array
    if len(template) == 0:
        return res
    while len(res) < sz:
        res = res + template
    return res[:sz]

def parse_template_code(template_code):
    template_code = re.sub("0[xX]", '', template_code) # remove 0x
    template_code = re.sub("[\s]", '', template_code) # remove spaces
    return bytearray.fromhex(template_code)

def verify_payload_size(size):
    assert(size != None)
    if (size > (1<<20)): # 1Mb ought to be enough for anybody
        raise ValueError('size is too large')

def generate_bytes(bytes_definition):
    # accepts a bytes definition object
    # {generate: random_bytes or random_ascii, seed: <seed_number>, size: <size_bytes>}
    # {generate: template, template_base64: '<base64str>',  size: <size_bytes>}
    # {generate: template_code, template_text_code: '<template_code_str>',  size: <size_bytes>}
    gen_type = bytes_definition.get('generate')
    if gen_type == None:
        return b64_to_bytes(bytes_definition['base64'])
    elif gen_type == 'template_code':
        code = parse_template_code(bytes_definition["template_code"])
        bytes_size = int(bytes_definition.get('size') or len(code))
        verify_payload_size(bytes_size)
        return generate_bytes_from_template(bytes_size, code)
    else:
        bytes_size = int(bytes_definition['size']) # required
        seed = int(bytes_definition.get('seed') or 12345) # optional
        verify_payload_size(bytes_size)
        if gen_type == 'random_bytes':
            return generate_random_bytes(bytes_size, seed, 0, 0xFF)
        elif gen_type == 'random_ascii':
            return generate_random_bytes(bytes_size, seed, 0x20, 0x7E)
        elif gen_type == 'template':
            return generate_bytes_from_template(bytes_size, b64_to_bytes(bytes_definition["template_base64"]))

class ScapyException(Exception): pass
class Scapy_service(Scapy_service_api):

#----------------------------------------------------------------------------------------------------
    class ScapyFieldDesc:
        def __init__(self,FieldName,regex='empty'):
            self.FieldName = FieldName
            self.regex = regex
            #defualt values - should be changed when needed, or added to constructor
            self.string_input =""
            self.string_input_mex_len = 1
            self.integer_input = 0
            self.integer_input_min = 0
            self.integer_input_max = 1
            self.input_array = []
            self.input_list_max_len = 1

        def stringRegex(self):
            return self.regex        
#----------------------------------------------------------------------------------------------------
    def __init__(self):
        self.Raw = {'Raw':''}
        self.high_level_protocols = ['Raw']
        self.transport_protocols = {'TCP':self.Raw,'UDP':self.Raw}
        self.network_protocols = {'IP':self.transport_protocols ,'ARP':''}
        self.low_level_protocols = { 'Ether': self.network_protocols }
        self.regexDB= {'MACField' : self.ScapyFieldDesc('MACField','^([0-9a-fA-F][0-9a-fA-F]:){5}([0-9a-fA-F][0-9a-fA-F])$'),
              'IPField' : self.ScapyFieldDesc('IPField','^(25[0-5]|2[0-4][0-9]|[0-1]{1}[0-9]{2}|[1-9]{1}[0-9]{1}|[1-9])\.(25[0-5]|2[0-4][0-9]|[0-1]{1}[0-9]{2}|[1-9]{1}[0-9]{1}|[1-9]|0)\.(25[0-5]|2[0-4][0-9]|[0-1]{1}[0-9]{2}|[1-9]{1}[0-9]{1}|[1-9]|0)\.(25[0-5]|2[0-4][0-9]|[0-1]{1}[0-9]{2}|[1-9]{1}[0-9]{1}|[0-9])$')}
        self.all_protocols = self._build_lib()
        self.protocol_tree = {'ALL':{'Ether':{'ARP':{},'IP':{'TCP':{'RAW':'payload'},'UDP':{'RAW':'payload'}}}}}
        self.version_major = '1'
        self.version_minor = '01'
        self.server_v_hashed = self._generate_version_hash(self.version_major,self.version_minor)
        self.protocol_definitions = {} # protocolId -> prococol definition overrides data
        self._load_definitions_from_json()

    def _load_definitions_from_json(self):
        # load protocol definitions from a json file
        self.protocol_definitions = {}
        with open('protocols.json', 'r') as f:
            protocols = json.load(f)
            for protocol in protocols:
                self.protocol_definitions[ protocol['id'] ] = protocol

    def _all_protocol_structs(self):
        old_stdout = sys.stdout
        sys.stdout = mystdout = StringIO()
        ls()
        sys.stdout = old_stdout
        all_protocol_data= mystdout.getvalue()
        return all_protocol_data

    def _protocol_struct(self,protocol):
        if '_' in protocol:
            return []
        if not protocol=='':
            if protocol not in self.all_protocols:
                return 'protocol not supported'
        protocol = eval(protocol)
        old_stdout = sys.stdout
        sys.stdout = mystdout = StringIO()
        ls(protocol)
        sys.stdout = old_stdout
        protocol_data= mystdout.getvalue()
        return protocol_data

    def _build_lib(self):
        lib = self._all_protocol_structs()
        lib = lib.splitlines()
        all_protocols=[]
        for entry in lib:
            entry = entry.split(':')
            all_protocols.append(entry[0].strip())
        del all_protocols[len(all_protocols)-1]
        return all_protocols

    def _parse_description_line(self,line):
        line_arr = [x.strip() for x in re.split(': | = ',line)]
        return tuple(line_arr)

    def _parse_entire_description(self,description):
        description = description.split('\n')
        description_list = [self._parse_description_line(x) for x in description]
        del description_list[len(description_list)-1]
        return description_list

    def _get_protocol_details(self,p_name):
        protocol_str = self._protocol_struct(p_name)
        if protocol_str=='protocol not supported':
            return 'protocol not supported'
        if len(protocol_str) is 0:
            return []
        tupled_protocol = self._parse_entire_description(protocol_str)
        return tupled_protocol

    def _value_from_dict(self, val):
        # allows building python objects from json
        if type(val) == type({}):
            value_type = val['vtype']
            if value_type == 'EXPRESSION':
                return eval(val['expr'], scapy.all.__dict__)
            elif value_type == 'BYTES':   # bytes payload(ex Raw.load)
                return generate_bytes(val)
            elif value_type == 'OBJECT':
                return val['value']
            else:
                return val # it's better to specify type explicitly
        elif type(val) == type([]):
            return [self._value_from_dict(v) for v in val]
        else:
            return val

    def _field_value_from_def(self, scapy_pkt, layer, fieldId, val):
        field_desc = layer.get_field(fieldId)
        sample_val = get_sample_field_val(layer, fieldId)
        # extensions for field values
        if type(val) == type({}):
            value_type = val['vtype']
            if value_type == 'UNDEFINED': # clear field value
                return None
            elif value_type == 'RANDOM': # random field value
                return field_desc.randval()
            elif value_type == 'MACHINE': # internal machine field repr
                return field_desc.m2i(layer, b64_to_bytes(val['base64']))
            elif value_type == 'BYTES':
                if 'total_size' in val: # custom case for total pkt size
                    gen = {}
                    gen.update(val)
                    total_sz = gen['total_size']
                    del gen['total_size']
                    gen['size'] = total_sz - len(scapy_pkt)
                    return generate_bytes(gen)
                else:
                    return generate_bytes(val)
        if is_number(sample_val) and is_string(val):
            # human-value. guess the type and convert to internal value
            # seems setfieldval already does this for some fields,
            # but does not convert strings/hex(0x123) to integers and long
            val = str(val) # unicode -> str(ascii)
            # parse str to int/long as a decimal or hex
            val_constructor = type(sample_val)
            if len(val) == 0:
                return None
            elif re.match(r"^0x[\da-f]+$", val, flags=re.IGNORECASE): # hex
                return val_constructor(val, 16)
            elif re.match(r"^\d+L?$", val): # base10
                return val_constructor(val)
        # generate recursive field-independent values
        return self._value_from_dict(val)

    def _print_tree(self):
        pprint(self.protocol_tree)

    def _get_all_db(self):
        db = {}
        for pro in self.all_protocols:
            details = self._get_protocol_details(pro)
            db[pro] = details
        return db

    def _get_all_fields(self):
        fields = []
        for pro in self.all_protocols:
            details = self._get_protocol_details(pro)
            for i in range(0,len(details),1):
                if len(details[i]) == 3:
                    fields.append(details[i][1])
        uniqueFields = list(set(fields))
        fieldDict = {}
        for f in uniqueFields:
            if f in self.regexDB:
                fieldDict[f] = self.regexDB[f].stringRegex()
            else:
                fieldDict[f] = self.ScapyFieldDesc(f).stringRegex()
        return fieldDict

    def _fully_define(self,pkt):
        # returns scapy object with all fields initialized
        rootClass = type(pkt)
        full_pkt = rootClass(bytes(pkt))
        full_pkt.build() # this trick initializes offset
        return full_pkt

    def _bytes_to_value(self, payload_bytes):
        # generates struct with a value
        return { "vtype": "BYTES", "base64": bytes_to_b64(payload_bytes) }

    def _pkt_to_field_tree(self,pkt):
        pkt.build()
        result = []
        pcap_struct = self._fully_define(pkt) # structure, which will appear in pcap binary
        while pkt:
            layer_id = type(pkt).__name__ # Scapy classname
            layer_full = self._fully_define(pkt) # current layer recreated from binary to get auto-calculated vals
            real_layer_id = type(pcap_struct).__name__ if pcap_struct else None
            valid_struct = True # shows if packet is mapped correctly to the binary representation
            if not pcap_struct:
                valid_struct = False
            elif not issubclass(type(pkt), type(pcap_struct)) and not issubclass(type(pcap_struct), type(pkt)):
                # structure mismatch. no need to go deeper in pcap_struct
                valid_struct = False
                pcap_struct = None
            fields = []
            for field_desc in pkt.fields_desc:
                field_id = field_desc.name
                ignored = field_id not in layer_full.fields
                offset = field_desc.offset
                protocol_offset = pkt.offset
                field_sz = field_desc.get_size_bytes()
                # some values are unavailable in pkt(original model)
                # at the same time,
                fieldval = pkt.getfieldval(field_id)
                pkt_fieldval_defined = is_string(fieldval) or is_number(fieldval) or is_bytes3(fieldval)
                if not pkt_fieldval_defined:
                    fieldval = layer_full.getfieldval(field_id)
                value = None
                hvalue = None
                value_base64 = None
                if is_python(3) and is_bytes3(fieldval):
                    value = self._bytes_to_value(fieldval)
                    if is_ascii_bytes(fieldval):
                        hvalue = bytes_to_str(fieldval)
                    else:
                        # can't be shown as ascii.
                        # also this buffer may not be unicode-compatible(still can try to convert)
                        value = self._bytes_to_value(fieldval)
                        hvalue = '<binary>'
                elif not is_string(fieldval):
                    # value as is. this can be int,long, or custom object(list/dict)
                    # "nice" human value, i2repr(string) will have quotes, so we have special handling for them
                    hvalue = field_desc.i2repr(pkt, fieldval)

                    if is_number(fieldval):
                        value = fieldval
                        if is_string(hvalue) and re.match(r"^\d+L$", hvalue):
                            hvalue =  hvalue[:-1] # chop trailing L for long decimal number(python2)
                    else:
                        # fieldval is an object( class / list / dict )
                        # generic serialization/deserialization needed for proper packet rebuilding from packet tree,
                        # some classes can not be mapped to json, but we can pass them serialize them
                        # as a python eval expr, value bytes base64, or field machine internal val(m2i)
                        value = {"vtype": "EXPRESSION", "expr": hvalue}
                if is_python(3) and is_string(fieldval):
                    hvalue = value = fieldval
                if is_python(2) and is_string(fieldval):
                    if is_ascii(fieldval):
                        hvalue = value = fieldval
                    else:
                        # python2 non-ascii byte buffers
                        # payload contains non-ascii chars, which
                        # sometimes can not be passed as unicode strings
                        value = self._bytes_to_value(fieldval)
                        hvalue = '<binary>'
                if field_desc.name == 'load':
                    # show Padding(and possible similar classes) as Raw
                    layer_id = 'Raw'
                    field_sz = len(pkt)
                    value = self._bytes_to_value(fieldval)
                field_data = {
                        "id": field_id,
                        "value": value,
                        "hvalue": hvalue,
                        "offset": offset,
                        "length": field_sz
                        }
                if ignored:
                    field_data["ignored"] = ignored
                fields.append(field_data)
            layer_data = {
                    "id": layer_id,
                    "offset": pkt.offset,
                    "fields": fields,
                    "real_id": real_layer_id,
                    "valid_structure": valid_struct,
                    }
            result.append(layer_data)
            pkt = pkt.payload
            if pcap_struct:
                pcap_struct = pcap_struct.payload or None
        return result

#input: container
#output: md5 encoded in base64
    def _get_md5(self,container):
        container = json.dumps(container)
        m = hashlib.md5()
        m.update(str_to_bytes(container))
        res_md5 = bytes_to_b64(m.digest())
        return res_md5

    def get_version(self):
        return {'built_by':'itraviv','version':self.version_major+'.'+self.version_minor}

    def supported_methods(self,method_name='all'):
        if method_name=='all':
            methods = {}
            for f in dir(Scapy_service):
                if f[0]=='_':
                    continue
                if inspect.ismethod(eval('Scapy_service.'+f)):
                    param_list = inspect.getargspec(eval('Scapy_service.'+f))[0]
                    del param_list[0] #deleting the parameter "self" that appears in every method 
                                      #because the server automatically operates on an instance,
                                      #and this can cause confusion
                    methods[f] = (len(param_list), param_list)
            return methods
        if method_name in dir(Scapy_service):
            return True
        return False

    def _generate_version_hash(self,v_major,v_minor):
        v_for_hash = v_major+v_minor+v_major+v_minor
        m = hashlib.md5()
        m.update(str_to_bytes(v_for_hash))
        return bytes_to_b64(m.digest())

    def _generate_invalid_version_error(self):
        error_desc1 = "Provided version handler does not correspond to the server's version.\nUpdate client to latest version.\nServer version:"+self.version_major+"."+self.version_minor
        return error_desc1

    def _verify_version_handler(self,client_v_handler):
        return (self.server_v_hashed == client_v_handler)

    def _parse_packet_dict(self, layer, layer_classes, base_layer):
        class_p = layer_classes[layer['id']] # class id -> class dict
        scapy_layer = class_p()
        if isinstance(scapy_layer, Raw):
            scapy_layer.load = str_to_bytes("dummy")
        if base_layer == None:
            base_layer = scapy_layer
        if 'fields' in layer:
            self._modify_layer(base_layer, scapy_layer, layer['fields'])
        return scapy_layer

    def _packet_model_to_scapy_packet(self,data):
        layer_classes = {}
        for layer_class in Packet.__subclasses__():
            layer_classes[layer_class.__name__] = layer_class
        base_layer = self._parse_packet_dict(data[0], layer_classes, None)
        for i in range(1,len(data),1):
            packet_layer = self._parse_packet_dict(data[i], layer_classes, base_layer)
            base_layer = base_layer/packet_layer
        return base_layer

    def _pkt_data(self,pkt):
        if pkt == None:
            return {'data': [], 'binary': None}
        data = self._pkt_to_field_tree(pkt)
        binary = bytes_to_b64(bytes(pkt))
        res = {'data': data, 'binary': binary}
        return res

#--------------------------------------------API implementation-------------
    def get_tree(self,client_v_handler):
        if not (self._verify_version_handler(client_v_handler)):
            raise ScapyException(self._generate_invalid_version_error())
        return self.protocol_tree

    def get_version_handler(self,client_v_major,client_v_minor):
        v_handle = self._generate_version_hash(client_v_major,client_v_minor)
        return v_handle

# pkt_descriptor in packet model format (dictionary)
    def build_pkt(self,client_v_handler,pkt_model_descriptor):
        if not (self._verify_version_handler(client_v_handler)):
            raise ScapyException(self._generate_invalid_version_error())
        pkt = self._packet_model_to_scapy_packet(pkt_model_descriptor)
        return self._pkt_data(pkt)

    # @deprecated. to be removed
    def get_all(self,client_v_handler):
        if not (self._verify_version_handler(client_v_handler)):
            raise ScapyException(self._generate_invalid_version_error())
        fields=self._get_all_fields()
        db=self._get_all_db()
        fields_md5 = self._get_md5(fields)
        db_md5 = self._get_md5(db)
        res = {}
        res['db'] = db
        res['fields'] = fields
        res['db_md5'] = db_md5
        res['fields_md5'] = fields_md5
        return res

    def _is_packet_class(self, pkt_class):
        # returns true for final Packet classes. skips aliases and metaclasses
        return issubclass(pkt_class, Packet) and pkt_class.name and pkt_class.fields_desc

    def _getDummyPacket(self, pkt_class):
        if issubclass(pkt_class, Raw):
            # need to have some payload. otherwise won't appear in the binary chunk
            return pkt_class(load=str_to_bytes("dummy"))
        else:
            return pkt_class()

        
    def _get_payload_classes(self, pkt_class):
        # tries to find, which subclasses allowed.
        # this can take long time, since it tries to build packets with all subclasses(O(N))
        allowed_subclasses = []
        for pkt_subclass in conf.layers:
            if self._is_packet_class(pkt_subclass):
                try:
                    pkt_w_payload = pkt_class() / self._getDummyPacket(pkt_subclass)
                    recreated_pkt = pkt_class(bytes(pkt_w_payload))
                    if type(recreated_pkt.lastlayer()) is pkt_subclass:
                        allowed_subclasses.append(pkt_subclass)
                except Exception as e:
                    # no actions needed on fail, just sliently skip
                    pass
        return allowed_subclasses

    def _get_fields_definition(self, pkt_class, fieldsDef):
        # fieldsDef - array of field definitions(or empty array)
        fields = []
        for field_desc in pkt_class.fields_desc:
            fieldId = field_desc.name
            field_data = {
                    "id": fieldId,
                    "name": field_desc.name
            }
            for fieldDef in fieldsDef:
                if fieldDef['id'] == fieldId:
                    field_data.update(fieldDef)
            if isinstance(field_desc, EnumField):
                try:
                    field_data["values_dict"] = field_desc.s2i
                    if field_data.get("type") == None:
                        if len(field_data["values_dict"] > 0):
                            field_data["type"] = "ENUM"
                        elif fieldId == 'load':
                            field_data["type"] = "BYTES"
                        else:
                            field_data["type"] = "STRING"
                    field_data["values_dict"] = field_desc.s2i
                except:
                    # MultiEnumField doesn't have s2i. need better handling
                    pass
            fields.append(field_data)
        return fields

    def get_definitions(self,client_v_handler, def_filter):
        # def_filter is an array of classnames or None
        all_classes = Packet.__subclasses__() # as an alternative to conf.layers
        if def_filter:
            all_classes = [c for c in all_classes if c.__name__ in def_filter]
        protocols = []
        for pkt_class in all_classes:
            if self._is_packet_class(pkt_class):
                # enumerate all non-abstract Packet classes
                protocolId = pkt_class.__name__
                protoDef = self.protocol_definitions.get(protocolId) or {}
                protocols.append({
                    "id": protocolId,
                    "name": protoDef.get('name') or pkt_class.name,
                    "fields": self._get_fields_definition(pkt_class, protoDef.get('fields') or [])
                    })
        res = {"protocols": protocols}
        return res

    def get_payload_classes(self,client_v_handler, pkt_model_descriptor):
        pkt = self._packet_model_to_scapy_packet(pkt_model_descriptor)
        pkt_class = type(pkt.lastlayer())
        protocolDef = self.protocol_definitions.get(pkt_class.__name__)
        if protocolDef and protocolDef.get('payload'):
            return protocolDef['payload']
        return [c.__name__ for c in self._get_payload_classes(pkt_class)]

#input in string encoded base64
    def check_update_of_dbs(self,client_v_handler,db_md5,field_md5):
        if not (self._verify_version_handler(client_v_handler)):
            raise ScapyException(self._generate_invalid_version_error())
        fields=self._get_all_fields()
        db=self._get_all_db()
        current_db_md5 = self._get_md5(db)
        current_field_md5 = self._get_md5(fields)
        res = []
        if (field_md5 == current_field_md5):
            if (db_md5 == current_db_md5):
                return True
            else:
                raise ScapyException("Protocol DB is not up to date")
        else:
            raise ScapyException("Fields DB is not up to date")

    def _modify_layer(self, scapy_pkt, scapy_layer, fields):
        for field in fields:
            fieldId = str(field['id'])
            fieldval = self._field_value_from_def(scapy_pkt, scapy_layer, fieldId, field['value'])
            if fieldval is not None:
                scapy_layer.setfieldval(fieldId, fieldval)
            else:
                scapy_layer.delfieldval(fieldId)

    def _is_last_layer(self, layer):
        # can be used, that layer has no payload
        # if true, the layer.payload is likely NoPayload()
        return layer is layer.lastlayer()

#input of binary_pkt must be encoded in base64
    def reconstruct_pkt(self,client_v_handler,binary_pkt,model_descriptor):
        pkt_bin = b64_to_bytes(binary_pkt)
        scapy_pkt = Ether(pkt_bin)
        if not model_descriptor:
            model_descriptor = []
        for depth in range(len(model_descriptor)):
            model_layer = model_descriptor[depth]
            if model_layer.get('delete') is True:
                # slice packet from the current item
                if depth == 0:
                    scapy_pkt = None
                    break
                else:
                    scapy_pkt[depth-1].payload = None
                    break
            if depth > 0 and self._is_last_layer(scapy_pkt[depth-1]):
                # insert new layer(s) from json definition
                remaining_definitions = model_descriptor[depth:]
                pkt_to_append = self._packet_model_to_scapy_packet(remaining_definitions)
                scapy_pkt = scapy_pkt / pkt_to_append
                break
            # modify fields of existing stack items
            scapy_layer = scapy_pkt[depth]
            if model_layer['id'] != type(scapy_layer).__name__:
                # TODO: support replacing payload, instead of breaking
                raise ScapyException("Protocol id inconsistent")
            if 'fields' in model_layer:
                self._modify_layer(scapy_pkt, scapy_layer, model_layer['fields'])
        return self._pkt_data(scapy_pkt)

    def read_pcap(self,client_v_handler,pcap_base64):
        pcap_bin = b64_to_bytes(pcap_base64)
        pcap = []
        res_packets = []
        with tempfile.NamedTemporaryFile(mode='w+b') as tmpPcap:
            tmpPcap.write(pcap_bin)
            tmpPcap.flush()
            pcap = rdpcap(tmpPcap.name)
        for scapy_packet in pcap:
            res_packets.append(self._pkt_data(scapy_packet))
        return res_packets

    def write_pcap(self,client_v_handler,packets_base64):
        packets = [Ether(b64_to_bytes(pkt_b64)) for pkt_b64 in packets_base64]
        pcap_bin = None
        with tempfile.NamedTemporaryFile(mode='r+b') as tmpPcap:
            wrpcap(tmpPcap.name, packets)
            pcap_bin = tmpPcap.read()
        return bytes_to_b64(pcap_bin)

 


#---------------------------------------------------------------------------


