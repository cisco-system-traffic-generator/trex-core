
import os
import sys
stl_pathname = os.path.abspath(os.path.join(os.pardir, os.pardir))
sys.path.append(stl_pathname)

from trex_stl_lib.api import *
import tempfile
import hashlib
import binascii
from pprint import pprint
#from scapy.layers.dns import DNS
#from scapy.contrib.mpls import MPLS

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
    
    def reconstruct_pkt(self,client_v_handler,binary_pkt):
        """ reconstruct_pkt(self,client_v_handler,binary_pkt)

        Makes a Scapy valid packet from a binary string and returns all information returned in build_pkt

        Parameters
        ----------
        Packet in binary, formatted in "base64" encoding

        Returns
        -------
        All data provided in build_pkt:
        show2 - detailed description of the packet
        buffer - the packet presented in binary
        offsets - the offset[in bytes] of each field in the packet

        """
        pass


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

    def _show2_to_dict(self,pkt):
        old_stdout = sys.stdout
        sys.stdout = mystdout = StringIO()
        pkt.show2()
        sys.stdout = old_stdout
        show2data = mystdout.getvalue() #show2 data
        listedShow2Data = show2data.split('###')
        show2Dict = {}
        for i in range(1,len(listedShow2Data)-1,2):
            protocol_fields = listedShow2Data[i+1]
            protocol_fields = protocol_fields.split('\n')[1:-1]
            protocol_fields = [f.strip() for f in protocol_fields]
            protocol_fields_dict = {}
            for f in protocol_fields:
                field_data = f.split('=')
                if len(field_data)!= 1 :
                    field_name = field_data[0].strip()
                    protocol_fields_dict[field_name] = field_data[1].strip()
            layer_name = re.sub(r'\W+', '',listedShow2Data[i]) #clear layer name to include only alpha-numeric
            show2Dict[layer_name] = protocol_fields_dict
        return show2Dict

#pkt_desc as string
#dictionary of offsets per protocol. tuple for each field: (name, offset, size) at json format
    def _get_all_pkt_offsets(self,pkt_desc):
        pkt_protocols = pkt_desc.split('/')
        scapy_pkt = eval(pkt_desc)
        scapy_pkt.build()
        total_protocols = len(pkt_protocols)
        res = {}
        for i in range(total_protocols):
            fields = {}
            for field in scapy_pkt.fields_desc:
                size = field.get_size_bytes()
                layer_name = pkt_protocols[i].partition('(')[0] #clear layer name to include only alpha-numeric
                layer_name = re.sub(r'\W+', '',layer_name)
                if field.name is 'load':
                    layer_name ='Raw'
                    size = len(scapy_pkt)
                fields[field.name]=[field.offset, size]
            fields['global_offset'] = scapy_pkt.offset
            res[layer_name] = fields
            scapy_pkt=scapy_pkt.payload
        return res

#input: container
#output: md5 encoded in base64
    def _get_md5(self,container):
        container = json.dumps(container)
        m = hashlib.md5()
        m.update(container.encode('ascii'))
        res_md5 = binascii.b2a_base64(m.digest())
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
        m.update(v_for_hash)
        v_handle = binascii.b2a_base64(m.digest())
        return unicode(v_handle,"utf-8")

    def _generate_invalid_version_error(self):
        error_desc1 = "Provided version handler does not correspond to the server's version.\nUpdate client to latest version.\nServer version:"+self.version_major+"."+self.version_minor
        return error_desc1

    def _verify_version_handler(self,client_v_handler):
        return (self.server_v_hashed == client_v_handler)

    def _parse_packet_dict(self,layer,scapy_layers,scapy_layer_names):
        class_name = scapy_layer_names.index(layer['id'])
        class_p = scapy_layers[class_name] # class pointer
        kwargs = {}
        if 'Fields' in layer:
            for field in layer['Fields']:
                key = field['id']
                value = field['value']
                if type(value) is list:
                    resolved_value = []
                    for arg_class in value:
                        option_class_p = scapy.all.__dict__[arg_class["class"]]
                        option_kwargs = {}
                        for field in arg_class['Fields']:
                            option_kwargs[field['id']] = field['value']
                        resolved_value.append(option_class_p(**option_kwargs))
                    value = resolved_value
                kwargs[key] = value
        return class_p(**kwargs)

    def _packet_model_to_scapy_packet(self,data):
        layers = Packet.__subclasses__()
        layer_names = [ layer.__name__ for layer in layers]
        base_layer = self._parse_packet_dict(data[0],layers,layer_names)
        for i in range(1,len(data),1):
            packet_layer = self._parse_packet_dict(data[i],layers,layer_names)
            base_layer = base_layer/packet_layer
        return base_layer


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
        show2data = self._show2_to_dict(pkt)
        bufferData = str(pkt) #pkt buffer
        bufferData = binascii.b2a_base64(bufferData)
        pkt_offsets = self._get_all_pkt_offsets(pkt.command())
        res = {}
        res['show2'] = show2data
        res['buffer'] = bufferData
        res['offsets'] = pkt_offsets
        return res

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

#input in string encoded base64
    def check_update_of_dbs(self,client_v_handler,db_md5,field_md5):
        if not (self._verify_version_handler(client_v_handler)):
            raise ScapyException(self._generate_invalid_version_error())
        fields=self._get_all_fields()
        db=self._get_all_db()
        current_db_md5 = self._get_md5(db)
        current_field_md5 = self._get_md5(fields)
        res = []
        if (field_md5.decode("base64") == current_field_md5.decode("base64")):
            if (db_md5.decode("base64") == current_db_md5.decode("base64")):
                return True
            else:
                raise ScapyException("Protocol DB is not up to date")
        else:
            raise ScapyException("Fields DB is not up to date")

#input of binary_pkt must be encoded in base64
    def reconstruct_pkt(self,client_v_handler,binary_pkt):
        pkt_in_hex = binary_pkt.decode('base64')
        scapy_pkt = Ether(pkt_in_hex)
        scapy_pkt_cmd = scapy_pkt.command()
        return self.build_pkt(client_v_handler,scapy_pkt_cmd)


 


#---------------------------------------------------------------------------


