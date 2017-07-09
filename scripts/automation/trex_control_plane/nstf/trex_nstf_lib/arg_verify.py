from trex_nstf_exceptions import NSTFErrorWrongType, NSTFErrorMissingParam, NSTFErrorBadIp, NSTFErrorBadIpRange
import socket


class ArgVerify(object):

    @staticmethod
    def verify_ip(ip):
        try:
            socket.inet_aton(ip)
        except Exception:
            return False
        return True

    @staticmethod
    def verify_ip_range(ip_range):
        if len(ip_range) != 2:
            return "Range should contain two IPs"
        if not ArgVerify.verify_ip(ip_range[0]):
            return "Bad first IP"
        if not ArgVerify.verify_ip(ip_range[1]):
            return "Bad second IP"

        return "ok"

    @staticmethod
    def verify(f_name, d):
        arg_types = d["types"]
        for arg in arg_types:
            name = arg["name"]
            given_arg = arg["arg"]
            if isinstance(arg["t"], list):
                needed_type = arg["t"]
            else:
                needed_type = [arg["t"]]
            if "allow_list" in arg:
                allow_list = arg["allow_list"]
            else:
                allow_list = False
            if "must" in arg:
                must = arg["must"]
            else:
                must = True
            if given_arg is None:
                if must:
                    raise NSTFErrorMissingParam(f_name, name)
                else:
                    continue
            if allow_list and isinstance(given_arg, list):
                given_arg = given_arg[0]
            type_ok = False
            for one_type in needed_type:
                if one_type == "ip address":
                    if ArgVerify.verify_ip(given_arg):
                        type_ok = True
                    else:
                        raise NSTFErrorBadIp(f_name, name, given_arg)
                elif one_type == "ip range":
                    ret = ArgVerify.verify_ip_range(given_arg)
                    if ret == "ok":
                        type_ok = True
                    else:
                        raise NSTFErrorBadIpRange(f_name, name, given_arg, ret)
                else:
                    if isinstance(given_arg, one_type):
                        type_ok = True
            if not type_ok:
                raise NSTFErrorWrongType(f_name, name, needed_type, allow_list)
