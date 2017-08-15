from cap_handling import CPcapReader
from arg_verify import ArgVerify
import os
import sys
import inspect
from trex_astf_exceptions import ASTFError, ASTFErrorBadParamCombination, ASTFErrorMissingParam
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
        ASTFProgram.buf_list = ASTFProgram.BufferList()

    @staticmethod
    def class_to_json():
        return ASTFProgram.buf_list.to_json()

    def __init__(self, file=None, side="c", commands=None):
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
        cmd.index = ASTFProgram.buf_list.add(cmd.buf)
        self.fields['commands'].append(cmd)

    def recv(self, bytes):
        ver_args = {"types":
                     [{"name": "bytes", 'arg': bytes, "t": int}]
                    }
        ArgVerify.verify(self.__class__.__name__ + "." + sys._getframe().f_code.co_name, ver_args)

        self.fields['commands'].append(ASTFCmdRecv(bytes))

    def delay(self, msec):
        ver_args = {"types":
                    [{"name": "msec", 'arg': msec, "t": [int, float]}]
        }
        ArgVerify.verify(self.__class__.__name__ + "." + sys._getframe().f_code.co_name, ver_args)

        self.fields['commands'].append(ASTFCmdDelay(msec))

    def reset(self):
        self.fields['commands'].append(ASTFCmdReset())

    def _set_cmds(self, cmds):
        for cmd in cmds:
            if type(cmd) is ASTFCmdSend:
                cmd.index = ASTFProgram.buf_list.add(cmd.buf)
            self.fields['commands'].append(cmd)

    def _create_cmds_from_cap(self, cmds, init_side):
        new_cmds = []
        origin = init_side
        for cmd in cmds:
            if origin == "c":
                new_cmd = ASTFCmdSend(cmd.payload)
                origin = "s"
            else:
                l = len(cmd.payload)
                new_cmd = ASTFCmdRecv(l - 5)
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

    def to_json(self):
        return {"index": self.index}


class ASTFIPGenGlobal(object):
    def __init__(self, ip_offset="1.0.0.0"):
        ver_args = {"types":
                    [{"name": "ip_offset", 'arg': ip_offset, "t": "ip address", "must": False},
                     ]}
        ArgVerify.verify(self.__class__.__name__, ver_args)

        self.fields = {}
        self.fields['ip_offset'] = ip_offset

    def to_json(self):
        return dict(self.fields)


class ASTFIPGen(object):
    def __init__(self, glob=ASTFIPGenGlobal(),
                 dist_client=ASTFIPGenDist(["16.0.0.1", "16.0.0.255"], distribution="seq"),
                 dist_server=ASTFIPGenDist(["48.0.0.1", "48.0.255.255"], distribution="seq")):
        ver_args = {"types":
                    [{"name": "glob", 'arg': glob, "t": ASTFIPGenGlobal, "must": False},
                     {"name": "dist_client", 'arg': dist_client, "t": ASTFIPGenDist, "must": False},
                     {"name": "dist_server", 'arg': dist_server, "t": ASTFIPGenDist, "must": False},
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
class ASTFCluster(object):
    def to_json(self):
        return {}


class ASTFTCPInfo(object):
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
            raise ASTFError("Side must be one of {0}".format(side_vals))

        new_opt = None
        new_port = None
        new_win = None
        if file is not None:
            cap = CPcapReader(_ASTFCapPath.get_pcap_file_path(file))
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
                new_win = ASTFTCPInfo.DEFAULT_WIN
        if options is not None:
            new_opt = options
        else:
            if new_opt is None:
                new_opt = 0
        if port is not None:
            new_port = port
        else:
            if new_port is None:
                new_port = ASTFTCPInfo.DEFAULT_PORT

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


class ASTFAssociationRule(object):
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


class ASTFAssociation(object):
    def __init__(self, rules=ASTFAssociationRule()):
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
    def __init__(self, ip_gen=ASTFIPGen(), cluster=ASTFCluster(), program=None):
        super(_ASTFClientTemplate, self).__init__(program=program)
        self.fields['ip_gen'] = ip_gen
        self.fields['cluster'] = cluster

    def to_json(self):
        ret = super(_ASTFClientTemplate, self).to_json()
        ret['ip_gen'] = self.fields['ip_gen'].to_json()
        ret['cluster'] = self.fields['cluster'].to_json()
        return ret


class ASTFTCPClientTemplate(_ASTFClientTemplate):
    def __init__(self, ip_gen=ASTFIPGen(), cluster=ASTFCluster(), tcp_info=ASTFTCPInfo(), program=None,
                 port=80, cps=1):
        ver_args = {"types":
                    [{"name": "ip_gen", 'arg': ip_gen, "t": ASTFIPGen, "must": False},
                     {"name": "cluster", 'arg': cluster, "t": ASTFCluster, "must": False},
                     {"name": "tcp_info", 'arg': tcp_info, "t": ASTFTCPInfo, "must": False},
                     {"name": "program", 'arg': program, "t": ASTFProgram}]
                    }
        ArgVerify.verify(self.__class__.__name__, ver_args)

        super(ASTFTCPClientTemplate, self).__init__(ip_gen=ip_gen, cluster=cluster, program=program)
        self.fields['tcp_info'] = tcp_info
        self.fields['port'] = port
        self.fields['cps'] = cps

    def to_json(self):
        ret = super(ASTFTCPClientTemplate, self).to_json()
        ret['tcp_info'] = self.fields['tcp_info'].to_json()
        ret['port'] = self.fields['port']
        ret['cps'] = self.fields['cps']
        return ret


class ASTFTCPServerTemplate(_ASTFTemplateBase):
    def __init__(self, tcp_info=ASTFTCPInfo(), program=None, assoc=ASTFAssociation()):
        ver_args = {"types":
                    [{"name": "tcp_info", 'arg': tcp_info, "t": ASTFTCPInfo, "must": False},
                     {"name": "assoc", 'arg': assoc, "t": [ASTFAssociation, ASTFAssociationRule], "must": False},
                     {"name": "program", 'arg': program, "t": ASTFProgram}]
                    }
        ArgVerify.verify(self.__class__.__name__, ver_args)

        super(ASTFTCPServerTemplate, self).__init__(program=program)
        self.fields['tcp_info'] = tcp_info
        self.fields['assoc'] = assoc

    def to_json(self):
        ret = super(ASTFTCPServerTemplate, self).to_json()
        ret['tcp_info'] = self.fields['tcp_info'].to_json()
        ret['assoc'] = self.fields['assoc'].to_json()
        return ret


class ASTFCapInfo(object):
    def __init__(self, file=None, cps=None, assoc=None, ip_gen=None, port=None, l7_percent=None):
        ver_args = {"types":
                    [{"name": "file", 'arg': file, "t": str},
                     {"name": "assoc", 'arg': assoc, "t": [ASTFAssociation, ASTFAssociationRule], "must": False},
                     {"name": "ip_gen", 'arg': ip_gen, "t": ASTFIPGen, "must": False}]
                    }
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


class ASTFTemplate(object):
    def __init__(self, client_template=None, server_template=None):
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


# For future use
class ASTFGlobal(object):
    pass


class ASTFProfile(object):
    def __init__(self, templates=None, glob=None, cap_list=None, default_ip_gen=ASTFIPGen()):
        ver_args = {"types":
                    [{"name": "templates", 'arg': templates, "t": ASTFTemplate, "allow_list": True, "must": False},
                     {"name": "glob", 'arg': glob, "t": ASTFGlobal, "must": False},
                     {"name": "cap_list", 'arg': cap_list, "t": ASTFCapInfo, "allow_list": True, "must": False},
                     {"name": "default_ip_gen", 'arg': default_ip_gen, "t": ASTFIPGen, "must": False}]
                    }
        ArgVerify.verify(self.__class__.__name__, ver_args)

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
                prog_c = ASTFProgram(file=cap_file, side="c")
                prog_s = ASTFProgram(file=cap_file, side="s")
                tcp_c = ASTFTCPInfo(file=cap_file, side="c")
                tcp_s = ASTFTCPInfo(file=cap_file, side="s")
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
                    d_port = tcp_c.port
                    my_assoc = ASTFAssociation(rules=ASTFAssociationRule(port=d_port))
                else:
                    d_port = cap.assoc.port
                    my_assoc = cap.assoc
                if d_port in d_ports:
                    raise ASTFError("More than one cap use dest port {0}. This is currently not supported.".format(d_port))
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
                    raise ASTFError("l7_percent values must sum up to 100")

            for c in all_cap_info:
                temp_c = ASTFTCPClientTemplate(program=c["prog_c"], tcp_info=c["tcp_c"], ip_gen=c["ip_gen"], port=c["d_port"],
                                               cps=c["cps"])
                temp_s = ASTFTCPServerTemplate(program=c["prog_s"], tcp_info=c["tcp_s"], assoc=c["my_assoc"])
                template = ASTFTemplate(client_template=temp_c, server_template=temp_s)
                self.templates.append(template)

    def to_json(self):
        ret = {}
        ret['buf_list'] = ASTFProgram.class_to_json()
        ret['tcp_info_list'] = ASTFTCPInfo.class_to_json()
        ret['ip_gen_dist_list'] = ASTFIPGenDist.class_to_json()
        ret['program_list'] = _ASTFTemplateBase.class_to_json()
        ret['templates'] = []
        for i in range(0, len(self.templates)):
            ret['templates'].append(self.templates[i].to_json())

        return json.dumps(ret, indent=4, separators=(',', ': '))


if __name__ == '__main__':
    cmd_list = []
    cmd_list.append(ASTFCmdSend("aaaaaaaaaaaaaa"))
    cmd_list.append(ASTFCmdSend("bbbbbbbb"))
    cmd_list.append(ASTFCmdSend("aaaaaaaaaaaaaa"))
    cmd_list.append(ASTFCmdReset())
    my_prog_c = ASTFProgram(commands=cmd_list)
    my_prog_s = ASTFProgram(file="/Users/ibarnea/src/trex-core/scripts/cap2/http_get.pcap", side="s")
    ip_gen_c = ASTFIPGenDist(ip_range=["17.0.0.0", "17.255.255.255"], distribution="seq")
    ip_gen_c2 = ASTFIPGenDist(ip_range=["17.0.0.0", "17.255.255.255"], distribution="seq")
    ip_gen_s = ASTFIPGenDist(ip_range=["49.0.0.0", "49.255.255.255"], distribution="seq")
    ip_gen_s2 = ASTFIPGenDist(ip_range=["200.0.0.0", "200.255.255.255"], distribution="seq")
    ip_gen = ASTFIPGen(glob=ASTFIPGenGlobal(ip_offset="1.0.0.0"),
                       dist_client=ip_gen_c,
                       dist_server=ip_gen_s)
    ip_gen2 = ASTFIPGen(glob=ASTFIPGenGlobal(ip_offset="1.0.0.0"),
                        dist_client=ip_gen_c2,
                        dist_server=ip_gen_s2)

    tcp1 = ASTFTCPInfo(file="../../../../cap2/http_get.pcap", side="c")
    tcp2 = ASTFTCPInfo(window=32768, options=0x2)

    assoc_rule1 = ASTFAssociationRule(port=81, ip_start="1.1.1.1", ip_end="1.1.1.10", ip_type="dst")
    assoc_rule2 = ASTFAssociationRule(port=90, ip_start="2.1.1.1", ip_end="2.1.1.10", ip_type="src")
    assoc = ASTFAssociation(rules=[assoc_rule1, assoc_rule2])

    temp_c = ASTFTCPClientTemplate(program=my_prog_c, tcp_info=tcp1, ip_gen=ip_gen)
    temp_s = ASTFTCPServerTemplate(program=my_prog_s, tcp_info=tcp2, assoc=assoc)
    template = ASTFTemplate(client_template=temp_c, server_template=temp_s)

    profile = ASTFProfile(template)

    f = open('/tmp/tcp_out.json', 'w')
    print(profile.to_json())
    f.write(str(profile.to_json()).replace("'", "\""))
