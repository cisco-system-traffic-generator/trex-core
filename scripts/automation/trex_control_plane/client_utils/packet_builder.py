#!/router/bin/python


import outer_packages
import dpkt
import socket
import binascii


class CTRexPktBuilder(object):
    """docstring for CTRexPktBuilder"""
    def __init__(self):
        super(CTRexPktBuilder, self).__init__()
        self.packet = None
        self.vm = CTRexPktBuilder.CTRexVM(self.packet)

    def add_l2_header(self):
        pass

    def add_l3_header(self):
        pass

    def add_pkt_payload(self):
        pass

    # VM access methods
    def set_vm_ip_range(self, ip_start, ip_end, ip_type="ipv4"):
        pass

    def set_vm_range_type(self, ip_type):
        pass

    def set_vm_core_mask(self, ip_type):
        pass

    def get_vm_data(self):
        pass

    def dump_pkt(self):
        pkt_in_hex = binascii.hexlify(str(self.packet))
        return [pkt_in_hex[i:i+2] for i in range(0, len(pkt_in_hex), 2)]

    # ----- useful shortcut methods ----- #
    def gen_dns_packet(self):
        pass

    # ----- internal methods ----- #
    @staticmethod
    def _is_valid_ip_addr(ip_addr, ip_type):
        if ip_type == "ipv4":
            try:
                socket.inet_pton(socket.AF_INET, ip_addr)
            except AttributeError:  # no inet_pton here, sorry
                try:
                    socket.inet_aton(ip_addr)
                except socket.error:
                    return False
                return ip_addr.count('.') == 3
            except socket.error:  # not a valid address
                return False
            return True
        elif ip_type == "ipv6":
            try:
                socket.inet_pton(socket.AF_INET6, ip_addr)
            except socket.error:  # not a valid address
                return False
            return True
        else:
            raise CTRexPktBuilder.IPAddressError()

    # ------ private classes ------
    class CTRexVM(object):
        """docstring for CTRexVM"""
        def __init__(self, packet):
            super(CTRexPktBuilder.CTRexVM, self).__init__()
            self.packet = packet
            self.vm_variables = {}

        def add_vm_variable(self, name):
            if name not in self.vm_variables.keys():
                self.vm_variables[name] = self.CTRexVMVariable(name)
            else:
                raise CTRexPktBuilder.VMVarNameExistsError(name)

        def set_vm_var_field(self, var_name, field_name, val):
            pass
            return self.vm_variables[var_name].set_field(field_name, val)


        def fix_checksum_ipv4(self):
            pass

        def flow_man_simple(self):
            pass

        def write_to_pkt(self):
            pass

        def dump(self):
            return [var.dump()
                    for var in self.vm_variables]

        class CTRexVMVariable(object):
            VALID_SIZE = [1, 2, 4, 8]
            VALID_TYPE = ["inc", "dec", "random"]
            VALID_CORE_MASK = ["split", "none"]

            def __init__(self, name):
                super(CTRexPktBuilder.CTRexVM.CTRexVMVariable, self).__init__()
                self.name = name
                self.size = 4
                self.big_endian = True
                self.type = "inc"
                self.core_mask = "none"
                self.init_addr = "10.0.0.1"
                self.min_addr = str(self.init_addr)
                self.max_addr = str(self.init_addr)

            def set_field(self, field_name, val):
                if field_name == "size":
                    if type(val) != int:
                        raise CTRexPktBuilder.VMVarFieldTypeError("size", int)
                    elif val not in self.VALID_SIZE:
                        raise CTRexPktBuilder.VMVarValueError("size", self.VALID_SIZE)
                elif field_name == "type":
                    if type(val) != str:
                        raise CTRexPktBuilder.VMVarFieldTypeError("type", str)
                    elif val not in self.VALID_TYPE:
                        raise CTRexPktBuilder.VMVarValueError("type", self.VALID_TYPE)
                elif field_name == "core_mask":
                    if type(val) != str:
                        raise CTRexPktBuilder.VMVarFieldTypeError("core_mask", str)
                    elif val not in self.VALID_TYPE:
                        raise CTRexPktBuilder.VMVarValueError("core_mask", self.VALID_CORE_MASK)
                # update field value on success
                setattr(self, field_name, val)

            def is_valid(self):
                if self.size not in self.VALID_SIZE:
                    return False
                if self.type not in self.VALID_TYPE:
                    return False
                if self.core_mask not in self.VALID_CORE_MASK:
                    return False
                return True

            def dump(self):
                return {"name" : self.name,
                        "Size" : self.size,
                        "big_endian" : self.big_endian,
                        "type" : self.type,
                        "core_mask" : self.core_mask,
                        "init_addr" : self.init_addr,
                        "min_addr" : self.min_addr,
                        "max_addr" : self.max_addr}

    class CPacketBuildException(Exception):
        """
        This is the general Packet error exception class.
        """
        def __init__(self, code, message):
            self.code = code
            self.message = message

        def __str__(self):
            return self.__repr__()

        def __repr__(self):
            return u"[errcode:%r] %r" % (self.code, self.message)

    class IPAddressError(CPacketBuildException):
        """
        This exception is used to indicate an error on the IP addressing part of the packet.
        """
        def __init__(self, message=''):
            self._default_message = 'Illegal type of IP addressing has been provided.'
            self.message = message or self._default_message
            super(CTRexPktBuilder.IPAddressError, self).__init__(-11, self.message)

    class VMVarNameExistsError(CPacketBuildException):
        """
        This exception is used to indicate an error on the IP addressing part of the packet.
        """
        def __init__(self, name, message=''):
            self._default_message = 'The given VM name ({0})already exists as part of the stream.'.format(name)
            self.message = message or self._default_message
            super(CTRexPktBuilder.VMVarNameExistsError, self).__init__(-21, self.message)

    class VMVarFieldTypeError(CPacketBuildException):
        """
        This exception is used to indicate an error on the IP addressing part of the packet.
        """
        def __init__(self, name, ok_type, message=''):
            self._default_message = 'The desired value of field {field_name} is of type {field_type}, \
            and not of the allowed {allowed_type}.'.format(field_name=name,
                                                           field_type=type(name).__name__,
                                                           allowed_type=ok_type.__name__)
            self.message = message or self._default_message
            super(CTRexPktBuilder.VMVarNameExistsError, self).__init__(-31, self.message)

    class VMVarValueError(CPacketBuildException):
        """
        This exception is used to indicate an error on the IP addressing part of the packet.
        """
        def __init__(self, name, ok_opts, message=''):
            self._default_message = 'The desired value of field {field_name} is illegal.\n \
            The only allowed options are: {allowed_opts}.'.format(field_name=name,
                                                                  allowed_opts=ok_opts)
            self.message = message or self._default_message
            super(CTRexPktBuilder.VMVarValueError, self).__init__(-32, self.message)


if __name__ == "__main__":
    pass
