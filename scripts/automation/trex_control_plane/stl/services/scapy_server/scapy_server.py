
import os
import sys
stl_pathname = os.path.abspath(os.path.join(os.pardir, os.pardir))
sys.path.append(stl_pathname)
import trex_stl_lib
from trex_stl_lib.api import *
from copy import deepcopy
import sys
import tempfile
import hashlib
import binascii


try:
    from cStringIO import StringIO
except ImportError:
    from io import StringIO

"""     
                            **** output redirection template ****
old_stdout = sys.stdout
sys.stdout = mystdout = StringIO()

ls()

sys.stdout = old_stdout

a= mystdout.getvalue()

f = open('scapy_supported_formats.txt','w')
f.write(a)
f.close()
"""

class ScapyException(Exception): pass

class scapy_service:

#----------------------------------------------------------------------------------------------------
    class scapyRegex:
        def __init__(self,FieldName,regex='empty'):
            self.FieldName = FieldName
            self.regex = regex

        def stringRegex(self):
            return self.regex

    class pktResult(BaseException):
        def __init__(self,result,errorCode,errorDesc):
            self.result = result
            self.errorCode = errorCode
            self.errorDesc = errorDesc
        
        def convert2tuple(self):
            return tuple([self.result,self.errorCode,self.errorDesc])
#----------------------------------------------------------------------------------------------------

    def __init__(self):
        self.Raw = {'Raw':''}
        self.high_level_protocols = ['Raw']
        self.transport_protocols = {'TCP':self.Raw,'UDP':self.Raw}
        self.network_protocols = {'IP':self.transport_protocols ,'ARP':''}
        self.low_level_protocols = { 'Ether': self.network_protocols }
        self.regexDB= {'MACField' : self.scapyRegex('MACField','^([0-9a-fA-F][0-9a-fA-F]:){5}([0-9a-fA-F][0-9a-fA-F])$'),
              'IPField' : self.scapyRegex('IPField','^(25[0-5]|2[0-4][0-9]|[0-1]{1}[0-9]{2}|[1-9]{1}[0-9]{1}|[1-9])\.(25[0-5]|2[0-4][0-9]|[0-1]{1}[0-9]{2}|[1-9]{1}[0-9]{1}|[1-9]|0)\.(25[0-5]|2[0-4][0-9]|[0-1]{1}[0-9]{2}|[1-9]{1}[0-9]{1}|[1-9]|0)\.(25[0-5]|2[0-4][0-9]|[0-1]{1}[0-9]{2}|[1-9]{1}[0-9]{1}|[0-9])$')}
        self.all_protocols = self.build_lib()
        self.protocol_tree = self.build_tree()
    
#-------------------------------------------------------TREE IMPLEMENTATION ------------------------
    class node(object):
        def __init__(self, value):#, children = []):
            self.value = value
            self.children = []

        def __str__(self, level=0):
            ret = "\t"*level+repr(self.value)+"\n"
            for child in self.children:
                ret += child.__str__(level+1)
            return ret

    def build_nodes(self,data,dictionary):
        n = self.node(data)
        if len(dictionary)==0:
            n.children=[]
            return n
        for obj in dictionary.keys():
            if not (obj==''):
                x = self.build_nodes(obj,dictionary[obj])
                n.children.append(x)
        return n

    def build_tree(self):
        root = self.node('ALL')
        root.children = []
        root = self.build_nodes('ALL',self.low_level_protocols)
        return root

    def protocol_struct(self,protocol=''):
        if '_' in protocol:
            return []
        if not protocol=='':
            if protocol not in self.all_protocols:
                return 'protocol not supported'
            protocol = eval(protocol)
        old_stdout = sys.stdout
        sys.stdout = mystdout = StringIO()
        if not protocol=='':
            ls(protocol)
        else:
            ls()
        sys.stdout = old_stdout
        protocol_data= mystdout.getvalue()
        return protocol_data

    def build_lib(self):
        lib = self.protocol_struct()
        lib = lib.split('\n')
        all_protocols=[]
        for entry in lib:
            entry = entry.split(':')
            all_protocols.append(entry[0].strip())
        del all_protocols[len(all_protocols)-1]
        return all_protocols

    def parse_description_line(self,line):
        line_arr = [x.strip() for x in re.split(': | = ',line)]
        return tuple(line_arr)

    def parse_entire_description(self,description):
        description = description.split('\n')
        description_list = [self.parse_description_line(x) for x in description]
        del description_list[len(description_list)-1]
        return description_list

    def get_protocol_details(self,p_name):
        protocol_str = self.protocol_struct(p_name)
        if protocol_str=='protocol not supported':
            return 'protocol not supported'
        if len(protocol_str) is 0:
            return []
        tupled_protocol = self.parse_entire_description(protocol_str)
        return tupled_protocol

    def print_tree_level(self,root,level=0):
        output = "\t"*level+str(root.value)
        print (output)
        if len(root.children)==0:
            return
        for child in root.children:
            self.print_tree_level(child,level+1)

    def print_tree(self):
        self.print_tree_level(self.protocol_tree)

    def get_all_protocols(self):
        return self.all_protocols

    def get_tree(self):
        old_stdout = sys.stdout
        sys.stdout = mystdout = StringIO()
        print_tree(t)
        sys.stdout = old_stdout
        a= mystdout.getvalue()
        return a

    def get_all_db(self):
        db = {}
        for pro in self.all_protocols:
            details = self.get_protocol_details(pro)
            db[pro] = details
        return db

    def get_all_fields(self):
        fields = []
        for pro in self.all_protocols:
            details = self.get_protocol_details(pro)
            for i in range(0,len(details),1):
                if len(details[i]) is 3:
                    fields.append(details[i][1])
        uniqeFields = list(set(fields))
        fieldDict = {}
        for f in uniqeFields:
            if f in self.regexDB:
                fieldDict[f] = self.regexDB[f].stringRegex()
            else:
                fieldDict[f] = self.scapyRegex(f).stringRegex()
        return fieldDict

    def show2_to_dict(self,pkt):
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
    def get_all_pkt_offsets(self,pkt_desc):
        pkt_protocols = pkt_desc.split('/')
        scapy_pkt = eval(pkt_desc)
        total_protocols = len(pkt_protocols)
        res = {}
        for i in range(total_protocols):
            fields = {}
            for field in scapy_pkt.fields_desc:
                size = field.get_size_bytes()
                if field.name is 'load':
                    size = len(scapy_pkt)
                fields[field.name]=[field.offset, size]
            layer_name = pkt_protocols[i].partition('(')[0] #clear layer name to include only alpha-numeric
            layer_name = re.sub(r'\W+', '',layer_name)
            res[layer_name] = fields
            scapy_pkt=scapy_pkt.payload
        return res


# pkt_descriptor in string format

    def build_pkt(self,pkt_descriptor):
        pkt = eval(pkt_descriptor)
        show2data = self.show2_to_dict(pkt)
        bufferData = str(pkt) #pkt buffer
        bufferData = binascii.b2a_base64(bufferData)
        pkt_offsets = self.get_all_pkt_offsets(pkt_descriptor)
        res = {}
        res['show2'] = show2data
        res['buffer'] = bufferData
        res['offsets'] = pkt_offsets
        return res


#input: container
#output: md5 encoded in base64
    def get_md5(self,container):
        container = json.dumps(container)
        m = hashlib.md5()
        m.update(container.encode('ascii'))
        res_md5 = binascii.b2a_base64(m.digest())
        return res_md5


    def get_all(self):
        fields=self.get_all_fields()
        db=self.get_all_db()
        fields_md5 = self.get_md5(fields)
        db_md5 = self.get_md5(db)
        res = {}
        res['db'] = db
        res['fields'] = fields
        res['db_md5'] = db_md5
        res['fields_md5'] = fields_md5
        return res

#input in string encoded base64
    def check_update(self,db_md5,field_md5):
        fields=self.get_all_fields()
        db=self.get_all_db()
        current_db_md5 = self.get_md5(db)
        current_field_md5 = self.get_md5(fields)
        res = []
        if field_md5 == current_field_md5:
            if db_md5 == current_db_md5:
                return True
            else:
                raise ScapyException("Protocol DB is not up to date")
        else:
            raise ScapyException("Fields DB is not up to date")

    def get_version(self):
        return {'built_by':'itraviv','version':'v1.0'}


    def supported_methods(self,method_name):
        if method_name in dir(scapy_service):
            return True
        return False


