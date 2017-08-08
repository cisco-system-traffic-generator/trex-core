from pprint import pprint
from cap_handling import CPcapReader
from arg_verify import ArgVerify
import os
import inspect
from trex_nstf_exceptions import NSTFError, NSTFErrorBadParamCombination, NSTFErrorMissingParam
import json
import base64
import hashlib

def listify(x):
    if isinstance(x, list):
        return x
    else:
        return [x]


class _NSTFCapPath(object):
    @staticmethod
    def get_pcap_file_path(pcap_file_name):
        f_path = pcap_file_name
        if not os.path.isabs(pcap_file_name):
            p = _NSTFCapPath.get_path_relative_to_profile()
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


class NSTFCmd(object):
    def __init__(self):
        self.fields = {}

    def to_json(self):
        return dict(self.fields)

    def dump(self):
        ret = "{0}".format(self.__class__.__name__)
        return ret


class NSTFCmdTx(NSTFCmd):
    def __init__(self, buf):
        super(NSTFCmdTx, self).__init__()
        self._buf = base64.b64encode(buf).decode()
        self.fields['name'] = 'tx'
        self.fields['buf_index'] = -1

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


class NSTFCmdRecv(NSTFCmd):
    def __init__(self, min_bytes):
        super(NSTFCmdRecv, self).__init__()
        self.fields['name'] = 'rcv'
        self.fields['min_bytes'] = min_bytes

    def dump(self):
        ret = "{0}({1})".format(self.__class__.__name__, self.fields['min_bytes'])
        return ret


class NSTFCmdDelay(NSTFCmd):
    def __init__(self, msec):
        super(NSTFCmdDelay, self).__init__()
        self.fields['name'] = 'delay'
        self.fields['delay'] = msec


class NSTFCmdReset(NSTFCmd):
    def __init__(self):
        super(NSTFCmdReset, self).__init__()
        self.fields['name'] = 'reset'


class NSTFProgram(object):
    class BufferList():
        def __init__(self):
            self.buf_list = []
            self.buf_hash = {}

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
        NSTFProgram.buf_list = NSTFProgram.BufferList()

    @staticmethod
    def class_to_json():
        return NSTFProgram.buf_list.to_json()

    def __init__(self, file=None, side="c", commands=None):
        ver_args = {"types":
                    [{"name": "file", 'arg': file, "t": str, "must": False},
                     {"name": "commands", 'arg': commands, "t": NSTFCmd, "must": False, "allow_list": True},
                     ]}
        ArgVerify.verify(self.__class__.__name__, ver_args)
        side_vals = ["c", "s"]
        if side not in side_vals:
            raise NSTFError("Side must be one of {0}".side_vals)

        self.fields = {}
        self.fields['commands'] = []
        if file is not None:
            cap = CPcapReader(_NSTFCapPath.get_pcap_file_path(file))
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
        cmd = NSTFCmdTx(buf)
        cmd.index = NSTFProgram.buf_list.add(buf)
        self.fields['commands'].append(cmd)

    def recv(self, bytes):
            self.fields['commands'].append(NSTFCmdRecv(bytes))

    def delay(self, msec):
        self.fields['commands'].append(NSTFCmdDelay(msec))

    def reset(self):
        self.fields['commands'].append(NSTFCmdReset())

    def _set_cmds(self, cmds):
        for cmd in cmds:
            if type(cmd) is NSTFCmdTx:
                cmd.index = NSTFProgram.buf_list.add(cmd.buf)
            self.fields['commands'].append(cmd)

    def _create_cmds_from_cap(self, cmds, init_side):
        new_cmds = []
        origin = init_side
        for cmd in cmds:
            if origin == "c":
                new_cmd = NSTFCmdTx(cmd.payload)
                origin = "s"
            else:
                l = len(cmd.payload)
                new_cmd = NSTFCmdRecv(l - 5)
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


class NSTFIPGenDist(object):
    in_list = []

    @staticmethod
    def class_to_json():
        ret = []
        for i in range(0, len(NSTFIPGenDist.in_list)):
            ret.append(NSTFIPGenDist.in_list[i].to_json())

        return ret

    @staticmethod
    def class_reset():
        NSTFIPGenDist.in_list = []

    class Inner(object):
        def __init__(self, ip_range=["16.0.0.1", "16.0.0.255"], distribution="seq"):
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

        def to_json(self):
            return dict(self.fields)

    def __init__(self, ip_range=["16.0.0.1", "16.0.0.255"], distribution="seq"):
        ver_args = {"types":
                    [{"name": "ip_range", 'arg': ip_range, "t": "ip range", "must": False},
                     ]}
        ArgVerify.verify(self.__class__.__name__, ver_args)
        distribution_vals = ["seq", "rand"]
        if distribution not in distribution_vals:
            raise NSTFError("Distribution must be one of {0}".format(distribution_vals))

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

    def to_json(self):
        return {"index": self.index}


class NSTFIPGenGlobal(object):
    def __init__(self, ip_offset="1.0.0.0"):
        ver_args = {"types":
                    [{"name": "ip_offset", 'arg': ip_offset, "t": "ip address", "must": False},
                     ]}
        ArgVerify.verify(self.__class__.__name__, ver_args)

        self.fields = {}
        self.fields['ip_offset'] = ip_offset

    def to_json(self):
        return dict(self.fields)


class NSTFIPGen(object):
    def __init__(self, glob=NSTFIPGenGlobal(),
                 dist_client=NSTFIPGenDist(["16.0.0.1", "16.0.0.255"], distribution="seq"),
                 dist_server=NSTFIPGenDist(["48.0.0.1", "48.0.255.255"], distribution="seq")):
        ver_args = {"types":
                    [{"name": "glob", 'arg': glob, "t": NSTFIPGenGlobal, "must": False},
                     {"name": "dist_client", 'arg': dist_client, "t": NSTFIPGenDist, "must": False},
                     {"name": "dist_server", 'arg': dist_server, "t": NSTFIPGenDist, "must": False},
                     ]}
        ArgVerify.verify(self.__class__.__name__, ver_args)

        self.fields = {}
        self.fields['global'] = glob
        self.fields['dist_client'] = dist_client
        self.fields['dist_server'] = dist_server

    @staticmethod
    def __str__():
        return "IPGen"

    def to_json(self):
        ret = {}
        for field in self.fields.keys():
            ret[field] = self.fields[field].to_json()

        return ret


# for future use
class NSTFCluster(object):
    def to_json(self):
        return {}


class NSTFTCPInfo(object):
    in_list = []
    DEFAULT_WIN = 32768
    DEFAULT_PORT = 80

    @staticmethod
    def class_reset():
        NSTFTCPInfo.in_list = []

    @staticmethod
    def class_to_json():
        ret = []
        for i in range(0, len(NSTFTCPInfo.in_list)):
            ret.append(NSTFTCPInfo.in_list[i].to_json())

        return ret

    class Inner(object):
        def __init__(self, window=32768, options=0, port=80):
            self.fields = {}
            self.fields['window'] = window
            self.fields['options'] = options
            self.fields['port'] = port

        def __eq__(self, other):
            if self.fields == other.fields:
                return True
            return False

        @property
        def window(self):
            return self.fields['window']

        @property
        def options(self):
            return self.fields['options']

        @property
        def port(self):
            return self.fields['port']

        def to_json(self):
            return dict(self.fields)

    def __init__(self, window=None, options=None, port=None, file=None, side="c"):
        side_vals = ["c", "s"]
        if side not in side_vals:
            raise NSTFError("Side must be one of {0}".format(side_vals))

        new_opt = None
        new_port = None
        new_win = None
        if file is not None:
            cap = CPcapReader(_NSTFCapPath.get_pcap_file_path(file))
            cap.analyze()
            if side == "c":
                new_win = cap.c_tcp_win
                new_opt = 0
                new_port = cap.d_port
            else:
                new_win = cap.s_tcp_win
                new_opt = 0
                # In case of server destination port is the source port received from client
                new_port = None

        # can override parameters from file
        if window is not None:
            new_win = window
        else:
            if new_win is None:
                new_win = NSTFTCPInfo.DEFAULT_WIN
        if options is not None:
            new_opt = options
        else:
            if new_opt is None:
                new_opt = 0
        if port is not None:
            new_port = port
        else:
            if new_port is None:
                new_port = NSTFTCPInfo.DEFAULT_PORT

        new_inner = self.Inner(window=new_win, options=new_opt, port=new_port)
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
        return self.in_list[self.index].port

    def to_json(self):
        return {"index": self.index}

    def dump(self, out, var_name):
        win = self.window
        opt = self.options
        out.write("{0} = {1}(window={2}, options={3})\n".format(self.__class__.__name__, var_name, win, opt))


class NSTFAssociationRule(object):
    def __init__(self, port=80, ip_start=None, ip_end=None):
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


class NSTFAssociation(object):
    def __init__(self, rules=NSTFAssociationRule()):
        ver_args = {"types":
                    [{"name": "rules", 'arg': rules, "t": NSTFAssociationRule, "must": False, "allow_list": True},
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


class _NSTFTemplateBase(object):
    program_list = []
    program_hash = {}

    @staticmethod
    def add_program(program):
        m = program.calc_hash()
        if m in  _NSTFTemplateBase.program_hash:
            return  _NSTFTemplateBase.program_hash[m]
        else:
            _NSTFTemplateBase.program_list.append(program)
            prog_index = len(_NSTFTemplateBase.program_list) - 1
            _NSTFTemplateBase.program_hash[m] = prog_index
            return prog_index

    @staticmethod
    def class_reset():
        _NSTFTemplateBase.program_list = []
        _NSTFTemplateBase.program_hash = {}


    @staticmethod
    def class_to_json():
        ret = []
        for i in range(0, len(_NSTFTemplateBase.program_list)):
            ret.append(_NSTFTemplateBase.program_list[i].to_json())
        return ret

    def __init__(self, program=None):
        self.fields = {}
        self.fields['program_index'] = _NSTFTemplateBase.add_program(program)

    def to_json(self):
        ret = {}
        ret['program_index'] = self.fields['program_index']

        return ret


class _NSTFClientTemplate(_NSTFTemplateBase):
    def __init__(self, ip_gen=NSTFIPGen(), cluster=NSTFCluster(), program=None):
        super(_NSTFClientTemplate, self).__init__(program=program)
        self.fields['ip_gen'] = ip_gen
        self.fields['cluster'] = cluster

    def to_json(self):
        ret = super(_NSTFClientTemplate, self).to_json()
        ret['ip_gen'] = self.fields['ip_gen'].to_json()
        ret['cluster'] = self.fields['cluster'].to_json()
        return ret


class NSTFTCPClientTemplate(_NSTFClientTemplate):
    def __init__(self, ip_gen=NSTFIPGen(), cluster=NSTFCluster(), tcp_info=NSTFTCPInfo(), program=None,
                 port=80, cps=1):
        ver_args = {"types":
                    [{"name": "ip_gen", 'arg': ip_gen, "t": NSTFIPGen, "must": False},
                     {"name": "cluster", 'arg': cluster, "t": NSTFCluster, "must": False},
                     {"name": "tcp_info", 'arg': tcp_info, "t": NSTFTCPInfo, "must": False},
                     {"name": "program", 'arg': program, "t": NSTFProgram}]
                    }
        ArgVerify.verify(self.__class__.__name__, ver_args)

        super(NSTFTCPClientTemplate, self).__init__(ip_gen=ip_gen, cluster=cluster, program=program)
        self.fields['tcp_info'] = tcp_info
        self.fields['port'] = port
        self.fields['cps'] = cps

    def to_json(self):
        ret = super(NSTFTCPClientTemplate, self).to_json()
        ret['tcp_info'] = self.fields['tcp_info'].to_json()
        ret['port'] = self.fields['port']
        ret['cps'] = self.fields['cps']
        return ret


class NSTFTCPServerTemplate(_NSTFTemplateBase):
    def __init__(self, tcp_info=NSTFTCPInfo(), program=None, assoc=NSTFAssociation()):
        ver_args = {"types":
                    [{"name": "tcp_info", 'arg': tcp_info, "t": NSTFTCPInfo, "must": False},
                     {"name": "assoc", 'arg': assoc, "t": [NSTFAssociation, NSTFAssociationRule], "must": False},
                     {"name": "program", 'arg': program, "t": NSTFProgram}]
                    }
        ArgVerify.verify(self.__class__.__name__, ver_args)

        super(NSTFTCPServerTemplate, self).__init__(program=program)
        self.fields['tcp_info'] = tcp_info
        self.fields['assoc'] = assoc

    def to_json(self):
        ret = super(NSTFTCPServerTemplate, self).to_json()
        ret['tcp_info'] = self.fields['tcp_info'].to_json()
        ret['assoc'] = self.fields['assoc'].to_json()
        return ret


class NSTFCapInfo(object):
    def __init__(self, file=None, cps=None, assoc=None, ip_gen=None, port=None, l7_percent=None):
        ver_args = {"types":
                    [{"name": "file", 'arg': file, "t": str},
                     {"name": "assoc", 'arg': assoc, "t": [NSTFAssociation, NSTFAssociationRule], "must": False},
                     {"name": "ip_gen", 'arg': ip_gen, "t": NSTFIPGen, "must": False}]
                    }
        ArgVerify.verify(self.__class__.__name__, ver_args)

        if l7_percent is not None:
            if cps is not None:
                raise NSTFErrorBadParamCombination(self.__class__.__name__, "cps", "l7_percent")
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
            if type(assoc) is NSTFAssociationRule:
                self.assoc = NSTFAssociation(assoc)
            else:
                self.assoc = assoc
        else:
            if port is not None:
                self.assoc = NSTFAssociation(NSTFAssociationRule(port=port))
            else:
                self.assoc = None
        self.ip_gen = ip_gen


class NSTFTemplate(object):
    def __init__(self, client_template=None, server_template=None):
        ver_args = {"types":
                    [{"name": "client_template", 'arg': client_template, "t": NSTFTCPClientTemplate},
                     {"name": "server_template", 'arg': server_template, "t": NSTFTCPServerTemplate}]
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


# For future use
class NSTFGlobal(object):
    pass


class NSTFProfile(object):
    def __init__(self, templates=None, glob=None, cap_list=None, default_ip_gen=NSTFIPGen()):
        ver_args = {"types":
                    [{"name": "templates", 'arg': templates, "t": NSTFTemplate, "allow_list": True, "must": False},
                     {"name": "glob", 'arg': glob, "t": NSTFGlobal, "must": False},
                     {"name": "cap_list", 'arg': cap_list, "t": NSTFCapInfo, "allow_list": True, "must": False},
                     {"name": "default_ip_gen", 'arg': default_ip_gen, "t": NSTFIPGen, "must": False}]
                    }
        ArgVerify.verify(self.__class__.__name__, ver_args)

        if cap_list is not None:
            if templates is not None:
                raise NSTFErrorBadParamCombination(self.__class__.__name__, "templates", "cap_list")
            self.cap_list = listify(cap_list)
            self.templates = []
        else:
            if templates is None:
                raise NSTFErrorMissingParam(self.__class__.__name__, "templates", "cap_list")
            self.templates = listify(templates)

        if cap_list is not None:
            mode = None
            all_cap_info = []
            d_ports = []
            total_payload = 0
            for cap in self.cap_list:
                cap_file = cap.file
                ip_gen = cap.ip_gen if cap.ip_gen is not None else default_ip_gen
                prog_c = NSTFProgram(file=cap_file, side="c")
                prog_s = NSTFProgram(file=cap_file, side="s")
                tcp_c = NSTFTCPInfo(file=cap_file, side="c")
                tcp_s = NSTFTCPInfo(file=cap_file, side="s")
                cps = cap.cps
                l7_percent = cap.l7_percent
                if mode is None:
                    if l7_percent is not None:
                        mode = "l7_percent"
                    else:
                        mode = "cps"
                else:
                    if mode == "l7_percent" and l7_percent is None:
                        raise NSTFError("If one cap specifies l7_percent, then all should specify it")
                    if mode is "cps" and l7_percent is not None:
                        raise NSTFError("Can't mix specifications of cps and l7_percent in same cap list")

                total_payload += prog_c.payload_len
                if cap.assoc is None:
                    d_port = tcp_c.port
                    my_assoc = NSTFAssociation(rules=NSTFAssociationRule(port=d_port))
                else:
                    d_port = cap.assoc.port
                    my_assoc = cap.assoc
                if d_port in d_ports:
                    raise NSTFError("More than one cap use dest port {0}. This is currently not supported.".format(d_port))
                d_ports.append(d_port)

                all_cap_info.append({"ip_gen": ip_gen, "prog_c": prog_c, "prog_s": prog_s, "tcp_c": tcp_c, "tcp_s": tcp_s,
                                     "cps": cps, "d_port": d_port, "my_assoc": my_assoc})

            # calculate cps from l7 percent
            if mode == "l7_percent":
                percent_sum = 0
                for c in all_cap_info:
                    c["cps"] = c["prog_c"].payload_len * 100.0 / total_payload
                    percent_sum += c["cps"]
                if percent_sum != 100:
                    raise NSTFError("l7_percent values must sum up to 100")

            for c in all_cap_info:
                temp_c = NSTFTCPClientTemplate(program=c["prog_c"], tcp_info=c["tcp_c"], ip_gen=c["ip_gen"], port=c["d_port"],
                                               cps=c["cps"])
                temp_s = NSTFTCPServerTemplate(program=c["prog_s"], tcp_info=c["tcp_s"], assoc=c["my_assoc"])
                template = NSTFTemplate(client_template=temp_c, server_template=temp_s)
                self.templates.append(template)

    def to_json(self):
        ret = {}
        ret['buf_list'] = NSTFProgram.class_to_json()
        ret['tcp_info_list'] = NSTFTCPInfo.class_to_json()
        ret['ip_gen_dist_list'] = NSTFIPGenDist.class_to_json()
        ret['program_list'] = _NSTFTemplateBase.class_to_json()
        ret['templates'] = []
        for i in range(0, len(self.templates)):
            ret['templates'].append(self.templates[i].to_json())

        return json.dumps(ret, indent=4, separators=(',', ': '))


if __name__ == '__main__':
    cmd_list = []
    cmd_list.append(NSTFCmdTx("aaaaaaaaaaaaaa"))
    cmd_list.append(NSTFCmdTx("bbbbbbbb"))
    cmd_list.append(NSTFCmdTx("aaaaaaaaaaaaaa"))
    cmd_list.append(NSTFCmdReset())
    my_prog_c = NSTFProgram(commands=cmd_list)
    my_prog_s = NSTFProgram(file="/Users/ibarnea/src/trex-core/scripts/cap2/http_get.pcap", side="s")
    ip_gen_c = NSTFIPGenDist(ip_range=["17.0.0.0", "17.255.255.255"], distribution="seq")
    ip_gen_c2 = NSTFIPGenDist(ip_range=["17.0.0.0", "17.255.255.255"], distribution="seq")
    ip_gen_s = NSTFIPGenDist(ip_range=["49.0.0.0", "49.255.255.255"], distribution="seq")
    ip_gen_s2 = NSTFIPGenDist(ip_range=["200.0.0.0", "200.255.255.255"], distribution="seq")
    ip_gen = NSTFIPGen(glob=NSTFIPGenGlobal(ip_offset="1.0.0.0"),
                       dist_client=ip_gen_c,
                       dist_server=ip_gen_s)
    ip_gen2 = NSTFIPGen(glob=NSTFIPGenGlobal(ip_offset="1.0.0.0"),
                        dist_client=ip_gen_c2,
                        dist_server=ip_gen_s2)

    tcp1 = NSTFTCPInfo(file="../../../../cap2/http_get.pcap", side="c")
    tcp2 = NSTFTCPInfo(window=32768, options=0x2)

    assoc_rule1 = NSTFAssociationRule(port=81, ip_start="1.1.1.1", ip_end="1.1.1.10", ip_type="dst")
    assoc_rule2 = NSTFAssociationRule(port=90, ip_start="2.1.1.1", ip_end="2.1.1.10", ip_type="src")
    assoc = NSTFAssociation(rules=[assoc_rule1, assoc_rule2])

    temp_c = NSTFTCPClientTemplate(program=my_prog_c, tcp_info=tcp1, ip_gen=ip_gen)
    temp_s = NSTFTCPServerTemplate(program=my_prog_s, tcp_info=tcp2, assoc=assoc)
    template = NSTFTemplate(client_template=temp_c, server_template=temp_s)

    profile = NSTFProfile(template)

    f = open('/tmp/tcp_out.json', 'w')
    print(profile.to_json())
    f.write(str(profile.to_json()).replace("'", "\""))
