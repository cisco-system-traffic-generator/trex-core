from .cap_handling import CPcapReader
from .arg_verify import ArgVerify
import os
import sys
import inspect
from .trex_astf_exceptions import ASTFError, ASTFErrorBadParamCombination, ASTFErrorMissingParam
from .trex_astf_global_info import ASTFGlobalInfo, ASTFGlobalInfoPerTemplate
import json
import base64
import hashlib


def listify(x):
    if isinstance(x, list):
        return x
    else:
        return [x]


class _ASTFCapPath(object):
    @staticmethod
    def get_pcap_file_path(pcap_file_name):
        f_path = pcap_file_name
        if not os.path.isabs(pcap_file_name):
            p = _ASTFCapPath.get_path_relative_to_profile()
            if p:
                f_path = os.path.abspath(os.path.join(os.path.dirname(p), pcap_file_name))

        return f_path

    @staticmethod
    def get_path_relative_to_profile():
        p = inspect.stack()
        for obj in p:
            if obj[3] == 'get_profile':
                return obj[1]
        return None


class ASTFCmd(object):
    def __init__(self):
        self.fields = {}

    def to_json(self):
        return dict(self.fields)

    def dump(self):
        ret = "{0}".format(self.__class__.__name__)
        return ret


class ASTFCmdSend(ASTFCmd):
    def __init__(self, buf):
        super(ASTFCmdSend, self).__init__()
        self._buf = base64.b64encode(buf).decode()
        self.fields['name'] = 'tx'
        self.fields['buf_index'] = -1
        self.buffer_len = len(buf)  # buf len before decode

    @property
    def buf_len(self):
        return self.buffer_len

    @property
    def buf(self):
        return self._buf

    @property
    def buf_index(self):
        return self.fields['buf_index']

    @buf_index.setter
    def index(self, index):
        self.fields['buf_index'] = index

    def dump(self):
        ret = "{0}(\"\"\"{1}\"\"\")".format(self.__class__.__name__, self.buf)
        return ret


class ASTFCmdRecv(ASTFCmd):
    def __init__(self, min_bytes):
        super(ASTFCmdRecv, self).__init__()
        self.fields['name'] = 'rx'
        self.fields['min_bytes'] = min_bytes

    def dump(self):
        ret = "{0}({1})".format(self.__class__.__name__, self.fields['min_bytes'])
        return ret


class ASTFCmdDelay(ASTFCmd):
    def __init__(self, msec):
        super(ASTFCmdDelay, self).__init__()
        self.fields['name'] = 'delay'
        self.fields['delay'] = msec


class ASTFCmdReset(ASTFCmd):
    def __init__(self):
        super(ASTFCmdReset, self).__init__()
        self.fields['name'] = 'reset'


class ASTFProgram(object):
    """

       Emulation L7 program

       .. code-block:: python

            # server commands
            prog_s = ASTFProgram()
            prog_s.recv(len(http_req))
            prog_s.send(http_response)
            prog_s.delay(10)
            prog_s.reset()

     """

    class BufferList():
        def __init__(self):
            self.buf_list = []
            self.buf_hash = {}

        def get_len(self):
            return len(self.buf_list)

        # add, and return index of added buffer
        def add(self, new_buf):
            m = hashlib.sha256(new_buf.encode()).digest()
            if m in self.buf_hash:
                return self.buf_hash[m]
            else:
                self.buf_list.append(new_buf)
                new_index = len(self.buf_list) - 1
                self.buf_hash[m] = new_index
                return new_index

            for index in range(0, len(self.buf_list)):
                if new_buf == self.buf_list[index]:
                    return index

            self.buf_list.append(new_buf)
            return len(self.buf_list) - 1

        def to_json(self):
            return self.buf_list

    buf_list = BufferList()

    @staticmethod
    def class_reset():
        ASTFProgram.buf_list = ASTFProgram.BufferList()

    @staticmethod
    def class_to_json():
        return ASTFProgram.buf_list.to_json()

    def __init__(self, file=None, side="c", commands=None):
        """

        :parameters:
                  file : string
                     pcap file to analyze

                  side : string
                        "c" for client side or "s" for server side

                  commands   : list
                        list of command objects cound be NULL in case you call the API

        """

        ver_args = {"types":
                    [{"name": "file", 'arg': file, "t": str, "must": False},
                     {"name": "commands", 'arg': commands, "t": ASTFCmd, "must": False, "allow_list": True},
                     ]}
        ArgVerify.verify(self.__class__.__name__, ver_args)
        side_vals = ["c", "s"]
        if side not in side_vals:
            raise ASTFError("Side must be one of {0}".side_vals)

        self.fields = {}
        self.fields['commands'] = []
        self.total_send_bytes = 0
        self.total_rcv_bytes = 0
        if file is not None:
            cap = CPcapReader(_ASTFCapPath.get_pcap_file_path(file))
            cap.analyze()
            self._p_len = cap.payload_len
            cap.condense_pkt_data()
            self._create_cmds_from_cap(cap.pkts, side)
        else:
            if commands is not None:
                self._set_cmds(listify(commands))

    def calc_hash(self):
        return hashlib.sha256(repr(self.to_json()).encode()).digest()

    def send(self, buf):
        """
        send (l7_buffer)

        :parameters:
                  buf : string
                     l7 stream

        """

        ver_args = {"types":
                    [{"name": "buf", 'arg': buf, "t": [bytes, str]}]
                    }
        ArgVerify.verify(self.__class__.__name__ + "." + sys._getframe().f_code.co_name, ver_args)

        # we support bytes or ascii strings
        if type(buf) is str:
            try:
                enc_buf = buf.encode('ascii')
            except UnicodeEncodeError as e:
                print (e)
                raise ASTFError("If buf is a string, it must contain only ascii")
        else:
            enc_buf = buf

        cmd = ASTFCmdSend(enc_buf)
        self.total_send_bytes += cmd.buf_len
        cmd.index = ASTFProgram.buf_list.add(cmd.buf)
        self.fields['commands'].append(cmd)

    def recv(self, bytes):
        """
        recv (bytes)

        :parameters:
                  bytes  : uint32_t
                     wait until we receive at least  x number of bytes

        """

        ver_args = {"types":
                    [{"name": "bytes", 'arg': bytes, "t": int}]
                    }
        ArgVerify.verify(self.__class__.__name__ + "." + sys._getframe().f_code.co_name, ver_args)

        self.total_rcv_bytes += bytes
        self.fields['commands'].append(ASTFCmdRecv(self.total_rcv_bytes))

    def delay(self, msec):
        """
        delay for x usec

        :parameters:
                  msec  : float
                     delay for this time in usec

        """

        ver_args = {"types":
                    [{"name": "msec", 'arg': msec, "t": [int, float]}]
                    }
        ArgVerify.verify(self.__class__.__name__ + "." + sys._getframe().f_code.co_name, ver_args)

        self.fields['commands'].append(ASTFCmdDelay(msec))

    def reset(self):
        """
        for TCP connection send RST to peer

        """

        self.fields['commands'].append(ASTFCmdReset())

    def _set_cmds(self, cmds):
        for cmd in cmds:
            if type(cmd) is ASTFCmdSend:
                self.total_send_bytes += cmd.buf_len
                cmd.index = ASTFProgram.buf_list.add(cmd.buf)
            self.fields['commands'].append(cmd)

    def _create_cmds_from_cap(self, cmds, init_side):
        new_cmds = []
        origin = init_side
        tot_rcv_bytes = 0

        for cmd in cmds:
            if origin == "c":
                new_cmd = ASTFCmdSend(cmd.payload)
                origin = "s"
            else:
                # Server need to see total rcv bytes, and not amount for each packet
                tot_rcv_bytes += len(cmd.payload)
                new_cmd = ASTFCmdRecv(tot_rcv_bytes)
                origin = "c"
            new_cmds.append(new_cmd)
        self._set_cmds(new_cmds)

    def to_json(self):
        ret = {}
        ret['commands'] = []
        for cmd in self.fields['commands']:
            ret['commands'].append(cmd.to_json())
        return ret

    def dump(self, out, var_name):
        out.write("cmd_list = []\n")
        for cmd in self.fields['commands']:
            out.write("cmd_list.append({0})\n".format(cmd.dump()))

        out.write("{0} = {1}()\n".format(var_name, self.__class__.__name__))
        out.write("{0}.set_cmds(cmd_list)\n".format(var_name))

    @property
    def payload_len(self):
        return self._p_len


class ASTFIPGenDist(object):
    """
        .. code-block:: python

            ip_gen_c = ASTFIPGenDist(ip_range=["16.0.0.0", "16.0.0.255"], distribution="seq")

    """

    in_list = []

    @staticmethod
    def class_to_json():
        ret = []
        for i in range(0, len(ASTFIPGenDist.in_list)):
            ret.append(ASTFIPGenDist.in_list[i].to_json())

        return ret

    @staticmethod
    def class_reset():
        ASTFIPGenDist.in_list = []

    class Inner(object):
        def __init__(self, ip_range, distribution="seq"):
            self.fields = {}
            self.fields['ip_start'] = ip_range[0]
            self.fields['ip_end'] = ip_range[1]
            self.fields['distribution'] = distribution

        def __eq__(self, other):
            if self.fields == other.fields:
                return True
            return False

        @property
        def ip_start(self):
            return self.fields['ip_start']

        @property
        def ip_end(self):
            return self.fields['ip_end']

        @property
        def distribution(self):
            return self.fields['distribution']

        @property
        def direction(self):
            return self.fields['dir']

        @direction.setter
        def direction(self, direction):
            self.fields['dir'] = direction

        @property
        def ip_offset(self):
            return self.fields['ip_offset']

        @ip_offset.setter
        def ip_offset(self, ip_offset):
            self.fields['ip_offset'] = ip_offset

        def to_json(self):
            return dict(self.fields)

    def __init__(self, ip_range, distribution="seq"):

        """
        Define a ASTFIPGenDist


        :parameters:
                  ip_range  : list of  min-max ip strings

                  distribution  : string
                      "seq" or "rand"

        """

        ver_args = {"types":
                    [{"name": "ip_range", 'arg': ip_range, "t": "ip range", "must": True},
                     ]}
        ArgVerify.verify(self.__class__.__name__, ver_args)
        distribution_vals = ["seq", "rand"]
        if distribution not in distribution_vals:
            raise ASTFError("Distribution must be one of {0}".format(distribution_vals))

        new_inner = self.Inner(ip_range=ip_range, distribution=distribution)
        for i in range(0, len(self.in_list)):
            if new_inner == self.in_list[i]:
                self.index = i
                return
        self.in_list.append(new_inner)
        self.index = len(self.in_list) - 1

    @property
    def ip_start(self):
        return self.in_list[self.index].ip_start

    @property
    def ip_end(self):
        return self.in_list[self.index].ip_end

    @property
    def distribution(self):
        return self.in_list[self.index].distribution

    @property
    def direction(self):
        return self.in_list[self.index].direction

    @direction.setter
    def direction(self, direction):
        self.in_list[self.index].direction = direction

    @property
    def ip_offset(self):
        return self.in_list[self.index].ip_offset

    @ip_offset.setter
    def ip_offset(self, ip_offset):
        self.in_list[self.index].ip_offset = ip_offset

    def to_json(self):
        return {"index": self.index}


class ASTFIPGenGlobal(object):
    """
        .. code-block:: python

            ip_gen_c = ASTFIPGenGlobal(ip_offset="1.0.0.0")

    """

    def __init__(self, ip_offset="1.0.0.0"):
        """
        Global properties for IP generator


        :parameters:
                  ip_offset  : offset for dual mask ports

        """

        ver_args = {"types":
                    [{"name": "ip_offset", 'arg': ip_offset, "t": "ip address", "must": False},
                     ]}
        ArgVerify.verify(self.__class__.__name__, ver_args)

        self.fields = {}
        self.fields['ip_offset'] = ip_offset

    @property
    def ip_offset(self):
        return self.fields['ip_offset']

    def to_json(self):
        return dict(self.fields)


class ASTFIPGen(object):
    """

        .. code-block:: python

            ip_gen_c = ASTFIPGenDist(ip_range=["16.0.0.0", "16.0.0.255"], distribution="seq")
            ip_gen_s = ASTFIPGenDist(ip_range=["48.0.0.0", "48.0.255.255"], distribution="seq")
            ip_gen = ASTFIPGen(glob=ASTFIPGenGlobal(ip_offset="1.0.0.0"),
                               dist_client=ip_gen_c,
                               dist_server=ip_gen_s)


    """

    def __init__(self, dist_client, dist_server, glob=ASTFIPGenGlobal()):
        """
        Define a ASTFIPGen generator


        :parameters:
                  dist_client  : Client side ASTFIPGenDist  :class:`trex_astf_lib.trex_astf_client.ASTFIPGenDist`

                  dist_server  : Server side ASTFIPGenDist  :class:`trex_astf_lib.trex_astf_client.ASTFIPGenDist`

                  glob :  ASTFIPGenGlobal see :class:`trex_astf_lib.trex_astf_client.ASTFIPGenGlobal`
        """

        ver_args = {"types":
                    [{"name": "glob", 'arg': glob, "t": ASTFIPGenGlobal, "must": False},
                     {"name": "dist_client", 'arg': dist_client, "t": ASTFIPGenDist, "must": True},
                     {"name": "dist_server", 'arg': dist_server, "t": ASTFIPGenDist, "must": True},
                     ]}
        ArgVerify.verify(self.__class__.__name__, ver_args)

        self.fields = {}
        self.fields['dist_client'] = dist_client
        dist_client.direction = "c"
        dist_client.ip_offset = glob.ip_offset
        self.fields['dist_server'] = dist_server
        dist_server.direction = "s"
        dist_server.ip_offset = glob.ip_offset

    @staticmethod
    def __str__():
        return "IPGen"

    def to_json(self):
        ret = {}
        for field in self.fields.keys():
            ret[field] = self.fields[field].to_json()

        return ret


# for future use
class ASTFCluster(object):
    def to_json(self):
        return {}


class ASTFTCPOptions(object):
    """
       .. code-block:: python

            opts = list of TCP option tuples (name, value)

    """

    def __init__(self, opts=None):
        self.fields = {}
        if opts is not None:
            for opt in opts:
                self.fields[opt[0]] = opt[1]

    @property
    def mss(self, val):
        return self.fields['MSS']

    @mss.setter
    def mss(self, val):
        self.fields['MSS'] = val

    def to_json(self):
        return dict(self.fields)

    def __eq__(self, other):
        for key in self.fields.keys():
            if self.fields[key] != other.fields[key]:
                return False
            return True


class ASTFTCPInfo(object):
    """
       .. code-block:: python

            tcp_params = ASTFTCPInfo(window=32768)

    """

    in_list = []
    DEFAULT_WIN = 32768
    DEFAULT_PORT = 80

    @staticmethod
    def class_reset():
        ASTFTCPInfo.in_list = []

    @staticmethod
    def class_to_json():
        ret = []
        for i in range(0, len(ASTFTCPInfo.in_list)):
            ret.append(ASTFTCPInfo.in_list[i].to_json())

        return ret

    class Inner(object):
        def __init__(self, window=32768, options=None):
            self.fields = {}
            self.fields['window'] = window
            self.fields['options'] = options

        def __eq__(self, other):
            if other is None:
                return False

            for key in self.fields.keys():
                if other.fields[key] is None:
                    return False
                if self.fields[key] != other.fields[key]:
                    return False
            return True

        @property
        def window(self):
            return self.fields['window']

        @property
        def options(self):
            return self.fields['options']

        def to_json(self):
            ret = {}
            ret['window'] = self.fields['window']
            if self.fields['options'] is not None:
                ret['options'] = self.fields['options'].to_json()
            return ret

    def __init__(self, window=None, options=None, port=None, file=None, side="c"):
        """

        :parameters:

                  window  : uint32_t
                      TCP window size default 32KB.  If given together with file, overrides info from file.

                  side    : string
                        "c" or "s". If file argument is given, determines if TCP information will be taken from "client" or "server" side

                  file    : string
                        pcap file to learn the TCP information from.

                  port    :  uint16_t
                        destination port. If given together with file, overrides info from file.

                  options  : uint16_t
                        TCP options to use (ASTFTCPOptions object).  If given together with file, overrides info from file.


        """

        ver_args = {"types":
                    [{"name": "options", 'arg': options, "t": ASTFTCPOptions, "must": False}
                     ]}
        ArgVerify.verify(self.__class__.__name__, ver_args)

        side_vals = ["c", "s"]
        if side not in side_vals:
            raise ASTFError("Side must be one of {0}".format(side_vals))

        new_opt = None
        new_port = None
        new_win = None
        if file is not None:
            cap = CPcapReader(_ASTFCapPath.get_pcap_file_path(file))
            cap.analyze()
            if side == "c":
                new_win = cap.c_tcp_win
                new_opt = ASTFTCPOptions(cap.c_tcp_opts)
                new_port = cap.d_port
            else:
                new_win = cap.s_tcp_win
                new_opt = ASTFTCPOptions(cap.s_tcp_opts)
                # In case of server destination port is the source port received from client
                new_port = None

        # can override parameters from file
        if window is not None:
            new_win = window
        else:
            if new_win is None:
                new_win = ASTFTCPInfo.DEFAULT_WIN
        if options is not None:
            new_opt = options
        if port is not None:
            new_port = port
        else:
            if new_port is None:
                new_port = ASTFTCPInfo.DEFAULT_PORT

        self.m_port = new_port
        new_inner = self.Inner(window=new_win, options=new_opt)
        for i in range(0, len(self.in_list)):
            if new_inner == self.in_list[i]:
                self.index = i
                return
        self.in_list.append(new_inner)
        self.index = len(self.in_list) - 1

    @property
    def window(self):
        return self.in_list[self.index].window

    @property
    def options(self):
        return self.in_list[self.index].options

    @property
    def port(self):
        return self.m_port

    def to_json(self):
        return {"index": self.index}

    def dump(self, out, var_name):
        win = self.window
        opt = self.options
        out.write("{0} = {1}(window={2}, options={3})\n".format(self.__class__.__name__, var_name, win, opt))


class ASTFAssociationRule(object):
    """
       .. code-block:: python

            # only port
            assoc=ASTFAssociationRule(port=81)

            # port with range or destination ips
            assoc=ASTFAssociationRule(port=81,ip_start="48.0.0.1",ip_end="48.0.0,16")

    """

    def __init__(self, port=80, ip_start=None, ip_end=None):
        """

        :parameters:
                  port  : uint16_t
                      destination port

                  ip_start : string
                      stop ip range

                  ip_end   : string
                      stop ip range

        """

        ver_args = {"types":
                    [{"name": "ip_start", 'arg': ip_start, "t": "ip address", "must": False},
                     {"name": "ip_end", 'arg': ip_end, "t": "ip address", "must": False},
                     ]}
        ArgVerify.verify(self.__class__.__name__, ver_args)

        self.fields = {}
        self.fields['port'] = port
        if ip_start is not None:
            self.fields['ip_start'] = str(ip_start)
        if ip_end is not None:
            self.fields['ip_end'] = str(ip_end)

    @property
    def port(self):
        return self.fields['port']

    def to_json(self):
        return dict(self.fields)


class ASTFAssociation(object):
    """
       .. code-block:: python

            assoc=ASTFAssociationRule(port=81)

    """
    def __init__(self, rules=ASTFAssociationRule()):
        """

        :parameters:
                  rules  : ASTFAssociationRule see :class:`trex_astf_lib.trex_astf_client.ASTFAssociationRule`
                       rule or rules list

        """

        ver_args = {"types":
                    [{"name": "rules", 'arg': rules, "t": ASTFAssociationRule, "must": False, "allow_list": True},
                     ]}
        ArgVerify.verify(self.__class__.__name__, ver_args)

        self.rules = listify(rules)

    def to_json(self):
        ret = []
        for rule in self.rules:
            ret.append(rule.to_json())
        return ret

    @property
    def port(self):
        if len(self.rules) != 1:
            pass  # exception

        return self.rules[0].port


class _ASTFTemplateBase(object):
    program_list = []
    program_hash = {}

    @staticmethod
    def add_program(program):
        m = program.calc_hash()
        if m in _ASTFTemplateBase.program_hash:
            return _ASTFTemplateBase.program_hash[m]
        else:
            _ASTFTemplateBase.program_list.append(program)
            prog_index = len(_ASTFTemplateBase.program_list) - 1
            _ASTFTemplateBase.program_hash[m] = prog_index
            return prog_index

    @staticmethod
    def get_total_send_bytes(ind):
        return _ASTFTemplateBase.program_list[ind].total_send_bytes

    @staticmethod
    def num_programs():
        return len(_ASTFTemplateBase.program_list)

    @staticmethod
    def class_reset():
        _ASTFTemplateBase.program_list = []
        _ASTFTemplateBase.program_hash = {}

    @staticmethod
    def class_to_json():
        ret = []
        for i in range(0, len(_ASTFTemplateBase.program_list)):
            ret.append(_ASTFTemplateBase.program_list[i].to_json())
        return ret

    def __init__(self, program=None):
        self.fields = {}
        self.fields['program_index'] = _ASTFTemplateBase.add_program(program)

    def to_json(self):
        ret = {}
        ret['program_index'] = self.fields['program_index']

        return ret


class _ASTFClientTemplate(_ASTFTemplateBase):
    def __init__(self, ip_gen, cluster=ASTFCluster(), program=None):
        super(_ASTFClientTemplate, self).__init__(program=program)
        self.fields['ip_gen'] = ip_gen
        self.fields['cluster'] = cluster

    def to_json(self):
        ret = super(_ASTFClientTemplate, self).to_json()
        ret['ip_gen'] = self.fields['ip_gen'].to_json()
        ret['cluster'] = self.fields['cluster'].to_json()
        return ret


class ASTFTCPClientTemplate(_ASTFClientTemplate):
    """
       One manual template

       .. code-block:: python

            # client commands
            prog_c = ASTFProgram()
            prog_c.send(http_req)
            prog_c.recv(len(http_response))


            # ip generator
            ip_gen_c = ASTFIPGenDist(ip_range=["16.0.0.0", "16.0.0.255"], distribution="seq")
            ip_gen_s = ASTFIPGenDist(ip_range=["48.0.0.0", "48.0.255.255"], distribution="seq")
            ip_gen = ASTFIPGen(glob=ASTFIPGenGlobal(ip_offset="1.0.0.0"),
                               dist_client=ip_gen_c,
                               dist_server=ip_gen_s)

            tcp_params = ASTFTCPInfo(window=32768)

            # template
            temp_c = ASTFTCPClientTemplate(program=prog_c, tcp_info=tcp_params, ip_gen=ip_gen)

     """

    def __init__(self, ip_gen, cluster=ASTFCluster(), tcp_info=ASTFTCPInfo(), program=None,
                 port=80, cps=1, glob_info=None):
        """

        :parameters:
                  ip_gen  : ASTFIPGen see :class:`trex_astf_lib.trex_astf_client.ASTFIPGen`
                       generator

                  cluster :  ASTFCluster see :class:`trex_astf_lib.trex_astf_client.ASTFCluster`

                  tcp_info : ASTFTCPInfo see :class:`trex_astf_lib.trex_astf_client.ASTFTCPInfo`

                  program  : ASTFProgram see :class:`trex_astf_lib.trex_astf_client.ASTFProgram`
                        L7 emulation program

                  port     : uint16_t
                        destination port

                  cps      : float
                        New connection per second rate

        """

        ver_args = {"types":
                    [{"name": "ip_gen", 'arg': ip_gen, "t": ASTFIPGen},
                     {"name": "cluster", 'arg': cluster, "t": ASTFCluster, "must": False},
                     {"name": "tcp_info", 'arg': tcp_info, "t": ASTFTCPInfo, "must": False},
                     {"name": "program", 'arg': program, "t": ASTFProgram}]
                    }
        ArgVerify.verify(self.__class__.__name__, ver_args)

        super(ASTFTCPClientTemplate, self).__init__(ip_gen=ip_gen, cluster=cluster, program=program)
        self.fields['tcp_info'] = tcp_info
        self.fields['port'] = port
        self.fields['cps'] = cps
        self.fields['glob_info'] = glob_info

    def to_json(self):
        ret = super(ASTFTCPClientTemplate, self).to_json()
        ret['port'] = self.fields['port']
        ret['cps'] = self.fields['cps']
        if self.fields['glob_info'] is not None:
            ret['glob_info'] = self.fields['glob_info'].to_json()
        return ret


class ASTFTCPServerTemplate(_ASTFTemplateBase):
    """
       One server side template

       .. code-block:: python

            # server commands
            prog_s = ASTFProgram()
            prog_s.recv(len(http_req))
            prog_s.send(http_response)


            # template
            temp_s = ASTFTCPServerTemplate(program=prog_s, tcp_info=tcp_params)  # using default association

     """

    def __init__(self, tcp_info=ASTFTCPInfo(), program=None, assoc=ASTFAssociation(), glob_info=None):
        """

        :parameters:
                  tcp_info : ASTFTCPInfo see :class:`trex_astf_lib.trex_astf_client.ASTFTCPInfo`

                  program  : ASTFProgram see :class:`trex_astf_lib.trex_astf_client.ASTFProgram`
                        L7 emulation program

                  assoc    : ASTFAssociation see :class:`trex_astf_lib.trex_astf_client.ASTFAssociation`

        """
        ver_args = {"types":
                    [{"name": "tcp_info", 'arg': tcp_info, "t": ASTFTCPInfo, "must": False},
                     {"name": "assoc", 'arg': assoc, "t": [ASTFAssociation, ASTFAssociationRule], "must": False},
                     {"name": "program", 'arg': program, "t": ASTFProgram}]
                    }
        ArgVerify.verify(self.__class__.__name__, ver_args)

        super(ASTFTCPServerTemplate, self).__init__(program=program)
        self.fields['tcp_info'] = tcp_info
        if isinstance(assoc, ASTFAssociationRule):
            new_assoc = ASTFAssociation(rules=assoc)
            self.fields['assoc'] = new_assoc
        else:
            self.fields['assoc'] = assoc
        self.fields['glob_info'] = glob_info

    def to_json(self):
        ret = super(ASTFTCPServerTemplate, self).to_json()
        ret['assoc'] = self.fields['assoc'].to_json()
        if self.fields['glob_info'] is not None:
            ret['glob_info'] = self.fields['glob_info'].to_json()
        return ret


class ASTFCapInfo(object):
    """

        .. code-block:: python

            ASTFCapInfo(file="../avl/delay_10_http_browsing_0.pcap",cps=1)

            ASTFCapInfo(file="../avl/delay_10_http_browsing_0.pcap",l7_percent=10.0)

            ASTFCapInfo(file="../avl/delay_10_http_browsing_0.pcap",l7_percent=10.0,port=8080)

            ASTFCapInfo(file="../avl/delay_10_http_browsing_0.pcap",l7_percent=10.0,port=8080,ip_gen=Mygen)

    """

    def __init__(self, file=None, cps=None, assoc=None, ip_gen=None, port=None, l7_percent=None,
                 s_glob_info=None, c_glob_info=None):
        """
        Define one template information based on pcap file analysis

        :parameters:
                  file  : string
                      pcap file name. Filesystem directory location is relative to the profile file in case it is not start with /

                  cps  :  float
                       new connection per second rate

                  assoc :  ASTFAssociation see :class:`trex_astf_lib.trex_astf_client.ASTFAssociation`
                       rule for server association in default take the destination port from pcap file

                  ip_gen  : ASTFIPGen see :class:`trex_astf_lib.trex_astf_client.ASTFIPGen`
                      tuple generator for this template

                  port    : uint16_t
                      Override destination port, by default is taken from pcap

                  l7_percent :  float
                        L7 stream bandwidth percent

        """

        ver_args = {"types":
                    [{"name": "file", 'arg': file, "t": str},
                     {"name": "assoc", 'arg': assoc, "t": [ASTFAssociation, ASTFAssociationRule], "must": False},
                     {"name": "ip_gen", 'arg': ip_gen, "t": ASTFIPGen, "must": False},
                     {"name": "c_glob_info", 'arg': c_glob_info, "t": ASTFGlobalInfoPerTemplate, "must": False},
                     {"name": "s_glob_info", 'arg': s_glob_info, "t": ASTFGlobalInfoPerTemplate, "must": False}]}
        ArgVerify.verify(self.__class__.__name__, ver_args)

        if l7_percent is not None:
            if cps is not None:
                raise ASTFErrorBadParamCombination(self.__class__.__name__, "cps", "l7_percent")
            self.l7_percent = l7_percent
            self.cps = None
        else:
            if cps is not None:
                self.cps = cps
            else:
                self.cps = 1
            self.l7_percent = None

        self.file = file
        if assoc is not None:
            if type(assoc) is ASTFAssociationRule:
                self.assoc = ASTFAssociation(assoc)
            else:
                self.assoc = assoc
        else:
            if port is not None:
                self.assoc = ASTFAssociation(ASTFAssociationRule(port=port))
            else:
                self.assoc = None
        self.ip_gen = ip_gen
        self.c_glob_info = c_glob_info
        self.s_glob_info = s_glob_info


class ASTFTemplate(object):
    """
       One manual template

       .. code-block:: python

            # client commands
            prog_c = ASTFProgram()
            prog_c.send(http_req)
            prog_c.recv(len(http_response))

            prog_s = ASTFProgram()
            prog_s.recv(len(http_req))
            prog_s.send(http_response)

            # ip generator
            ip_gen_c = ASTFIPGenDist(ip_range=["16.0.0.0", "16.0.0.255"], distribution="seq")
            ip_gen_s = ASTFIPGenDist(ip_range=["48.0.0.0", "48.0.255.255"], distribution="seq")
            ip_gen = ASTFIPGen(glob=ASTFIPGenGlobal(ip_offset="1.0.0.0"),
                               dist_client=ip_gen_c,
                               dist_server=ip_gen_s)

            tcp_params = ASTFTCPInfo(window=32768)

            # template
            temp_c = ASTFTCPClientTemplate(program=prog_c, tcp_info=tcp_params, ip_gen=ip_gen)
            temp_s = ASTFTCPServerTemplate(program=prog_s, tcp_info=tcp_params)  # using default association
            template = ASTFTemplate(client_template=temp_c, server_template=temp_s)

     """

    def __init__(self, client_template=None, server_template=None):
        """
        Define a ASTF profile

        You should give either templates or cap_list (mutual exclusion).

        :parameters:
                  client_template  : ASTFTCPClientTemplate see :class:`trex_astf_lib.trex_astf_client.ASTFTCPClientTemplate`
                       client side template info

                  server_template  :  ASTFTCPServerTemplate see :class:`trex_astf_lib.trex_astf_client.ASTFTCPServerTemplate`
                       server side template info

        """
        ver_args = {"types":
                    [{"name": "client_template", 'arg': client_template, "t": ASTFTCPClientTemplate},
                     {"name": "server_template", 'arg': server_template, "t": ASTFTCPServerTemplate}]
                    }
        ArgVerify.verify(self.__class__.__name__, ver_args)

        self.fields = {}
        self.fields['client_template'] = client_template
        self.fields['server_template'] = server_template

    def to_json(self):
        ret = {}
        for field in self.fields.keys():
            ret[field] = self.fields[field].to_json()

        return ret


class ASTFProfile(object):
    """ ASTF profile

       .. code-block:: python

            ip_gen_c = ASTFIPGenDist(ip_range=["16.0.0.0", "16.0.0.255"], distribution="seq")
            ip_gen_s = ASTFIPGenDist(ip_range=["48.0.0.0", "48.0.255.255"], distribution="seq")
            ip_gen = ASTFIPGen(glob=ASTFIPGenGlobal(ip_offset="1.0.0.0"),
                               dist_client=ip_gen_c,
                               dist_server=ip_gen_s)

            return ASTFProfile(default_ip_gen=ip_gen,
                                cap_list=[ASTFCapInfo(file="../avl/delay_10_http_browsing_0.pcap",cps=1)])

    """
    def __init__(self, default_ip_gen, default_c_glob_info=None, default_s_glob_info=None,
                 templates=None, cap_list=None):
        """
        Define a ASTF profile

        You should give either templates or cap_list (mutual exclusion).

        :parameters:
                  default_ip_gen  : ASTFIPGen  :class:`trex_astf_lib.trex_astf_client.ASTFIPGen`
                       tuple generator object

                  default_tcp_server_info  :  ASTFTCPInfo :class:`trex_astf_lib.trex_astf_client.ASTFTCPInfo`
                       tcp parameters to be used for server side, if cap_list is given. This is optional. If not specified,
                       TCP parameters for each flow will be taken from its cap file.

                  default_tcp_client_info  :  ASTFTCPInfo :class:`trex_astf_lib.trex_astf_client.ASTFTCPInfo`
                       Same as default_tcp_server_info for client side.

                  templates  :  ASTFTemplate see :class:`trex_astf_lib.trex_astf_client.ASTFTemplate`
                       define a list of manual templates or one template

                  cap_list  : ASTFCapInfo see :class:`trex_astf_lib.trex_astf_client.ASTFCapInfo`
                      define a list of pcap files list in case there is no  templates
        """

        ver_args = {"types":
                    [{"name": "templates", 'arg': templates, "t": ASTFTemplate, "allow_list": True, "must": False},
                     {"name": "cap_list", 'arg': cap_list, "t": ASTFCapInfo, "allow_list": True, "must": False},
                     {"name": "default_ip_gen", 'arg': default_ip_gen, "t": ASTFIPGen},
                     {"name": "default_c_glob_info", 'arg': default_c_glob_info, "t": ASTFGlobalInfo, "must": False},
                     {"name": "default_s_glob_info", 'arg': default_s_glob_info, "t": ASTFGlobalInfo, "must": False}]
                    }
        ArgVerify.verify(self.__class__.__name__, ver_args)

        self.default_c_glob_info = default_c_glob_info
        self.default_s_glob_info = default_s_glob_info
        if cap_list is not None:
            if templates is not None:
                raise ASTFErrorBadParamCombination(self.__class__.__name__, "templates", "cap_list")
            self.cap_list = listify(cap_list)
            self.templates = []
        else:
            if templates is None:
                raise ASTFErrorMissingParam(self.__class__.__name__, "templates", "cap_list")
            self.templates = listify(templates)

        if cap_list is not None:
            mode = None
            all_cap_info = []
            d_ports = []
            total_payload = 0
            for cap in self.cap_list:
                cap_file = cap.file
                ip_gen = cap.ip_gen if cap.ip_gen is not None else default_ip_gen
                glob_c = cap.c_glob_info
                glob_s = cap.s_glob_info
                prog_c = ASTFProgram(file=cap_file, side="c")
                prog_s = ASTFProgram(file=cap_file, side="s")

                #                if default_tcp_client_info is not None:
                #                   tcp_c = default_tcp_client_info
                #                   tcp_c_from_file = ASTFTCPInfo(file=cap_file, side="c")
                # We want to take port from file and not from default, because it should be different for each file
                #                   tcp_c_port = tcp_c_from_file.port
                #               else:
                tcp_c = ASTFTCPInfo(file=cap_file, side="c")
                tcp_c_port = tcp_c.port

                cps = cap.cps
                l7_percent = cap.l7_percent
                if mode is None:
                    if l7_percent is not None:
                        mode = "l7_percent"
                    else:
                        mode = "cps"
                else:
                    if mode == "l7_percent" and l7_percent is None:
                        raise ASTFError("If one cap specifies l7_percent, then all should specify it")
                    if mode is "cps" and l7_percent is not None:
                        raise ASTFError("Can't mix specifications of cps and l7_percent in same cap list")

                total_payload += prog_c.payload_len
                if cap.assoc is None:
                    d_port = tcp_c_port
                    my_assoc = ASTFAssociation(rules=ASTFAssociationRule(port=d_port))
                else:
                    d_port = cap.assoc.port
                    my_assoc = cap.assoc
                if d_port in d_ports:
                    raise ASTFError("More than one cap use dest port {0}. This is currently not supported.".format(d_port))
                d_ports.append(d_port)

                all_cap_info.append({"ip_gen": ip_gen, "prog_c": prog_c, "prog_s": prog_s, "glob_c": glob_c, "glob_s": glob_s,
                                     "cps": cps, "d_port": d_port, "my_assoc": my_assoc})

            # calculate cps from l7 percent
            if mode == "l7_percent":
                percent_sum = 0
                for c in all_cap_info:
                    c["cps"] = c["prog_c"].payload_len * 100.0 / total_payload
                    percent_sum += c["cps"]
                if percent_sum != 100:
                    raise ASTFError("l7_percent values must sum up to 100")

            for c in all_cap_info:
                temp_c = ASTFTCPClientTemplate(program=c["prog_c"], glob_info=c["glob_c"], ip_gen=c["ip_gen"], port=c["d_port"],
                                               cps=c["cps"])
                temp_s = ASTFTCPServerTemplate(program=c["prog_s"], glob_info=c["glob_s"], assoc=c["my_assoc"])
                template = ASTFTemplate(client_template=temp_c, server_template=temp_s)
                self.templates.append(template)

    def to_json(self):
        ret = {}
        ret['buf_list'] = ASTFProgram.class_to_json()
        ret['ip_gen_dist_list'] = ASTFIPGenDist.class_to_json()
        ret['program_list'] = _ASTFTemplateBase.class_to_json()
        if self.default_c_glob_info is not None:
            ret['c_glob_info'] = self.default_c_glob_info.to_json()
        if self.default_s_glob_info is not None:
            ret['s_glob_info'] = self.default_s_glob_info.to_json()
        ret['templates'] = []
        for i in range(0, len(self.templates)):
            ret['templates'].append(self.templates[i].to_json())

        return json.dumps(ret, indent=4, separators=(',', ': '))

    def print_stats(self):
        tot_bps = 0
        tot_cps = 0
        print ("Num buffers: {0}".format(ASTFProgram.buf_list.get_len()))
        print ("Num programs: {0}".format(_ASTFTemplateBase.num_programs()))
        for i in range(0, len(self.templates)):
            print ("template {0}:".format(i))
            d = self.templates[i].to_json()
            c_prog_ind = d['client_template']['program_index']
            s_prog_ind = d['server_template']['program_index']
            tot_bytes = _ASTFTemplateBase.get_total_send_bytes(c_prog_ind) + _ASTFTemplateBase.get_total_send_bytes(s_prog_ind)
            temp_cps = d['client_template']['cps']
            temp_bps = tot_bytes * temp_cps * 8
            print ("  total bytes:{0} cps:{1} bps(bytes * cps * 8):{2}".format(tot_bytes, temp_cps, temp_bps))
            tot_bps += temp_bps
            tot_cps += temp_cps
        print("total for all templates - cps:{0} bps:{1}".format(tot_cps, tot_bps))
