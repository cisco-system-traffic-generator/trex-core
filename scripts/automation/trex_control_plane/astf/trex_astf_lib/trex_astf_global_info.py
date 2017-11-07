import socket          


class ASTFGlobalInfoBase(object):
    _g_params = {}

    class inner(object):
        def __init__(self, params, name):
            self._fields = {}
            self._params = params
            self._name = name


        def __setattr__(self, name, val):
            if name.startswith("_"):
                return super(ASTFGlobalInfoBase.inner, self).__setattr__(name, val)
            for p in self._params:
                if name == p["name"]:
                    if "sub_type" in p:
                        if p["sub_type"]=="ipv6_addr":
                            if (type(val)!=str):
                                raise AttributeError("{0} in {1} should have one of the following types: {2}"
                                                     .format(name, self._name, str))
                            b=socket.inet_pton(socket.AF_INET6, val)
                            l = list(b);
                            # in case of Python 2
                            if not(type(l[0]) is int):
                                l=[ord(i) for i in l]
                            self._fields[name] = l;
                            return;

                    if "type" in p and type(val) not in p["type"]:
                        raise AttributeError("{0} in {1} should have one of the following types: {2}"
                                             .format(name, self._name, p["type"]))

                    self._fields[name] = val
                    return

            raise AttributeError("%r has no attribute %s" % (self._name, name))

        def __getattr__(self, name):
            if name.startswith("_"):
                return super(ASTFGlobalInfoBase.inner, self).__getattr__(name)
            for p in self._params:
                if name == p["name"]:
                    return self._fields[name]

            raise AttributeError("%r has no attribute %s" % (self._name, name))

        def to_json(self):
            return self._fields

    def __init__(self, params=_g_params, name="globalp"):
        self._fields = {}
        self._params = params
        self._name = name

    def __setattr__(self, name, val):
        if name.startswith("_"):
            return super(ASTFGlobalInfoBase, self).__setattr__(name, val)

        if name in self._params:
            if type(self._params[name]) is dict:
                next_level_params = self._params[name].keys()
            else:
                next_level_params = []
                for n in self._params[name]:
                    next_level_params.append(n["name"])

            raise AttributeError("{0} in {1} should be followed by one of {2}".format(name, self._name, next_level_params))

    def __getattr__(self, name):
        if name.startswith("_"):
            return super(ASTFGlobalInfoBase.in_tcp, self).__getattr__(name)

        if name in self._params:
            long_name = self._name + "." + name
            if type(self._params[name]) is dict:
                return self._fields.setdefault(name, ASTFGlobalInfoBase(params=self._params[name], name=long_name))
            elif type(self._params[name]) is list:
                return self._fields.setdefault(name, ASTFGlobalInfoBase.inner(params=self._params[name], name=long_name))

        raise AttributeError("{0} has no attribute {1} it has {2}".format(self._name, name, self._params.keys()))

    def to_json(self):
        ret = {}
        for field in self._fields.keys():
            ret[field] = self._fields[field].to_json()

        return ret


class ASTFGlobalInfo(ASTFGlobalInfoBase):
    _g_params = {
        "ipv6": [
            {"name": "src_msb", "sub_type" : "ipv6_addr" },
            {"name": "dst_msb", "sub_type" : "ipv6_addr" },
            {"name": "enable", "type": [int]}
        ],

        "tcp": [
                {"name": "mss", "type": [int]},
                {"name": "initwnd", "type": [int]},
                {"name": "rxbufsize", "type": [int]},
                {"name": "txbufsize", "type": [int]},
                {"name": "rexmtthresh", "type": [int]},
                {"name": "do_rfc1323", "type": [int]},
                {"name": "keepinit", "type": [int]},
                {"name": "keepidle", "type": [int]},
                {"name": "keepintvl", "type": [int]},
                {"name": "delay_ack_msec", "type": [int]}
            ],
        "ip": [
            {"name": "tos", "type": [int]},
            {"name": "ttl", "type": [int]}
        ],
    }

    def __init__(self, params=_g_params, name="GlobalInfo"):
        return super(ASTFGlobalInfo, self).__init__(params, name)


class ASTFGlobalInfoPerTemplate(ASTFGlobalInfoBase):
    _g_params = {
        "tcp": [
                {"name": "window", "type": [int]},
                {"name": "mss", "type": [int]}
            ],
        "ip": [
            {"name": "tos", "type": [int]},
            {"name": "ttl", "type": [int]}
        ],
    }

    def __init__(self, params=_g_params, name="GlobalInfoPerTemplate"):
        return super(ASTFGlobalInfoPerTemplate, self).__init__(params, name)
