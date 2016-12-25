import random
import string
import struct
import socket
import json
import yaml
import binascii
import base64
import inspect
import copy

from .trex_stl_packet_builder_interface import CTrexPktBuilderInterface
from .trex_stl_types import *
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

def safe_ord (c):
    if type(c) is str:
        return ord(c)
    elif type(c) is int:
        return c
    else:
        raise TypeError("Cannot convert: {0} of type: {1}".format(c, type(c)))

def _buffer_to_num(str_buffer):
    validate_type('str_buffer', str_buffer, bytes)
    res=0
    for i in str_buffer:
        res = res << 8
        res += safe_ord(i)
    return res


def ipv4_str_to_num (ipv4_buffer):
    validate_type('ipv4_buffer', ipv4_buffer, bytes)
    assert len(ipv4_buffer)==4, 'Size of ipv4_buffer is not 4'
    return _buffer_to_num(ipv4_buffer)

def mac_str_to_num (mac_buffer):
    validate_type('mac_buffer', mac_buffer, bytes)
    assert len(mac_buffer)==6, 'Size of mac_buffer is not 6'
    return _buffer_to_num(mac_buffer)


def is_valid_ipv4_ret(ip_addr):
    """
    Return buffer in network order
    """
    if  type(ip_addr) == bytes and len(ip_addr) == 4:
        return ip_addr

    if  type(ip_addr)== int:
        ip_addr = socket.inet_ntoa(struct.pack("!I", ip_addr))

    try:
        return socket.inet_pton(socket.AF_INET, ip_addr)
    except AttributeError:  # no inet_pton here, sorry
        return socket.inet_aton(ip_addr)
    except socket.error:  # not a valid address
        raise CTRexPacketBuildException(-10,"Not valid ipv4 format");


def is_valid_ipv6_ret(ipv6_addr):
    """
    Return buffer in network order
    """
    if type(ipv6_addr) == bytes and len(ipv6_addr) == 16:
        return ipv6_addr
    try:
        return socket.inet_pton(socket.AF_INET6, ipv6_addr)
    except AttributeError:  # no inet_pton here, sorry
        raise CTRexPacketBuildException(-10, 'No inet_pton function available')
    except:
        raise CTRexPacketBuildException(-10, 'Not valid ipv6 format')

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
            raise CTRexPacketBuildException(-12, 'Field type should be in %s' % FILED_TYPES);


class CTRexScFieldRangeValue(CTRexScFieldRangeBase):
    """
    Range of field values
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
            raise CTRexPacketBuildException(-12, 'Invalid range: min is greater than max.');
        if min_value == max_value:
            raise CTRexPacketBuildException(-13, "Invalid range: min value is equal to max value.");


class CTRexScIpv4SimpleRange(CTRexScFieldRangeBase):
    """
    Range of ipv4 ip
    """
    def __init__(self, field_name, field_type, min_ip, max_ip):
        super(CTRexScIpv4SimpleRange, self).__init__(field_name,field_type)
        self.min_ip = min_ip
        self.max_ip = max_ip
        mmin=ipv4_str_to_num (is_valid_ipv4_ret(min_ip))
        mmax=ipv4_str_to_num (is_valid_ipv4_ret(max_ip))
        if  mmin > mmax :
            raise CTRexPacketBuildException(-11, 'CTRexScIpv4SimpleRange m_min ip is bigger than max');


class CTRexScIpv4TupleGen(CTRexScriptsBase):
    """
    Range tuple
    """
    FLAGS_ULIMIT_FLOWS =1

    def __init__(self, min_ipv4, max_ipv4, num_flows=100000, min_port=1025, max_port=65535, flags=0):
        super(CTRexScIpv4TupleGen, self).__init__()
        self.min_ip = min_ipv4
        self.max_ip = max_ipv4
        mmin=ipv4_str_to_num (is_valid_ipv4_ret(min_ipv4))
        mmax=ipv4_str_to_num (is_valid_ipv4_ret(max_ipv4))
        if  mmin > mmax :
            raise CTRexPacketBuildException(-11, 'CTRexScIpv4SimpleRange m_min ip is bigger than max');

        self.num_flows=num_flows;

        self.min_port =min_port
        self.max_port =max_port
        self.flags =   flags


class CTRexScTrimPacketSize(CTRexScriptsBase):
    """
    Trim packet size. Field type is CTRexScFieldRangeBase.FILED_TYPES = ["inc","dec","rand"]
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


class STLScVmRaw(CTRexScriptsBase):
    """
    Raw instructions
    """
    def __init__(self,list_of_commands=None,split_by_field=None,cache_size=None):
        """
        Include a list of a basic instructions objects.

        :parameters:
             list_of_commands : list
                list of instructions 

             split_by_field : string 
                by which field to split to threads

             cache_size     : uint16_t 
                In case it is bigger than zero, FE results will be cached - this will speedup of the program at the cost of limiting the number of possible packets to the number of cache. The cache size is limited to the pool size

        The following example splits the generated traffic by "ip_src" variable.

        .. code-block:: python

            # Split by 

            # TCP SYN
            base_pkt  = Ether()/IP(dst="48.0.0.1")/TCP(dport=80,flags="S")     
    
    
            # vm
            vm = STLScVmRaw( [ STLVmFlowVar(name="ip_src", 
                                                  min_value="16.0.0.0", 
                                                  max_value="16.0.0.254", 
                                                  size=4, op="inc"),                     
    
    
                               STLVmWrFlowVar(fv_name="ip_src", pkt_offset= "IP.src" ),  
    
                               STLVmFixIpv4(offset = "IP"), # fix checksum              
                              ]
                             ,split_by_field = "ip_src",
                             cache_size = 1000
                           )

        """

        super(STLScVmRaw, self).__init__()
        self.split_by_field = split_by_field
        self.cache_size = cache_size

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
    Instruction base
    """
    def __init__(self, ins_type):
        self.type = ins_type
        validate_type('ins_type', ins_type, str)

class CTRexVmInsFixIpv4(CTRexVmInsBase):
    def __init__(self, offset):
        super(CTRexVmInsFixIpv4, self).__init__("fix_checksum_ipv4")
        self.pkt_offset = offset
        validate_type('offset', offset, int)

class CTRexVmInsFixHwCs(CTRexVmInsBase):
    L4_TYPE_UDP = 11
    L4_TYPE_TCP = 13

    def __init__(self, l2_len,l3_len,l4_type):
        super(CTRexVmInsFixHwCs, self).__init__("fix_checksum_hw")
        self.l2_len = l2_len
        validate_type('l2_len', l2_len, int)
        self.l3_len = l3_len
        validate_type('l3_len', l3_len, int)
        self.l4_type = l4_type
        validate_type('l4_type', l4_type, int)



class CTRexVmInsFlowVar(CTRexVmInsBase):
    #TBD add more validation tests

    OPERATIONS =['inc', 'dec', 'random']
    VALID_SIZES =[1, 2, 4, 8]

    def __init__(self, fv_name, size, op, init_value, min_value, max_value,step):
        super(CTRexVmInsFlowVar, self).__init__("flow_var")
        self.name = fv_name;
        validate_type('fv_name', fv_name, str)
        self.size = size
        self.op = op
        self.init_value = init_value
        validate_type('init_value', init_value, int)
        assert init_value >= 0, 'init_value (%s) is negative' % init_value
        self.min_value=min_value
        validate_type('min_value', min_value, int)
        assert min_value >= 0, 'min_value (%s) is negative' % min_value
        self.max_value=max_value
        validate_type('max_value', max_value, int)
        assert max_value >= 0, 'max_value (%s) is negative' % max_value
        self.step=step
        validate_type('step', step, int)
        assert step >= 0, 'step (%s) is negative' % step

class CTRexVmInsFlowVarRandLimit(CTRexVmInsBase):
    #TBD add more validation tests

    VALID_SIZES =[1, 2, 4, 8]

    def __init__(self, fv_name, size, limit, seed, min_value, max_value):
        super(CTRexVmInsFlowVarRandLimit, self).__init__("flow_var_rand_limit")
        self.name = fv_name;
        validate_type('fv_name', fv_name, str)
        self.size = size
        self.limit=limit
        validate_type('limit', limit, int)
        assert limit >= 0, 'limit (%s) is negative' % limit
        self.seed=seed
        validate_type('seed', seed, int)
        self.min_value=min_value
        validate_type('min_value', min_value, int)
        assert min_value >= 0, 'min_value (%s) is negative' % min_value
        self.max_value=max_value
        validate_type('max_value', max_value, int)
        assert max_value >= 0, 'max_value (%s) is negative' % max_value


class CTRexVmInsWrFlowVar(CTRexVmInsBase):
    def __init__(self, fv_name, pkt_offset, add_value=0, is_big_endian=True):
        super(CTRexVmInsWrFlowVar, self).__init__("write_flow_var")
        self.name = fv_name
        validate_type('fv_name', fv_name, str)
        self.pkt_offset = pkt_offset
        validate_type('pkt_offset', pkt_offset, int)
        self.add_value = add_value
        validate_type('add_value', add_value, int)
        self.is_big_endian = is_big_endian
        validate_type('is_big_endian', is_big_endian, bool)

class CTRexVmInsWrMaskFlowVar(CTRexVmInsBase):
    def __init__(self, fv_name, pkt_offset,pkt_cast_size,mask,shift,add_value, is_big_endian=True):
        super(CTRexVmInsWrMaskFlowVar, self).__init__("write_mask_flow_var")
        self.name = fv_name
        validate_type('fv_name', fv_name, str)
        self.pkt_offset = pkt_offset
        validate_type('pkt_offset', pkt_offset, int)
        self.pkt_cast_size = pkt_cast_size
        validate_type('pkt_cast_size', pkt_cast_size, int)
        self.mask = mask
        validate_type('mask', mask, int)
        self.shift = shift
        validate_type('shift', shift, int)
        self.add_value =add_value
        validate_type('add_value', add_value, int)
        self.is_big_endian = is_big_endian
        validate_type('is_big_endian', is_big_endian, bool)

class CTRexVmInsTrimPktSize(CTRexVmInsBase):
    def __init__(self,fv_name):
        super(CTRexVmInsTrimPktSize, self).__init__("trim_pkt_size")
        self.name = fv_name
        validate_type('fv_name', fv_name, str)

class CTRexVmInsTupleGen(CTRexVmInsBase):
    def __init__(self, fv_name, ip_min, ip_max, port_min, port_max, limit_flows, flags=0):
        super(CTRexVmInsTupleGen, self).__init__("tuple_flow_var")
        self.name =fv_name
        validate_type('fv_name', fv_name, str)
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
            Inlcude list of instructions.
            """
            super(CTRexVmEngine, self).__init__()
            self.ins=[]
            self.split_by_var = ''
            self.cache_size = 0


       # return as json
       def get_json (self):
           inst_array = [];
           # dump it as dict
           for obj in self.ins:
               inst_array.append(obj.__dict__);

           d={'instructions': inst_array, 'split_by_var': self.split_by_var};
           if self.cache_size >0 :
               d['cache']=self.cache_size
           return d

       def add_ins (self,ins):
           #assert issubclass(ins, CTRexVmInsBase)
           self.ins.append(ins);

       def dump (self):
           cnt=0;
           for obj in self.ins:
               print("ins",cnt)
               cnt = cnt +1
               print(obj.__dict__)

       def dump_bjson (self):
          print(json.dumps(self.get_json(), sort_keys=True, indent=4))

       def dump_as_yaml (self):
          print(yaml.dump(self.get_json(), default_flow_style=False))



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
        Return string 'IP:UDP:TCP'
        """
        l=self.get_list_iter ();
        l1=map(lambda p: p.name,l );
        return ":".join(l1);

    def _layer_offset(self, name, cnt = 0):
        """
        Return offset of layer. Example: 'IP',1 returns offfset of layer ip:1
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
        Return offset of layer. Example: 'IP',1 returns offfset of layer ip:1
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
        Return offset of layer. Example: 'IP',1 returns offfset of layer ip:1
        """
        t=self._layer_offset(layer,layer_cnt);
        l_offset=t[1];
        layer_pkt=t[0]

        #layer_pkt.dump_fields_offsets ()

        for f in layer_pkt.fields_desc:
            if f.name == field_name:
                return (l_offset+f.offset,f.get_size_bytes ());

        raise CTRexPacketBuildException(-11, "No layer %s-%d." % (field_name, layer_cnt))

    def get_layer_offet_by_str(self, layer_des):
        """
        Return layer offset by string.

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
        Return field_des (offset,size) layer:cnt.field
        Example:  
        802|1Q.vlan get 802.1Q->valn replace | with .
        IP.src
        IP:0.src  (first IP.src like IP.src)
        Example: IP:1.src  for internal IP

        Return (offset, size) as tuple.


        """

        s=field_des.split(".");
        if len(s)!=2:
            raise CTRexPacketBuildException(-11, ("Field desription should be layer:cnt.field Example: IP.src or IP:1.src"));


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
    Instruction base
    """
    def __init__(self):
       pass;

    def get_obj(self):
        return self;

    def get_json(self):
        return self.get_obj().__dict__

    def dump_bjson(self):
       print(json.dumps(self.get_json(), sort_keys=True, indent=4))

    def dump_as_yaml(self):
       print(yaml.dump(self.get_json(), default_flow_style=False))


    def get_var_ref (self):
        '''
          Virtual function returns a ref var name.
        '''
        return None

    def get_var_name(self):
        '''
          Virtual function returns the varible name if it exists.
        '''
        return None

    def compile(self,parent):
        '''
          Virtual function to take parent that has function name_to_offset.
        '''
        pass;


def valid_fv_size (size):
    if not (size in CTRexVmInsFlowVar.VALID_SIZES):
        raise CTRexPacketBuildException(-11,("Flow var has invalid size %d ") % size  );

def valid_fv_ops (op):
    if not (op in CTRexVmInsFlowVar.OPERATIONS):
        raise CTRexPacketBuildException(-11,("Flow var has invalid op %s ") % op  );

def get_max_by_size (size):
    d={
    1:((1<<8) -1),
    2:((1<<16)-1),
    4:((1<<32)-1),
    8:0xffffffffffffffff
    };
    return d[size]

def convert_val (val):
    if is_integer(val):
        return val
    if type(val) == str:
        return ipv4_str_to_num (is_valid_ipv4_ret(val))
    raise CTRexPacketBuildException(-11,("init val invalid %s ") % val  );

def check_for_int (val):
    validate_type('val', val, int)


class STLVmFlowVar(CTRexVmDescBase):

    def __init__(self, name, init_value=None, min_value=0, max_value=255, size=4, step=1,op="inc"):
        """
        Flow variable instruction. Allocates a variable on a stream context. The size argument determines the variable size.
        The operation can be inc, dec, and random. 
        For increment and decrement operations, can set the "step" size.
        For all operations, can set initialization value, minimum and maximum value.

        :parameters:
             name : string 
                Name of the stream variable 

             init_value : int
                Init value of the variable. If not specified, it will be min_value

             min_value  : int
                Min value 

             max_value  : int
                Max value 

             size  : int
                Number of bytes of the variable. Possible values: 1,2,4,8 for uint8_t, uint16_t, uint32_t, uint64_t

             step  : int 
                Step in case of "inc" or "dec" operations

             op    : string 
                Possible values: "inc", "dec", "random"

        .. code-block:: python

            # Example1

            # input 
            STLVmFlowVar(min_value=0, max_value=3, size=1,op="inc")

            # output 0,1,2,3,0,1,2,3 ..

            # input 
            STLVmFlowVar(min_value=0, max_value=3, size=1,op="dec")

            # output 0,3,2,1,0,3,2,1 ..


            # input 
            STLVmFlowVar(min_value=0, max_value=3, size=1,op="random")

            # output 1,1,2,3,1,2,1,0 ..

            # input 
            STLVmFlowVar(min_value=0, max_value=10, size=1,op="inc",step=3)

            # output 0,3,6,9,0,3,6,9,0..


        """
        super(STLVmFlowVar, self).__init__()
        self.name = name;
        validate_type('name', name, str)
        self.size =size
        valid_fv_size(size)
        self.op =op
        valid_fv_ops (op)

        # choose default value for init val
        if init_value == None:
            init_value = max_value if op == "dec" else min_value

        self.init_value = convert_val (init_value)
        self.min_value  = convert_val (min_value);
        self.max_value  = convert_val (max_value)
        self.step       = convert_val (step)

        if self.min_value > self.max_value :
            raise CTRexPacketBuildException(-11,("max %d is lower than min %d ") % (self.max_value,self.min_value)  );

    def get_obj (self):
        return CTRexVmInsFlowVar(self.name,self.size,self.op,self.init_value,self.min_value,self.max_value,self.step);

    def get_var_name(self):
        return [self.name]

class STLVmFlowVarRepetableRandom(CTRexVmDescBase):

    def __init__(self, name,  size=4, limit=100, seed=None, min_value=0, max_value=None):
        """
        Flow variable instruction for repeatable random with limit number of generating numbers. Allocates memory on a stream context. 
        The size argument determines the variable size. Could be 1,2,4 or 8

        :parameters:
             name : string 
                Name of the stream variable 

             size  : int
                Number of bytes of the variable. Possible values: 1,2,4,8 for uint8_t, uint16_t, uint32_t, uint64_t

             limit  : int 
                The number of distinct repetable random number 

             seed   : int 
                For deterministic result, you can set this to a uint16_t number

             min_value  : int
                Min value 

             max_value  : int
                Max value 


        .. code-block:: python

            # Example1

            # input , 1 byte or random with limit of 5 
            STLVmFlowVarRepetableRandom("var1",size=1,limit=5)

            # output 255,1,7,129,8, ==> repeat 255,1,7,129,8

            STLVmFlowVarRepetableRandom("var1",size=4,limit=100,min_value=0x12345678, max_value=0x32345678)


        """
        super(STLVmFlowVarRepetableRandom, self).__init__()
        self.name = name;
        validate_type('name', name, str)
        self.size =size
        valid_fv_size(size)
        self.limit =limit

        if seed == None:
            self.seed = random.randint(1, 32000)
        else:
            self.seed = seed

        self.min_value  = convert_val (min_value);

        if max_value == None :
            self.max_value = get_max_by_size (self.size) 
        else:
            self.max_value = convert_val (max_value)

        if self.min_value > self.max_value :
            raise CTRexPacketBuildException(-11,("max %d is lower than min %d ") % (self.max_value,self.min_value)  );

    def get_obj (self):
        return  CTRexVmInsFlowVarRandLimit(self.name, self.size, self.limit, self.seed, self.min_value, self.max_value);

    def get_var_name(self):
        return [self.name]

class STLVmFixChecksumHw(CTRexVmDescBase):
    def __init__(self, l3_offset,l4_offset,l4_type):
        """
        Fix Ipv4 header checksum and TCP/UDP checksum using hardware assist. 
        Use this if the packet header has changed or data payload has changed as it is necessary to fix the checksums.
        This instruction works on NICS that support this hardware offload.

        For fixing only IPv4 header checksum use STLVmFixIpv4. This instruction should be used if both L4 and L3 need to be fixed. 
        
        example for supported packets

        Ether()/(IPv4|IPv6)/(UDP|TCP)
        Ether()/(IPv4|IPv6)/(UDP|TCP)
        SomeTunnel()/(IPv4|IPv6)/(UDP|TCP)
        SomeTunnel()/(IPv4|IPv6)/(UDP|TCP)


        :parameters:
             l3_offset : offset in bytes 
                **IPv4/IPv6 header** offset from packet start. It is **not** the offset of the checksum field itself.
                in could be string in case of scapy packet. format IP[:[id]]

             l4_offset : offset in bytes to UDP/TCP header

             l4_type   : CTRexVmInsFixHwCs.L4_TYPE_UDP or CTRexVmInsFixHwCs.L4_TYPE_TCP 

             see full example stl/syn_attack_fix_cs_hw.py

        .. code-block:: python

            # Example2

            pkt = Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)

            # by offset 
            STLVmFixChecksumHw(l3_offset=14,l4_offset=14+20,l4_type=CTRexVmInsFixHwCs.L4_TYPE_UDP)

            # in case of scapy packet can be defined by header name 
            STLVmFixChecksumHw(l3_offset="IP",l4_offset="UDP",l4_type=CTRexVmInsFixHwCs.L4_TYPE_UDP)

            # string for second "IP" header in the packet is IP:1
            STLVmFixChecksumHw(offset="IP:1")

        """

        super(STLVmFixChecksumHw, self).__init__()
        self.l3_offset = l3_offset; # could be a name of offset
        self.l4_offset = l4_offset; # could be a name of offset
        self.l4_type = l4_type
        self.l2_len = 0


    def get_obj (self):
        return CTRexVmInsFixHwCs(self.l2_len,self.l3_len,self.l4_type);

    def compile(self,parent):
        if type(self.l3_offset)==str:
            self.l2_len = parent._pkt_layer_offset(self.l3_offset);
        if type(self.l4_offset)==str:
            self.l4_offset = parent._pkt_layer_offset(self.l4_offset);

        assert self.l4_offset >= self.l2_len+8, 'l4_offset should be higher than l3_offset offset'
        self.l3_len = self.l4_offset - self.l2_len;


class STLVmFixIpv4(CTRexVmDescBase):
    def __init__(self, offset):
        """
        Fix IPv4 header checksum. Use this if the packet header has changed and it is necessary to change the checksum.

        :parameters:
             offset : uint16_t or string 
                **IPv4 header** offset from packet start. It is **not** the offset of the checksum field itself.
                in could be string in case of scapy packet. format IP[:[id]]

        .. code-block:: python

            # Example2

            pkt = Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)

            # by offset 
            STLVmFixIpv4(offset=14)

            # in case of scapy packet can be defined by header name 
            STLVmFixIpv4(offset="IP")

            # string for second "IP" header in the packet is IP:1
            STLVmFixIpv4(offset="IP:1")

        """

        super(STLVmFixIpv4, self).__init__()
        self.offset = offset; # could be a name of offset

    def get_obj (self):
        return CTRexVmInsFixIpv4(self.offset);

    def compile(self,parent):
        if type(self.offset)==str:
            self.offset = parent._pkt_layer_offset(self.offset);

class STLVmWrFlowVar(CTRexVmDescBase):
    def __init__(self, fv_name, pkt_offset, offset_fixup=0, add_val=0, is_big=True):
        """
        Write a stream variable into a packet field. 
        The write position is determined by the packet offset + offset fixup. The size of the write is determined by the stream variable. 
        Example: Offset 10, fixup 0, variable size 4. This function writes at 10, 11, 12, and 13.
        
        For inromation about chaning the write size, offset, or fixup, see the `STLVmWrMaskFlowVar` command. 
        The Field name/offset can be given by name in the following format: ``header[:id].field``. 

        
        :parameters:
            fv_name : string 
                Stream variable to write to a packet offset.

            pkt_offset : string or in
                Name of the field or offset in bytes from packet start.

            offset_fixup : int 
                Number of bytes to move forward. If negative, move backward.

             add_val     : int
                Value to add to the stream variable before writing it to the packet field. Can be used as a constant offset. 

             is_big      : bool 
                How to write the variable to the the packet. True=big-endian, False=little-endian 

        .. code-block:: python

            # Example3

            pkt = Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)


            # write to ip.src offset 
            STLVmWrFlowVar (fv_name="tuple", pkt_offset= "IP.src" )

            # packet offset is varible 
            STLVmWrFlowVar (fv_name="tuple", pkt_offset= 26 )

            # add l3_len_fix before writing fv_rand into IP.len field 
            STLVmWrFlowVar(fv_name="fv_rand", pkt_offset= "IP.len", add_val=l3_len_fix) 

        """

        super(STLVmWrFlowVar, self).__init__()
        self.name =fv_name
        validate_type('fv_name', fv_name, str)
        self.offset_fixup =offset_fixup
        validate_type('offset_fixup', offset_fixup, int)
        self.pkt_offset =pkt_offset
        self.add_val =add_val
        validate_type('add_val', add_val, int)
        self.is_big =is_big;
        validate_type('is_big', is_big, bool)

    def get_var_ref (self):
        return self.name

    def get_obj (self):
            return  CTRexVmInsWrFlowVar(self.name,self.pkt_offset+self.offset_fixup,self.add_val,self.is_big)

    def compile(self,parent):
        if type(self.pkt_offset)==str:
            t=parent._name_to_offset(self.pkt_offset)
            self.pkt_offset = t[0]

class STLVmWrMaskFlowVar(CTRexVmDescBase):
    def __init__(self, fv_name, pkt_offset, pkt_cast_size=1, mask=0xff, shift=0, add_value=0, offset_fixup=0, is_big=True):

        """
        Write a stream variable into a packet field with some operations. 
        Using this instruction, the variable size and the field can have different sizes. 

        Pseudocode of this code::

                uint32_t val=(cast_to_size)rd_from_variable("name") # read flow-var
                val+=m_add_value                                    # add value
        
                if (m_shift>0) {                                    # shift 
                    val=val<<m_shift
                }else{
                    if (m_shift<0) {
                        val=val>>(-m_shift)
                    }
                }
        
                pkt_val=rd_from_pkt(pkt_offset)                     # RMW to the packet
                pkt_val = (pkt_val & ~m_mask) | (val & m_mask)
                wr_to_pkt(pkt_offset,pkt_val)


        :parameters:
            fv_name : string 
                The stream variable name to write to a packet field

            pkt_cast_size : uint8_t 
                The size in bytes of the packet field


            mask          : uint32_t 
                The mask of the field. 1 means to write. 0 don't care

            shift          : uint8_t 
                How many bits to shift 

            pkt_offset : string or in
                the name of the field or offset in byte from packet start.

            offset_fixup : int 
                how many bytes to go forward. In case of a negative value go backward 

             add_val     : int
                value to add to stream variable before writing it to packet field. can be used as a constant offset 

             is_big      : bool 
                how to write the variable to the the packet. is it big-edian or little edian 

        Example 1 - Cast from uint16_t (var) to uint8_t (pkt)::


            base_pkt =  Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)

            vm = STLScVmRaw( [ STLVmFlowVar(name="mac_src", 
                                            min_value=1, 
                                            max_value=30, 
                                            size=2, 
                                            op="dec",step=1), 
                               STLVmWrMaskFlowVar(fv_name="mac_src", 
                                                  pkt_offset= 11,
                                                  pkt_cast_size=1, 
                                                  mask=0xff) # mask command ->write it as one byte
                          ]
                       )

            pkt = Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)

        Example 2 - Change MSB of uint16_t variable::


            vm = STLScVmRaw( [ STLVmFlowVar(name="mac_src", 
                                            min_value=1, 
                                            max_value=30, 
                                            size=2, op="dec",step=1), 
                               STLVmWrMaskFlowVar(fv_name="mac_src", 
                                                  pkt_offset= 10,
                                                  pkt_cast_size=2, 
                                                  mask=0xff00,
                                                  shift=8) # take the var shift it 8 (x256) write only to LSB
                              ]
                            )



        Example 3 - Every 2 packets, change the MAC (shift right)::

                vm = STLScVmRaw( [ STLVmFlowVar(name="mac_src", 
                                                min_value=1, 
                                                max_value=30, 
                                                size=2, op="dec",step=1), 
                                   STLVmWrMaskFlowVar(fv_name="mac_src", 
                                                      pkt_offset= 10,
                                                      pkt_cast_size=1, 
                                                      mask=0x1,
                                                      shift=-1) # take var mac_src>>1 and write the LSB every two packet there should be a change
                                 ]
                                )


        """

        super(STLVmWrMaskFlowVar, self).__init__()
        self.name =fv_name
        validate_type('fv_name', fv_name, str)
        self.offset_fixup =offset_fixup
        validate_type('offset_fixup', offset_fixup, int)
        self.pkt_offset =pkt_offset
        self.pkt_cast_size =pkt_cast_size
        validate_type('pkt_cast_size', pkt_cast_size, int)
        if not (pkt_cast_size in [1,2,4]):
            raise CTRexPacketBuildException(-10,"not valid cast size");

        self.mask = mask
        validate_type('mask', mask, int)
        self.shift = shift
        validate_type('shift', shift, int)
        self.add_value = add_value
        validate_type('add_value', add_value, int)

        self.is_big =is_big;
        validate_type('is_big', is_big, bool)

    def get_var_ref (self):
        return self.name

    def get_obj (self):
            return  CTRexVmInsWrMaskFlowVar(self.name,self.pkt_offset+self.offset_fixup,self.pkt_cast_size,self.mask,self.shift,self.add_value,self.is_big)

    def compile(self,parent):
        if type(self.pkt_offset)==str:
            t=parent._name_to_offset(self.pkt_offset)
            self.pkt_offset = t[0]


class STLVmTrimPktSize(CTRexVmDescBase):
    def __init__(self,fv_name):
        """
            Trim the packet size by the stream variable size. This instruction only changes the total packet size, and does not repair the fields to match the new size.


            :parameters:
                fv_name : string
                    Stream variable name. The value of this variable is the new total packet size.


            For Example::

                def create_stream (self):
                    # pkt
                    p_l2  = Ether();
                    p_l3  = IP(src="16.0.0.1",dst="48.0.0.1")
                    p_l4  = UDP(dport=12,sport=1025)
                    pyld_size = max(0, self.max_pkt_size_l3 - len(p_l3/p_l4));
                    base_pkt = p_l2/p_l3/p_l4/('\x55'*(pyld_size))

                    l3_len_fix =-(len(p_l2));
                    l4_len_fix =-(len(p_l2/p_l3));


                    # vm
                    vm = STLScVmRaw( [ STLVmFlowVar(name="fv_rand", min_value=64,
                                                    max_value=len(base_pkt),
                                                    size=2, op="inc"),

                                       STLVmTrimPktSize("fv_rand"),                         # change total packet size <<<

                                       STLVmWrFlowVar(fv_name="fv_rand",
                                                      pkt_offset= "IP.len",
                                                      add_val=l3_len_fix), # fix ip len

                                       STLVmFixIpv4(offset = "IP"),                       # fix checksum

                                       STLVmWrFlowVar(fv_name="fv_rand",
                                                      pkt_offset= "UDP.len",
                                                      add_val=l4_len_fix) # fix udp len
                                      ]
                                   )

                    pkt = STLPktBuilder(pkt = base_pkt,
                                        vm = vm)

                    return STLStream(packet = pkt,
                                     mode = STLTXCont())


        """

        super(STLVmTrimPktSize, self).__init__()
        self.name = fv_name
        validate_type('fv_name', fv_name, str)

    def get_var_ref (self):
        return self.name

    def get_obj (self):
        return  CTRexVmInsTrimPktSize(self.name)



class STLVmTupleGen(CTRexVmDescBase):
    def __init__(self,name, ip_min="0.0.0.1", ip_max="0.0.0.10", port_min=1025, port_max=65535, limit_flows=100000, flags=0):
        """
        Generate a struct with two variables: ``var_name.ip`` as uint32_t and ``var_name.port`` as uint16_t 
        The variables are dependent. When the ip variable value reaches its maximum, the port is incremented. 

        For:

        * ip_min      = 10.0.0.1
        * ip_max      = 10.0.0.5
        * port_min    = 1025
        * port_max    = 1028
        * limit_flows = 10

        The result:

        +------------+------------+-----------+ 
        | ip         | port       | flow_id   | 
        +============+============+===========+ 
        | 10.0.0.1   | 1025       | 1         | 
        +------------+------------+-----------+ 
        | 10.0.0.2   | 1025       | 2         | 
        +------------+------------+-----------+ 
        | 10.0.0.3   | 1025       | 3         | 
        +------------+------------+-----------+ 
        | 10.0.0.4   | 1025       | 4         | 
        +------------+------------+-----------+ 
        | 10.0.0.5   | 1025       | 5         | 
        +------------+------------+-----------+ 
        | 10.0.0.1   | 1026       | 6         | 
        +------------+------------+-----------+ 
        | 10.0.0.2   | 1026       | 7         | 
        +------------+------------+-----------+ 
        | 10.0.0.3   | 1026       | 8         | 
        +------------+------------+-----------+ 
        | 10.0.0.4   | 1026       | 9         | 
        +------------+------------+-----------+ 
        | 10.0.0.5   | 1026       | 10        | 
        +------------+------------+-----------+ 
        | 10.0.0.1   | 1025       | 1         | 
        +------------+------------+-----------+


        :parameters:
            name : string 
                Name of the stream struct. 

            ip_min : string or int 
                Min value of the ip value. Number or IPv4 format.

            ip_max : string or int 
                Max value of the ip value. Number or IPv4 format.

            port_min : int 
                Min value of port variable. 

            port_max : int 
                Max value of port variable. 

            limit_flows : int 
                Limit of number of flows. 

            flags       : 0

            ="0.0.0.10", port_min=1025, port_max=65535, limit_flows=100000, flags=0

        .. code-block:: python

            # Example5

            def create_stream (self):
                # pkt 
                p_l2  = Ether();
                p_l3  = IP(src="16.0.0.1",dst="48.0.0.1")
                p_l4  = UDP(dport=12,sport=1025)
                pyld_size = max(0, self.max_pkt_size_l3 - len(p_l3/p_l4));
                base_pkt = p_l2/p_l3/p_l4/('\x55'*(pyld_size))

                l3_len_fix =-(len(p_l2));
                l4_len_fix =-(len(p_l2/p_l3));


                # vm
                vm = STLScVmRaw( [ STLVmFlowVar(name="fv_rand", min_value=64, 
                                                max_value=len(base_pkt), 
                                                size=2, op="inc"),

                                   STLVmTrimPktSize("fv_rand"),                         # change total packet size <<<

                                   STLVmWrFlowVar(fv_name="fv_rand", 
                                                  pkt_offset= "IP.len", 
                                                  add_val=l3_len_fix), # fix ip len 

                                   STLVmFixIpv4(offset = "IP"),                       # fix checksum

                                   STLVmWrFlowVar(fv_name="fv_rand", 
                                                  pkt_offset= "UDP.len", 
                                                  add_val=l4_len_fix) # fix udp len  
                                  ]
                               )

                pkt = STLPktBuilder(pkt = base_pkt,
                                    vm = vm)

                return STLStream(packet = pkt,
                                 mode = STLTXCont())


        """

        super(STLVmTupleGen, self).__init__()
        self.name = name
        validate_type('name', name, str)
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

class STLPktBuilder(CTrexPktBuilderInterface):

    def __init__(self, pkt = None, pkt_buffer = None, vm = None, path_relative_to_profile = False, build_raw = False, remove_fcs = True):
        """

        This class defines a method for building a template packet and Field Engine using the Scapy package.
        Using this class the user can also define how TRex will handle the packet by specifying the Field engine settings.
        The pkt can be a Scapy pkt or pcap file name.
        If using a pcap file, and path_relative_to_profile is True, then the function loads the pcap file from a path relative to the profile.

        
        .. code-block:: python

            # Example6

            # packet is scapy
            STLPktBuilder( pkt = Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)/(10*'x') )

    
            # packet is taken from pcap file relative to python 
            STLPktBuilder( pkt ="stl/yaml/udp_64B_no_crc.pcap")
    
            # packet is taken from pcap file relative to profile file 
            STLPktBuilder( pkt ="stl/yaml/udp_64B_no_crc.pcap",
                                path_relative_to_profile = True )
    
    
            vm = STLScVmRaw( [   STLVmTupleGen ( ip_min="16.0.0.1", ip_max="16.0.0.2", 
                                                   port_min=1025, port_max=65535,
                                                    name="tuple"), # define tuple gen 
    
                             STLVmWrFlowVar (fv_name="tuple.ip", pkt_offset= "IP.src" ), # write ip to packet IP.src
                             STLVmFixIpv4(offset = "IP"),                                # fix checksum
                             STLVmWrFlowVar (fv_name="tuple.port", pkt_offset= "UDP.sport" )  #write udp.port
                             ]
                           )
    
            base_pkt = Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)
            pad = max(0, size - len(base_pkt)) * 'x'
    
            STLPktBuilder(pkt = base_pkt/pad, vm= vm)


        :parameters:

             pkt : string, 
                Scapy object or pcap filename.

             pkt_buffer : bytes 
                Packet as buffer. 

             vm   : list or base on :class:`trex_stl_lib.trex_stl_packet_builder_scapy.STLScVmRaw`
                List of instructions to manipulate packet fields. 

             path_relative_to_profile : bool 
                If pkt is a pcap file, determines whether to load it relative to profile file.

             build_raw : bool 
                If a buffer is specified (by pkt_buffer), determines whether to build Scapy. Useful in cases where it is necessary to take the offset from Scapy. 

             remove_fcs : bool 
                If a buffer is specified (by pkt_buffer), determines whether to remove FCS. 



        """
        super(STLPktBuilder, self).__init__()

        validate_type('pkt', pkt, (type(None), str, Packet))
        validate_type('pkt_buffer', pkt_buffer, (type(None), bytes))

        self.pkt = None     # as input
        self.pkt_raw = None # from raw pcap file
        self.vm_scripts = [] # list of high level instructions
        self.vm_low_level = None
        self.is_pkt_built = False
        self.metadata=""
        self.path_relative_to_profile = path_relative_to_profile
        self.remove_fcs = remove_fcs
        self.is_binary_source = pkt_buffer != None


        if pkt != None and pkt_buffer != None:
            raise CTRexPacketBuildException(-15, "Packet builder cannot be provided with both pkt and pkt_buffer.")

        # process packet
        if pkt != None:
            self.set_packet(pkt)

        elif pkt_buffer != None:
            self.set_pkt_as_str(pkt_buffer)

        # process VM
        if vm != None:
            if not isinstance(vm, (STLScVmRaw, list)):
                raise CTRexPacketBuildException(-14, "Bad value for variable vm.")

            self.add_command(vm if isinstance(vm, STLScVmRaw) else STLScVmRaw(vm))

        # raw source build to see MAC presence/ fields offset by name in VM
        if build_raw and self.pkt_raw and not self.pkt:
            self.__lazy_build_packet()

        # if we have packet and VM - compile now
        if (self.pkt or self.pkt_raw) and (self.vm_scripts):
            self.compile()


    def dump_vm_data_as_yaml(self):
        print(yaml.dump(self.get_vm_data(), default_flow_style=False))

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
        Dumps the packet as a decimal array of bytes (each item x gets value in range 0-255)

        :parameters:
            encode : bool
                Encode using base64. (disable for debug)

                Default: **True**

        :return:
            + packet representation as array of bytes

        :raises:
            + :exc:`AssertionError`, in case packet is empty.

        """
        pkt_buf = self._get_pkt_as_str()
        return {'binary': base64.b64encode(pkt_buf).decode() if encode else pkt_buf,
                'meta': self.metadata}


    def dump_pkt_to_pcap(self, file_path):
        wrpcap(file_path, self._get_pkt_as_str())

    def add_command (self, script):
        self.vm_scripts.append(script.clone());

    def dump_scripts (self):
        self.vm_low_level.dump_as_yaml()

    def dump_as_hex (self):
        pkt_buf = self._get_pkt_as_str()
        print(hexdump(pkt_buf))

    def pkt_layers_desc (self):
        """
        Return layer description in this format: IP:TCP:Pyload

        """
        pkt_buf = self._get_pkt_as_str()
        return self.pkt_layers_desc_from_buffer(pkt_buf)

    @staticmethod
    def pkt_layers_desc_from_buffer (pkt_buf):
        scapy_pkt = Ether(pkt_buf);
        pkt_utl = CTRexScapyPktUtl(scapy_pkt);
        return pkt_utl.get_pkt_layers()


    def set_pkt_as_str (self, pkt_buffer):
        validate_type('pkt_buffer', pkt_buffer, bytes)
        self.pkt_raw = pkt_buffer


    def set_pcap_file (self, pcap_file):
        """
        Load raw pcap file into a buffer. Loads only the first packet.

        :parameters:
            pcap_file : file_name

        :raises:
            + :exc:`AssertionError`, if packet is empty.

        """
        f_path = self._get_pcap_file_path (pcap_file)

        p=RawPcapReader(f_path)
        was_set = False

        for pkt in p:
            was_set=True;
            self.pkt_raw = pkt[0]
            break
        if not was_set :
            raise CTRexPacketBuildException(-14, "No buffer inside the pcap file {0}".format(f_path))

    def to_pkt_dump(self):
        p = self.pkt
        if p and isinstance(p, Packet):
            p.show2();
            hexdump(p);
            return;
        p = self.pkt_raw;
        if p:
            scapy_pkt = Ether(p);
            scapy_pkt.show2();
            hexdump(p);


    def set_packet (self, pkt):
        """
        Scapy packet 

        Example::

           pkt =Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)/IP()/('x'*10)

        """
        if  isinstance(pkt, Packet):
            self.pkt = pkt;
        else:
            if isinstance(pkt, str):
                self.set_pcap_file(pkt)
            else:
                raise CTRexPacketBuildException(-14, "bad packet" )

    def is_default_src_mac (self):
        if self.is_binary_source:
            return True
        p = self.pkt
        if isinstance(p, Packet):
            if isinstance(p,Ether):
                if 'src' in p.fields :
                    return False
        return True

    def is_default_dst_mac (self):
        if self.is_binary_source:
            return True
        p = self.pkt
        if isinstance(p, Packet):
            if isinstance(p,Ether):
                if 'dst' in p.fields :
                    return False
        return True

    def compile (self):
        if self.pkt == None and self.pkt_raw == None:
            raise CTRexPacketBuildException(-14, "Packet is empty")


        self.vm_low_level = CTRexVmEngine()

        # compile the VM
        for sc in self.vm_scripts:
            if isinstance(sc, STLScVmRaw):
                self._compile_raw(sc)

    def get_pkt_len (self):
        if self.pkt:
            return len(self.pkt)
        elif self.pkt_raw:
            return len(self.pkt_raw)
        else:
            raise CTRexPacketBuildException(-14, "Packet is empty")

    ####################################################
    # private


    def _get_pcap_file_path (self,pcap_file_name):
        f_path = pcap_file_name
        if os.path.isabs(pcap_file_name):
            f_path = pcap_file_name
        else:
            if self.path_relative_to_profile:
                p = self._get_path_relative_to_profile () # loader
                if p :
                  f_path=os.path.abspath(os.path.join(os.path.dirname(p),pcap_file_name))

        return f_path


    def _get_path_relative_to_profile (self):
        p = inspect.stack()
        for obj in p:
            if obj[3]=='get_streams':
                return obj[1]
        return None

    def _compile_raw (self,obj):

        # make sure we have varibles once
        vars={};

        # add it add var to dit
        for desc in obj.commands:
            var_names =  desc.get_var_name()

            if var_names :
                for var_name in var_names:
                    if var_name in vars:
                        raise CTRexPacketBuildException(-11,("Variable %s defined twice ") % (var_name)  );
                    else:
                        vars[var_name]=1

        # check that all write exits
        for desc in obj.commands:
            var_name =  desc.get_var_ref()
            if var_name :
                if not var_name in vars:
                    raise CTRexPacketBuildException(-11,("Variable %s does not exist  ") % (var_name) );
            desc.compile(self);

        for desc in obj.commands:
            self.vm_low_level.add_ins(desc.get_obj());

        # set split_by_var
        if obj.split_by_field :
            validate_type('obj.split_by_field', obj.split_by_field, str)
            self.vm_low_level.split_by_var = obj.split_by_field

        #set cache size 
        if obj.cache_size :
            validate_type('obj.cache_size', obj.cache_size, int)
            self.vm_low_level.cache_size = obj.cache_size



    # lazy packet build only on demand
    def __lazy_build_packet (self):
        # alrady built ? bail out
        if self.is_pkt_built:
            return

        # for buffer, promote to a scapy packet
        if self.pkt_raw:
            self.pkt = Ether(self.pkt_raw)
            self.pkt_raw = None

        # regular scapy packet
        elif not self.pkt:
            # should not reach here
            raise CTRexPacketBuildException(-11, 'Empty packet')

        if self.remove_fcs and self.pkt.lastlayer().name == 'Padding':
            self.pkt.lastlayer().underlayer.remove_payload()
        
        self.pkt.build()
        self.is_pkt_built = True

    def _pkt_layer_offset (self,layer_name):

        self.__lazy_build_packet()

        p_utl=CTRexScapyPktUtl(self.pkt);
        return p_utl.get_layer_offet_by_str(layer_name)

    def _name_to_offset(self,field_name):

        self.__lazy_build_packet()

        p_utl=CTRexScapyPktUtl(self.pkt);
        return p_utl.get_field_offet_by_str(field_name)

    def _get_pkt_as_str(self):

        if self.pkt:
            return bytes(self.pkt)

        if self.pkt_raw:
            return self.pkt_raw

        raise CTRexPacketBuildException(-11, 'Empty packet');

    def _add_tuple_gen(self,tuple_gen):

        pass;


def STLIPRange (src = None,
                dst = None,
                fix_chksum = True):

    vm = []

    if src:
        vm += [
               STLVmFlowVar(name="src", min_value = src['start'], max_value = src['end'], size = 4, op = "inc", step = src['step']),
               STLVmWrFlowVar(fv_name="src",pkt_offset= "IP.src")
              ]

    if dst:
        vm += [
                STLVmFlowVar(name="dst", min_value = dst['start'], max_value = dst['end'], size = 4, op = "inc", step = dst['step']),
                STLVmWrFlowVar(fv_name="dst",pkt_offset= "IP.dst")
              ]

    if fix_chksum:
        vm.append( STLVmFixIpv4(offset = "IP"))


    return vm

