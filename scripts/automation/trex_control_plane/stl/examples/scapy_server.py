import stl_path
import trex_stl_lib
from trex_stl_lib.api import *
from copy import deepcopy
import sys
import tempfile
import md5

#print ls()

from cStringIO import StringIO
import sys
"""
old_stdout = sys.stdout
sys.stdout = mystdout = StringIO()

ls()

sys.stdout = old_stdout

a= mystdout.getvalue()

f = open('scapy_supported_formats.txt','w')
f.write(a)
f.close()
"""
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
#----------------------------------------------------------------------------------------------------


def build_lib():
    lib = protocol_struct()
    lib = lib.split('\n')
    all_protocols=[]
    for entry in lib:
        entry = entry.split(':')
        all_protocols.append(entry[0].strip())
    del all_protocols[len(all_protocols)-1]
    return all_protocols

def protocol_struct(protocol=''):
    if '_' in protocol:
        return []
    if not protocol=='':
        if protocol not in all_protocols:
            return 'protocol not supported'
        protocol = eval(protocol)
    old_stdout = sys.stdout
    sys.stdout = mystdout = StringIO()
    if not protocol=='':
        ls(protocol)
    else:
        ls()
    sys.stdout = old_stdout
    a= mystdout.getvalue()
#   print a
    return a

def parse_description_line(line):
    line_arr = [x.strip() for x in re.split(': | = ',line)]
    return tuple(line_arr)

def parse_entire_description(d):
    d = d.split('\n')
    description_list = [parse_description_line(x) for x in d]
    del description_list[len(description_list)-1]
    return description_list

def get_protocol_details(p_name):
    protocol_str = protocol_struct(p_name)
    if protocol_str=='protocol not supported':
        return 'protocol not supported'
    if len(protocol_str) is 0:
        return []
    tupled_protocol = parse_entire_description(protocol_str)
    return tupled_protocol


class scapyRegex:
    def __init__(self,FieldName,regex='empty'):
        self.FieldName = FieldName
        self.regex = regex

    def stringRegex(self):
        return self.regex

all_protocols = build_lib()

Raw = {'Raw':''}
high_level_protocols = ['Raw']
transport_protocols = {'TCP':Raw,'UDP':Raw}
network_protocols = {'IP':transport_protocols ,'ARP':''}
low_level_protocols = { 'Ether': network_protocols }
regexDB= {'MACField' : scapyRegex('MACField','^([0-9a-fA-F][0-9a-fA-F]:){5}([0-9a-fA-F][0-9a-fA-F])$'),
          'IPField' : scapyRegex('IPField','^(25[0-5]|2[0-4][0-9]|[0-1]{1}[0-9]{2}|[1-9]{1}[0-9]{1}|[1-9])\.(25[0-5]|2[0-4][0-9]|[0-1]{1}[0-9]{2}|[1-9]{1}[0-9]{1}|[1-9]|0)\.(25[0-5]|2[0-4][0-9]|[0-1]{1}[0-9]{2}|[1-9]{1}[0-9]{1}|[1-9]|0)\.(25[0-5]|2[0-4][0-9]|[0-1]{1}[0-9]{2}|[1-9]{1}[0-9]{1}|[0-9])$')}


def build_nodes(data,dictionary):
    n = node(data)
    if len(dictionary)==0:
        n.children=[]
        return n
    for obj in dictionary.keys():
        if not (obj==''):
            x = build_nodes(obj,dictionary[obj])
            n.children.append(x)
    return n

def build_tree():
    root = node('ALL')
    root.children = []
    root = build_nodes('ALL',low_level_protocols)
    return root

protocol_tree = build_tree()

def print_tree(root,level=0):
    output = "\t"*level+str(root.value)
    print output
    if len(root.children)==0:
        return
    for child in root.children:
        print_tree(child,level+1)

def get_all_protocols():
    return json.dumps(all_protocols)

def get_tree():
    old_stdout = sys.stdout
    sys.stdout = mystdout = StringIO()
    print_tree(t)
    sys.stdout = old_stdout
    a= mystdout.getvalue()
    return json.dumps(a)

def get_all_db():
    db = {}
    for pro in all_protocols:
        details = get_protocol_details(pro)
        db[pro] = json.dumps(details)
    return db

def get_all_fields():
    fields = []
    for pro in all_protocols:
        details = get_protocol_details(pro)
        for i in range(0,len(details),1):
            if len(details[i]) is 3:
                fields.append(details[i][1])
    uniqeFields = list(set(fields))
    fieldDict = {}
    for f in uniqeFields:
        if f in regexDB:
            fieldDict[f] = regexDB[f].stringRegex()
        else:
            fieldDict[f] = scapyRegex(f).stringRegex()
    return fieldDict


class pktResult:
    def __init__(self,result,errorCode,errorDesc):
        self.result = result
        self.errorCode = errorCode
        self.errorDesc = errorDesc
    
    def convert2tuple(self):
        return tuple([self.result,self.errorCode,self.errorDesc])



# pkt_descriptor in JSON format only!
# returned tuple in JSON format: (pktResultTuple , show2data, hexdump(buffer))
# pktResultTuple is: ( result, errorCode, error description )

def build_pkt(pkt_descriptor):
    try:
        pkt = eval(json.loads(pkt_descriptor))
        old_stdout = sys.stdout
        sys.stdout = mystdout = StringIO()
        pkt.show2()
        sys.stdout = old_stdout
        show2data = mystdout.getvalue() #show2 data
        bufferData = str(pkt) #pkt buffer
        bufferData = bufferData.encode('base64')
        pktRes = pktResult('Success',0,'None').convert2tuple()
        res = [pktRes,json.dumps(show2data),json.dumps(bufferData)]
        JSONres = json.dumps(res)
        return JSONres
    except:
        pktRes = pktResult('Pkt build Failed',str(sys.exc_info()[0]),str(sys.exc_info()[1])).convert2tuple()
        res = [pktRes,[],[]]
        res = json.dumps(res)
        return res

#input: container
#output: md5 in json format encoded in base64
def getMD5(container):
    resMD5 = md5.new(json.dumps(container))
    resMD5 = json.dumps(resMD5.digest().encode('base64'))
    return resMD5


def get_all():
    fields=get_all_fields()
    db=get_all_db()
    fieldMD5 = getMD5(fields)
    dbMD5 = getMD5(db)
    res = {}
    res['db'] = db
    res['fields'] = fields
    res['DB_md5'] = dbMD5
#   print dbMD5
    res['fields_md5'] = fieldMD5
    return json.dumps(res)

#input in json string encoded base64
def check_update(dbMD5,fieldMD5):
    fields=get_all_fields()
    db=get_all_db()
    currentDBMD5 = json.loads(getMD5(db))
    currentFieldMD5 = json.loads(getMD5(fields))
    dbMD5_parsed = json.loads(dbMD5)
    fieldMD5_parsed = json.loads(fieldMD5)
    res = []
#   print 'this is current DB MD5: %s ' % currentDBMD5
#   print 'this is given DB MD5: %s ' % dbMD5_parsed
    if fieldMD5_parsed == currentFieldMD5:
        resField = pktResult('Success',0,'None').convert2tuple()
    else:
        resField = pktResult('Fail',-1,'Field DB is not up to date').convert2tuple()
    if dbMD5_parsed == currentDBMD5:
        resDB = pktResult('Success',0,'None').convert2tuple()
    else:
        resDB = pktResult('Fail',-1,'Protocol DB is not up to date').convert2tuple()
    return json.dumps([resField,resDB])

#pkt_desc as json
#dictionary of offsets per protocol. tuple for each field: (name, offset, size) at json format
def get_all_pkt_offsets(pkt_desc):
    pkt_desc= json.loads(pkt_desc)
    try:
        pkt_protocols = pkt_desc.split('/')
        scapy_pkt = eval(pkt_desc)
        total_protocols = len(pkt_protocols)
        res = {}
        for i in range(total_protocols):
            fields = []
            for field in scapy_pkt.fields_desc:
                size = field.get_size_bytes()
                if field.name is 'load':
                    size = len(scapy_pkt)
                fields.append([field.name, field.offset, size])
            res[pkt_protocols[i]] = fields
            scapy_pkt=scapy_pkt.payload
        pktRes = pktResult('Success',0,'None').convert2tuple()
        FinalRes = [pktRes,res]
        return json.dumps(FinalRes)
    except:
        pktRes = pktResult('Pkt build Failed',str(sys.exc_info()[0]),str(sys.exc_info()[1])).convert2tuple()
        res = [pktRes,{}]
        res = json.dumps(res)
        return res





