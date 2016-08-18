
import os
import sys
stl_pathname = os.path.abspath(os.path.join(os.pardir, os.pardir))
additional_stl_udp_pkts = os.path.abspath(os.path.join(os.pardir, os.pardir,'stl'))
sys.path.append(stl_pathname)
sys.path.append(additional_stl_udp_pkts)
import trex_stl_lib
from trex_stl_lib.api import *
from copy import deepcopy
import sys
import tempfile
import hashlib
import binascii
from pprint import pprint
from scapy.layers.dns import *
from udp_1pkt_vxlan import VXLAN
from udp_1pkt_mpls import MPLS

try:
    from cStringIO import StringIO
except ImportError:
    from io import StringIO




class Scapy_service_api():
    """ get_all(self) 

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
    def get_all(self):
        pass

    """ check_update(self,db_md5,field_md5) 
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

    def check_update(self,db_md5,field_md5):        
        pass

    """ build_pkt(self,pkt_descriptor) -> Dictionary (of Offsets,Show2 and Buffer)
        
        Performs calculations on the given packet and returns results for that packet.
    
        Parameters
        ----------
        pkt_descriptor - A string describing a network packet, in Scapy Format

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
    def build_pkt(self,pkt_descriptor):
        pass

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
    def get_tree(self):
        pass



class ScapyException(Exception): pass
class Scapy_service(Scapy_service_api):

#----------------------------------------------------------------------------------------------------
    class scapyRegex:
        def __init__(self,FieldName,regex='empty'):
            self.FieldName = FieldName
            self.regex = regex

        def stringRegex(self):
            return self.regex        
#----------------------------------------------------------------------------------------------------
    def __init__(self):
        self.Raw = {'Raw':''}
        self.high_level_protocols = ['Raw']
        self.transport_protocols = {'TCP':self.Raw,'UDP':self.Raw}
        self.network_protocols = {'IP':self.transport_protocols ,'ARP':''}
        self.low_level_protocols = { 'Ether': self.network_protocols }
        self.regexDB= {'MACField' : self.scapyRegex('MACField','^([0-9a-fA-F][0-9a-fA-F]:){5}([0-9a-fA-F][0-9a-fA-F])$'),
              'IPField' : self.scapyRegex('IPField','^(25[0-5]|2[0-4][0-9]|[0-1]{1}[0-9]{2}|[1-9]{1}[0-9]{1}|[1-9])\.(25[0-5]|2[0-4][0-9]|[0-1]{1}[0-9]{2}|[1-9]{1}[0-9]{1}|[1-9]|0)\.(25[0-5]|2[0-4][0-9]|[0-1]{1}[0-9]{2}|[1-9]{1}[0-9]{1}|[1-9]|0)\.(25[0-5]|2[0-4][0-9]|[0-1]{1}[0-9]{2}|[1-9]{1}[0-9]{1}|[0-9])$')}
        self.all_protocols = self._build_lib()
        self.protocol_tree = {'ALL':{'Ether':{'ARP':{},'IP':{'TCP':{'RAW':'payload'},'UDP':{'RAW':'payload'}}}}}
    

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
                fieldDict[f] = self.scapyRegex(f).stringRegex()
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
        return {'built_by':'itraviv','version':'v1.01'}

    def supported_methods(self,method_name=''):
        if method_name=='':
            methods = {}
            for f in dir(Scapy_service):
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

#--------------------------------------------API implementation-------------
    def get_tree(self):
        return self.protocol_tree

# pkt_descriptor in string format
    def build_pkt(self,pkt_descriptor):
        pkt = eval(pkt_descriptor)
        show2data = self._show2_to_dict(pkt)
        bufferData = str(pkt) #pkt buffer
        bufferData = binascii.b2a_base64(bufferData)
        pkt_offsets = self._get_all_pkt_offsets(pkt_descriptor)
        res = {}
        res['show2'] = show2data
        res['buffer'] = bufferData
        res['offsets'] = pkt_offsets
        return res

    def get_all(self):
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
    def check_update(self,db_md5,field_md5):
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


#---------------------------------------------------------------------------


