import external_packages
import random
import string
import struct
import socket       
import json
import yaml
import binascii
import base64

from packet_builder_interface import CTrexPktBuilderInterface

from scapy.all import *



class CTRexPacketBuildException(Exception):
    """
    This is the general Packet Building error exception class.
    """
    def __init__(self, code, message):
        self.code = code
        self.message = message

    def __str__(self):
        return self.__repr__()

    def __repr__(self):
        return u"[errcode:%r] %r" % (self.code, self.message)

################################################################################################

def ipv4_str_to_num (ipv4_buffer):

    assert type(ipv4_buffer)==str, 'type of ipv4_buffer is not str'
    assert len(ipv4_buffer)==4, 'size of ipv4_buffer is not 4'
    res=0
    shift=24
    for i in ipv4_buffer:
       res = res + (ord(i)<<shift);
       shift =shift -8
    return res



def is_valid_ipv4(ip_addr):
    """
    return buffer in network order 
    """
    if  type(ip_addr)==str and len(ip_addr) == 4:
        return ip_addr

    if  type(ip_addr)==int :
        ip_addr = socket.inet_ntoa(struct.pack("!I", ip_addr)) 

    try:
        return socket.inet_pton(socket.AF_INET, ip_addr)
    except AttributeError:  # no inet_pton here, sorry
        return socket.inet_aton(ip_addr)
    except socket.error:  # not a valid address
        raise CTRexPacketBuildException(-10,"not valid ipv4 format");


class CTRexScriptsBase(object):
    """
    VM Script base class 
    """
    def clone (self):
        return copy.deepcopy(self)


class CTRexScFieldRangeBase(CTRexScriptsBase):

    FILED_TYPES = ['inc', 'dec', 'rand']

    def __init__(self, field_name,
                       field_type
                            ):
        super(CTRexScFieldRangeBase, self).__init__()
        self.field_name =field_name
        self.field_type =field_type
        if not self.field_type in CTRexScFieldRangeBase.FILED_TYPES :
            raise CTRexPacketBuildException(-12, 'field type should be in %s' % FILED_TYPES);


class CTRexScFieldRangeValue(CTRexScFieldRangeBase):
    """
    range of field value
    """
    def __init__(self, field_name, 
                       field_type,
                       min_value,
                       max_value
                           ):
        super(CTRexScFieldRangeValue, self).__init__(field_name,field_type)
        self.min_value =min_value;
        self.max_value =max_value;
        if min_value > max_value:
            raise CTRexPacketBuildException(-12, 'min is greater than max');
        if min_value == max_value:
            raise CTRexPacketBuildException(-13, "min value is equal to max value, you can't use this type of range");


class CTRexScIpv4SimpleRange(CTRexScFieldRangeBase):
    """
    range of ipv4 ip
    """
    def __init__(self, field_name, field_type, min_ip, max_ip):
        super(CTRexScIpv4SimpleRange, self).__init__(field_name,field_type)
        self.min_ip = min_ip
        self.max_ip = max_ip
        mmin=ipv4_str_to_num (is_valid_ipv4(min_ip))
        mmax=ipv4_str_to_num (is_valid_ipv4(max_ip))
        if  mmin > mmax :
            raise CTRexPacketBuildException(-11, 'CTRexScIpv4SimpleRange m_min ip is bigger than max');


class CTRexScIpv4TupleGen(CTRexScriptsBase):
    """
    range tuple 
    """
    FLAGS_ULIMIT_FLOWS =1

    def __init__(self, min_ipv4, max_ipv4, num_flows=100000, min_port=1025, max_port=65535, flags=0):
        super(CTRexScIpv4TupleGen, self).__init__()
        self.min_ip = min_ipv4
        self.max_ip = max_ipv4
        mmin=ipv4_str_to_num (is_valid_ipv4(min_ipv4))
        mmax=ipv4_str_to_num (is_valid_ipv4(max_ipv4))
        if  mmin > mmax :
            raise CTRexPacketBuildException(-11, 'CTRexScIpv4SimpleRange m_min ip is bigger than max');

        self.num_flows=num_flows;

        self.min_port =min_port
        self.max_port =max_port
        self.flags =   flags


class CTRexScTrimPacketSize(CTRexScriptsBase):
    """
    trim packet size. field type is CTRexScFieldRangeBase.FILED_TYPES = ["inc","dec","rand"]  
    """
    def __init__(self,field_type="rand",min_pkt_size=None, max_pkt_size=None):
        super(CTRexScTrimPacketSize, self).__init__()
        self.field_type = field_type
        self.min_pkt_size = min_pkt_size
        self.max_pkt_size = max_pkt_size
        if max_pkt_size != None and min_pkt_size !=None :
            if min_pkt_size == max_pkt_size:
                raise CTRexPacketBuildException(-11, 'CTRexScTrimPacketSize min_pkt_size is the same as max_pkt_size ');

            if min_pkt_size > max_pkt_size:
                raise CTRexPacketBuildException(-11, 'CTRexScTrimPacketSize min_pkt_size is bigger than max_pkt_size ');

class CTRexScRaw(CTRexScriptsBase):
    """
    raw instructions 
    """
    def __init__(self,list_of_commands=None):
        super(CTRexScRaw, self).__init__()
        if list_of_commands==None:
            self.commands =[]
        else:
            self.commands = list_of_commands

    def add_cmd (self,cmd):
        self.commands.append(cmd)



################################################################################################
# VM raw instructions 
################################################################################################

class CTRexVmInsBase(object):
    """
    instruction base
    """
    def __init__(self, ins_type):
        self.type = ins_type
        assert type(ins_type)==str, 'type of ins_type is not str'

class CTRexVmInsFixIpv4(CTRexVmInsBase):
    def __init__(self, offset):
        super(CTRexVmInsFixIpv4, self).__init__("fix_checksum_ipv4")
        self.pkt_offset = offset
        assert type(offset)==int, 'type of offset is not int'


class CTRexVmInsFlowVar(CTRexVmInsBase):
    #TBD add more validation tests

    OPERATIONS =['inc', 'dec', 'random']
    VALID_SIZES =[1, 2, 4, 8]

    def __init__(self, fv_name, size, op, init_value, min_value, max_value):
        super(CTRexVmInsFlowVar, self).__init__("flow_var")
        self.name = fv_name;
        assert type(fv_name)==str, 'type of fv_name is not str'
        self.size = size
        self.op = op
        self.init_value = init_value
        assert type(init_value)==int, 'type of init_value is not int'
        self.min_value=min_value
        assert type(min_value)==int, 'type of min_value is not int'
        self.max_value=max_value
        assert type(max_value)==int, 'type of min_value is not int'

class CTRexVmInsWrFlowVar(CTRexVmInsBase):
    def __init__(self, fv_name, pkt_offset, add_value=0, is_big_endian=True):
        super(CTRexVmInsWrFlowVar, self).__init__("write_flow_var")
        self.name = fv_name
        assert type(fv_name)==str, 'type of fv_name is not str'
        self.pkt_offset = pkt_offset
        assert type(pkt_offset)==int, 'type of pkt_offset is not int'
        self.add_value = add_value
        assert type(add_value)==int, 'type of add_value is not int'
        self.is_big_endian = is_big_endian
        assert type(is_big_endian)==bool, 'type of is_big_endian is not bool'

class CTRexVmInsTrimPktSize(CTRexVmInsBase):
    def __init__(self,fv_name):
        super(CTRexVmInsTrimPktSize, self).__init__("trim_pkt_size")
        self.name = fv_name
        assert type(fv_name)==str, 'type of fv_name is not str'

class CTRexVmInsTupleGen(CTRexVmInsBase):
    def __init__(self, fv_name, ip_min, ip_max, port_min, port_max, limit_flows, flags=0):
        super(CTRexVmInsTupleGen, self).__init__("tuple_flow_var")
        self.name =fv_name
        assert type(fv_name)==str, 'type of fv_name is not str'
        self.ip_min = ip_min;
        self.ip_max = ip_max;
        self.port_min = port_min;
        self.port_max = port_max;
        self.limit_flows = limit_flows;
        self.flags       =flags;


################################################################################################
# 
class CTRexVmEngine(object):

       def __init__(self):
            """
            inlcude list of instruction
            """
            super(CTRexVmEngine, self).__init__()
            self.ins=[]
            self.split_by_var = ''

       # return as json 
       def get_json (self):
           inst_array = [];
           # dump it as dict
           for obj in self.ins:
               inst_array.append(obj.__dict__);

           return {'instructions': inst_array, 'split_by_var': self.split_by_var}

       def add_ins (self,ins):
           #assert issubclass(ins, CTRexVmInsBase)
           self.ins.append(ins);

       def dump (self):
           cnt=0;
           for obj in self.ins:
               print "ins",cnt
               cnt = cnt +1
               print obj.__dict__

       def dump_bjson (self):
          print json.dumps(self.get_json(), sort_keys=True, indent=4)

       def dump_as_yaml (self):
          print yaml.dump(self.get_json(), default_flow_style=False)



################################################################################################

class CTRexScapyPktUtl(object):

    def __init__(self, scapy_pkt):
        self.pkt = scapy_pkt

    def pkt_iter (self):
        p=self.pkt;
        while True:
            yield p
            p=p.payload
            if p ==None or isinstance(p,NoPayload):
                break;

    def get_list_iter(self):
        l=list(self.pkt_iter())
        return l


    def get_pkt_layers(self):
        """
        return string 'IP:UDP:TCP'
        """
        l=self.get_list_iter ();
        l1=map(lambda p: p.name,l );
        return ":".join(l1);

    def _layer_offset(self, name, cnt = 0):
        """
        return offset of layer e.g 'IP',1 will return offfset of layer ip:1 
        """
        save_cnt=cnt
        for pkt in self.pkt_iter ():
            if pkt.name == name:
                if cnt==0:
                    return (pkt, pkt.offset)
                else:
                    cnt=cnt -1

        raise CTRexPacketBuildException(-11,("no layer %s-%d" % (name, save_cnt)));


    def layer_offset(self, name, cnt = 0):
        """
        return offset of layer e.g 'IP',1 will return offfset of layer ip:1 
        """
        save_cnt=cnt
        for pkt in self.pkt_iter ():
            if pkt.name == name:
                if cnt==0:
                    return pkt.offset
                else:
                    cnt=cnt -1

        raise CTRexPacketBuildException(-11,("no layer %s-%d" % (name, save_cnt)));

    def get_field_offet(self, layer, layer_cnt, field_name):
        """
        return offset of layer e.g 'IP',1 will return offfset of layer ip:1 
        """
        t=self._layer_offset(layer,layer_cnt);
        l_offset=t[1];
        layer_pkt=t[0]

        #layer_pkt.dump_fields_offsets ()

        for f in layer_pkt.fields_desc:
            if f.name == field_name:
                return (l_offset+f.offset,f.get_size_bytes ());

        raise CTRexPacketBuildException(-11, "no layer %s-%d." % (name, save_cnt, field_name));

    def get_layer_offet_by_str(self, layer_des):
        """
        return layer offset by string  

       :parameters:

        IP:0
        IP:1
        return offset


        """
        l1=layer_des.split(":")
        layer=""
        layer_cnt=0;

        if len(l1)==1:
            layer=l1[0];
        else:
            layer=l1[0];
            layer_cnt=int(l1[1]);

        return self.layer_offset(layer, layer_cnt)



    def get_field_offet_by_str(self, field_des):
        """
        return field_des (offset,size) layer:cnt.field 
        for example 
        802|1Q.vlan get 802.1Q->valn replace | with .
        IP.src
        IP:0.src  (first IP.src like IP.src)
        for example IP:1.src  for internal IP

        return (offset, size) as tuple 


        """

        s=field_des.split(".");
        if len(s)!=2:
            raise CTRexPacketBuildException(-11, ("field desription should be layer:cnt.field e.g IP.src or IP:1.src"));


        layer_ex = s[0].replace("|",".")
        field = s[1]

        l1=layer_ex.split(":")
        layer=""
        layer_cnt=0;

        if len(l1)==1:
            layer=l1[0];
        else:
            layer=l1[0];
            layer_cnt=int(l1[1]);

        return self.get_field_offet(layer,layer_cnt,field)

    def has_IPv4 (self):
        return self.pkt.has_layer("IP");

    def has_IPv6 (self):
        return self.pkt.has_layer("IPv6");

    def has_UDP (self):
        return self.pkt.has_layer("UDP");

################################################################################################

class CTRexVmDescBase(object):
    """
    instruction base
    """
    def __init__(self):
       pass;

    def get_obj(self):
        return self;

    def get_json(self):
        return self.get_obj().__dict__

    def dump_bjson(self):
       print json.dumps(self.get_json(), sort_keys=True, indent=4)

    def dump_as_yaml(self):
       print yaml.dump(self.get_json(), default_flow_style=False)


    def get_var_ref (self):
        '''
          virtual function return a ref var name
        ''' 
        return None

    def get_var_name(self):
        '''
          virtual function return the varible name if exists
        ''' 
        return None

    def compile(self,parent): 
        '''
          virtual function to take parent than has function name_to_offset
        ''' 
        pass;


def valid_fv_size (size):
    if not (size in CTRexVmInsFlowVar.VALID_SIZES):
        raise CTRexPacketBuildException(-11,("flow var has not valid size %d ") % size  );

def valid_fv_ops (op):
    if not (op in CTRexVmInsFlowVar.OPERATIONS):
        raise CTRexPacketBuildException(-11,("flow var does not have a valid op %s ") % op  );

def convert_val (val):
    if type(val) == int:
        return val
    else:
        if type(val) == str:
          return ipv4_str_to_num (is_valid_ipv4(val))
        else:
            raise CTRexPacketBuildException(-11,("init val not valid %s ") % val  );

def check_for_int (val):
    assert type(val)==int, 'type of vcal is not int'


class CTRexVmDescFlowVar(CTRexVmDescBase):
    def __init__(self, name, init_value=None, min_value=0, max_value=255, size=4, op="inc"):
        super(CTRexVmDescFlowVar, self).__init__()
        self.name = name;
        assert type(name)==str, 'type of name is not str'
        self.size =size
        valid_fv_size(size)
        self.op =op
        valid_fv_ops (op)

        # choose default value for init val
        if init_val == None:
            init_val = max_val if op == "dec" else min_val

        self.init_value = convert_val (init_value)
        self.min_value  = convert_val (min_value);
        self.max_value  = convert_val (max_value)

        if self.min_value > self.max_value :
            raise CTRexPacketBuildException(-11,("max %d is lower than min %d ") % (self.max_value,self.min_value)  );

    def get_obj (self):
        return CTRexVmInsFlowVar(self.name,self.size,self.op,self.init_value,self.min_value,self.max_value);

    def get_var_name(self):
        return [self.name]


class CTRexVmDescFixIpv4(CTRexVmDescBase):
    def __init__(self, offset):
        super(CTRexVmDescFixIpv4, self).__init__()
        self.offset = offset; # could be a name of offset 

    def get_obj (self):
        return CTRexVmInsFixIpv4(self.offset);

    def compile(self,parent): 
        if type(self.offset)==str:
            self.offset = parent._pkt_layer_offset(self.offset);

class CTRexVmDescWrFlowVar(CTRexVmDescBase):
    def __init__(self, fv_name, pkt_offset, offset_fixup=0, add_val=0, is_big=True):
        super(CTRexVmDescWrFlowVar, self).__init__()
        self.name =fv_name
        assert type(fv_name)==str, 'type of fv_name is not str'
        self.offset_fixup =offset_fixup
        assert type(offset_fixup)==int, 'type of offset_fixup is not int'
        self.pkt_offset =pkt_offset
        self.add_val =add_val
        assert type(add_val)==int,'type of add_val is not int'
        self.is_big =is_big;
        assert type(is_big)==bool,'type of is_big_endian is not bool'

    def get_var_ref (self):
        return self.name

    def get_obj (self):
            return  CTRexVmInsWrFlowVar(self.name,self.pkt_offset+self.offset_fixup,self.add_val,self.is_big)

    def compile(self,parent): 
        if type(self.pkt_offset)==str:
            t=parent._name_to_offset(self.pkt_offset)
            self.pkt_offset = t[0]


class CTRexVmDescTrimPktSize(CTRexVmDescBase):
    def __init__(self,fv_name):
        super(CTRexVmDescTrimPktSize, self).__init__()
        self.name = fv_name
        assert type(fv_name)==str, 'type of fv_name is not str'

    def get_var_ref (self):
        return self.name

    def get_obj (self):
        return  CTRexVmInsTrimPktSize(self.name)



class CTRexVmDescTupleGen(CTRexVmDescBase):
    def __init__(self,name, ip_min="0.0.0.1", ip_max="0.0.0.10", port_min=1025, port_max=65535, limit_flows=100000, flags=0):
        super(CTRexVmDescTupleGen, self).__init__()
        self.name = name
        assert type(name)==str, 'type of fv_name is not str'
        self.ip_min = convert_val(ip_min);
        self.ip_max = convert_val(ip_max);
        self.port_min = port_min;
        check_for_int (port_min)
        self.port_max = port_max;
        check_for_int(port_max)
        self.limit_flows = limit_flows;
        check_for_int(limit_flows)
        self.flags       =flags;
        check_for_int(flags)

    def get_var_name(self):
        return [self.name+".ip",self.name+".port"]

    def get_obj (self):
        return  CTRexVmInsTupleGen(self.name, self.ip_min, self.ip_max, self.port_min, self.port_max, self.limit_flows, self.flags);


################################################################################################


class CScapyTRexPktBuilder(CTrexPktBuilderInterface):

    """
    This class defines the TRex API of building a packet using dpkt package.
    Using this class the user can also define how TRex will handle the packet by specifying the VM setting.
    """
    def __init__(self, pkt = None, vm = None):
        """
        Instantiate a CTRexPktBuilder object

        :parameters:
             None

        """
        super(CScapyTRexPktBuilder, self).__init__()

        self.pkt = None
        self.vm_scripts = [] # list of high level instructions
        self.vm_low_level = None
        self.metadata=""


        # process packet
        if pkt != None:
            if not isinstance(pkt, Packet):
                raise CTRexPacketBuildException(-14, "bad value for variable pkt")
            self.set_packet(pkt)

        # process VM
        if vm != None:
            if not isinstance(vm, (CTRexScRaw, list)):
                raise CTRexPacketBuildException(-14, "bad value for variable pkt")

            self.add_command(vm if isinstance(vm, CTRexScRaw) else CTRexScRaw(vm))


    def dump_vm_data_as_yaml(self):
       print yaml.dump(self.get_vm_data(), default_flow_style=False)

    def get_vm_data(self):
        """
        Dumps the instructions 

        :parameters:
            None

        :return:
            + json object of instructions

        :raises:
            + :exc:`AssertionError`, in case VM is not compiled (is None).
        """

        assert self.vm_low_level is not None, 'vm_low_level is None, please use compile()'

        return self.vm_low_level.get_json() 

    def dump_pkt(self, encode = True):
        """
        Dumps the packet as a decimal array of bytes (each item x gets value between 0-255)

        :parameters:
            encode : bool
                Encode using base64. (disable for debug)

                Default: **True**

        :return:
            + packet representation as array of bytes

        :raises:
            + :exc:`AssertionError`, in case packet is empty.

        """

        assert self.pkt, 'empty packet'

        return {'binary': base64.b64encode(str(self.pkt)) if encode else str(self.pkt),
                'meta': self.metadata}

    def dump_pkt_to_pcap(self, file_path):
        wrpcap(file_path, self.pkt)

    def add_command (self, script):
        self.vm_scripts.append(script.clone());

    def dump_scripts (self):
        self.vm_low_level.dump_as_yaml()

    def set_packet (self, pkt):
        """
        Scapy packet   Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)/IP()/"A"*10
        """
        self.pkt = pkt;


    def compile (self):
        self.vm_low_level=CTRexVmEngine()
        assert self.pkt, 'empty packet'
        self.pkt.build();

        
        for sc in self.vm_scripts:
            if isinstance(sc, CTRexScRaw):
                self._compile_raw(sc)

        #for obj in self.vm_scripts:
        #    # tuple gen script 
        #    if isinstance(obj, CTRexScIpv4TupleGen)
        #        self._add_tuple_gen(tuple_gen)

    ####################################################
    # private 

    def _compile_raw (self,obj):

        # make sure we have varibles once 
        vars={};

        # add it add var to dit
        for desc in obj.commands:
            var_names =  desc.get_var_name()

            if var_names :
                print var_names
                for var_name in var_names:
                    if vars.has_key(var_name):
                        raise CTRexPacketBuildException(-11,("variable %s define twice ") % (var_name)  );
                    else:
                        vars[var_name]=1

        # check that all write exits
        for desc in obj.commands:
            var_name =  desc.get_var_ref()
            if var_name :
                if not vars.has_key(var_name):
                    raise CTRexPacketBuildException(-11,("variable %s does not exists  ") % (var_name) );  
            desc.compile(self);

        for desc in obj.commands:
            self.vm_low_level.add_ins(desc.get_obj());


    def _pkt_layer_offset (self,layer_name):
        assert self.pkt != None, 'empty packet'
        p_utl=CTRexScapyPktUtl(self.pkt);
        return p_utl.get_layer_offet_by_str(layer_name)

    def _name_to_offset(self,field_name):
        assert self.pkt != None, 'empty packet'
        p_utl=CTRexScapyPktUtl(self.pkt);
        return p_utl.get_field_offet_by_str(field_name)

    def _add_tuple_gen(self,tuple_gen):

        pass;




