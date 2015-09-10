#!/router/bin/python


import outer_packages
import dpkt
import socket
import binascii
import copy
import random
import string
import struct
import re


class CTRexPktBuilder(object):
    """
    This class defines the TRex API of building a packet using dpkt package.
    Using this class the user can also define how TRex will handle the packet by specifying the VM setting.
    """
    def __init__(self, max_pkt_size=dpkt.ethernet.ETH_LEN_MAX):
        """
        Instantiate a CTRexPktBuilder object

        :parameters:
             None

        """
        super(CTRexPktBuilder, self).__init__()
        self._packet = None
        self._pkt_by_hdr = {}
        self._pkt_top_layer = None
        self._max_pkt_size = max_pkt_size
        self.payload_generator = CTRexPktBuilder.CTRexPayloadGen(self._packet, self._max_pkt_size)
        self.vm = CTRexPktBuilder.CTRexVM()

    def add_pkt_layer(self, layer_name, pkt_layer):
        """
        This method adds additional header to the already existing packet

        :parameters:
            layer_name: str
                a string representing the name of the layer.
                Example: "l2", "l4_tcp", etc.

            pkt_layer : dpkt.Packet obj
                a dpkt object, generally from higher layer, that will be added on top of existing layer.

        """
        assert isinstance(pkt_layer, dpkt.Packet)
        if layer_name in self._pkt_by_hdr:
            raise ValueError("Given layer name '{0}' already exists.".format(layer_name))
        else:
            dup_pkt = copy.copy(pkt_layer)  # using copy of layer to avoid cyclic packets that may lead to infinite loop
            if not self._pkt_top_layer:     # this is the first header added
                self._packet = dup_pkt
            else:
                self._pkt_top_layer.data = dup_pkt
            self._pkt_top_layer = dup_pkt
            self._pkt_by_hdr[layer_name] = dup_pkt
            return

    def set_ip_layer_addr(self, layer_name, attr, ip_addr, ip_type="ipv4"):
        try:
            layer = self._pkt_by_hdr[layer_name.lower()]
            if not (isinstance(layer, dpkt.ip.IP) or isinstance(layer, dpkt.ip6.IP6)):
                raise ValueError("The specified layer '{0}' is not of IPv4/IPv6 type.".format(layer_name))
            else:
                decoded_ip = CTRexPktBuilder._decode_ip_addr(ip_addr, ip_type)
                setattr(layer, attr, decoded_ip)
        except KeyError:
            raise KeyError("Specified layer '{0}' doesn't exist on packet.".format(layer_name))

    def set_ipv6_layer_addr(self, layer_name, attr, ip_addr):
        self.set_ip_layer_addr(self, layer_name, attr, ip_addr, ip_type="ipv6")

    def set_eth_layer_addr(self, layer_name, attr, mac_addr):
        try:
            layer = self._pkt_by_hdr[layer_name.lower()]
            if not isinstance(layer, dpkt.ethernet.Ethernet):
                raise ValueError("The specified layer '{0}' is not of Ethernet type.".format(layer_name))
            else:
                decoded_mac = CTRexPktBuilder._decode_mac_addr(mac_addr)
                setattr(layer, attr, decoded_mac)
        except KeyError:
            raise KeyError("Specified layer '{0}' doesn't exist on packet.".format(layer_name))

    def set_layer_attr(self, layer_name, attr, val):
        """
        This method enables the user to change a value of a previously defined packet layer.
        This method isn't to be used to set the data attribute of a packet with payload.
        Use :func:`packet_builder.CTRexPktBuilder.set_payload` instead.

        :parameters:
            layer_name: str
                a string representing the name of the layer.
                Example: "l2", "l4_tcp", etc.

            attr : str
                a string representing the attribute to be changed on desired layer

            val :
                value of attribute.

        :raises:
            + :exc:`KeyError`, in case of missing layer (the desired layer isn't part of packet)
            + :exc:`ValueError`, in case invalid attribute to the specified layer.

        """
        try:
            layer = self._pkt_by_hdr[layer_name.lower()]
            if attr == 'data' and not isinstance(val, dpkt.Packet):
                # Don't allow setting 'data' attribute
                raise ValueError("Set a data attribute with obejct that is not dpkt.Packet is not allowed using "
                                 "set_layer_attr method.\nUse set_payload method instead.")
            if hasattr(layer, attr):
                setattr(layer, attr, val)
                if attr == 'data':
                    # re-evaluate packet from the start, possible broken link between layers
                    self._reevaluate_packet(layer_name.lower())
            else:
                raise ValueError("Given attr name '{0}' doesn't exists on specified layer ({1}).".format(layer_name,
                                                                                                         attr))
        except KeyError:
            raise KeyError("Specified layer '{0}' doesn't exist on packet.".format(layer_name))

    def set_pkt_payload(self, payload):
        """
        This method sets a payload to the topmost layer of the generated packet.
        This method isn't to be used to set another networking layer to the packet.
        Use :func:`packet_builder.CTRexPktBuilder.set_layer_attr` instead.


        :parameters:
            payload:
                a payload to be added to the packet at the topmost layer.
                this object cannot be of type dpkt.Packet.

        :raises:
            + :exc:`AttributeError`, in case no underlying header to host the payload.

        """
        assert isinstance(payload, str)
        try:
            self._pkt_top_layer.data = payload
        except AttributeError:
            raise AttributeError("The so far built packet doesn't contain an option for payload attachment.\n"
                                 "Make sure to set appropriate underlying header before adding payload")

    def load_packet(self, packet):
        """
        This method enables the user to change a value of a previously defined packet layer.

        :parameters:
            packet: dpkt.Packet obj
                a dpkt object that represents a packet.


        :raises:
            + :exc:`CTRexPktBuilder.IPAddressError`, in case invalid ip type option specified.

        """
        assert isinstance(packet, dpkt.Packet)
        self._packet = copy.copy(packet)

        self._pkt_by_hdr.clear()
        self._pkt_top_layer = self._packet
        # analyze packet to layers
        tmp_layer = self._packet
        while True:
            if isinstance(tmp_layer, dpkt.Packet):
                layer_name = self._gen_layer_name(type(tmp_layer).__name__)
                self._pkt_by_hdr[layer_name] = tmp_layer
                self._pkt_top_layer = tmp_layer
                try:
                    # check existence of upper layer
                    tmp_layer = tmp_layer.data
                except AttributeError:
                    # this is the most upper header
                    self._pkt_by_hdr['pkt_final_payload'] = tmp_layer.data
                    break
            else:
                self._pkt_by_hdr['pkt_final_payload'] = tmp_layer
                break
        return

    def get_packet(self, get_ptr=False):
        """
        This method enables the user to change a value of a previously defined packet layer.

        :parameters:
            get_ptr : bool
                indicate whether to get a reference to packet or a copy.
                Use only in advanced modes
                if set to true, metadata for packet is cleared, and any further modification is not guaranteed.

                default value : False

        :return:
                the current packet built by CTRexPktBuilder object.

        """
        if get_ptr:
            self._pkt_by_hdr = {}
            self._pkt_top_layer = None
            return self._packet

        else:
            return copy.copy(self._packet)

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
        pkt_in_hex = binascii.hexlify(str(self._packet))
        return [pkt_in_hex[i:i+2] for i in range(0, len(pkt_in_hex), 2)]

    # ----- useful shortcut methods ----- #
    def gen_dns_packet(self):
        pass

    # ----- internal methods ----- #
    def _reevaluate_packet(self, layer_name):
        cur_layer = self._packet
        known_layers = set(self._pkt_by_hdr.keys())
        found_layers = set()
        while True:
            pointing_layer_name = self._find_pointing_layer(known_layers, cur_layer)
            found_layers.add(pointing_layer_name)
            if self._pkt_by_hdr[layer_name] is cur_layer:
                self._pkt_top_layer = cur_layer
                disconnected_layers = known_layers.difference(found_layers)
                # remove disconnected layers
                for layer in disconnected_layers:
                    self._pkt_by_hdr.pop(layer)
                break
            else:
                cur_layer = cur_layer.data

    def _gen_layer_name(self, layer_class_name):
        assert isinstance(layer_class_name, str)
        layer_name = layer_class_name.lower()
        idx = 1
        while True:
            tmp_name = "{name}_{id}".format(name=layer_name, id=idx)
            if tmp_name not in self._pkt_by_hdr:
                return tmp_name
            else:
                idx += 1

    def _find_pointing_layer(self, known_layers, layer_obj):
        assert isinstance(known_layers, set)
        for layer in known_layers:
            if self._pkt_by_hdr[layer] is layer_obj:
                return layer

    @staticmethod
    def _decode_mac_addr(mac_addr):
        """
        Static method to test for MAC address validity.

        :parameters:
            mac_addr : str
                a string representing an MAC address, separated by ':' or '-'.

                examples: '00:de:34:ef:2e:f4', '00-de-34-ef-2e-f4

        :return:
            + an hex-string representation of the MAC address.
              for example, ip 00:de:34:ef:2e:f4 will return '\x00\xdeU\xef.\xf4'

        :raises:
            + :exc:`CTRexPktBuilder.MACAddressError`, in case invalid ip type option specified.

        """
        tmp_mac = mac_addr.lower().replace('-', ':')
        if re.match("[0-9a-f]{2}([-:])[0-9a-f]{2}(\\1[0-9a-f]{2}){4}$", tmp_mac):
            return binascii.unhexlify(tmp_mac.replace(':', ''))
            # another option for both Python 2 and 3: 
            # codecs.decode(tmp_mac.replace(':', ''), 'hex')
        else:
            raise CTRexPktBuilder.MACAddressError()

    @staticmethod
    def _decode_ip_addr(ip_addr, ip_type):
        """
        Static method to test for IPv4/IPv6 address validity.

        :parameters:
            ip_addr : str
                a string representing an IP address (IPv4/IPv6)

            ip_type : str
                The type of IP to be checked.
                Valid types: "ipv4", "ipv6".

        :return:
            + an hex-string representation of the ip address.
              for example, ip 1.2.3.4 will return '\x01\x02\x03\x04'

        :raises:
            + :exc:`CTRexPktBuilder.IPAddressError`, in case invalid ip type option specified.

        """
        if ip_type == "ipv4":
            try:
                return socket.inet_pton(socket.AF_INET, ip_addr)
            except AttributeError:  # no inet_pton here, sorry
                # try:
                return socket.inet_aton(ip_addr)
                # except socket.error:
                #     return False
                # return ip_addr.count('.') == 3
            except socket.error:  # not a valid address
                raise CTRexPktBuilder.IPAddressError()
        elif ip_type == "ipv6":
            try:
                return socket.inet_pton(socket.AF_INET6, ip_addr)
            except socket.error:  # not a valid address
                raise CTRexPktBuilder.IPAddressError()
        else:
            raise CTRexPktBuilder.IPAddressError()

    # ------ private classes ------ #
    class CTRexPayloadGen(object):

        def __init__(self, packet_ref, max_pkt_size):
            self._pkt_ref = packet_ref
            self._max_pkt_size = max_pkt_size

        def gen_random_str(self):
            gen_length = self._calc_gen_length()
            # return a string of size gen_length bytes, to pad the packet to its max_size
            return ''.join(random.SystemRandom().choice(string.ascii_letters + string.digits)
                           for _ in range(gen_length))

        def gen_repeat_ptrn(self, ptrn_to_repeat):
            gen_length = self._calc_gen_length()
            if isinstance(ptrn_to_repeat, str):
                # generate repeated string
                return (ptrn_to_repeat * (gen_length/len(ptrn_to_repeat) + 1))[:gen_length]
            elif isinstance(ptrn_to_repeat, int):
                ptrn = binascii.unhexlify(hex(ptrn_to_repeat)[2:])
                return (ptrn * (gen_length/len(ptrn) + 1))[:gen_length]
            elif isinstance(ptrn_to_repeat, tuple):
                if not all((isinstance(x, int) and (x < 255) and (x >= 0))
                           for x in ptrn_to_repeat):
                    raise ValueError("All numbers in tuple must be in range 0 <= number <= 255 ")
                # generate repeated sequence
                to_pack = (ptrn_to_repeat * (gen_length/len(ptrn_to_repeat) + 1))[:gen_length]
                return struct.pack('B'*gen_length, *to_pack)
            else:
                raise ValueError("Given ptrn_to_repeat argument type ({0}) is illegal.".
                                 format(type(ptrn_to_repeat)))

        def _calc_gen_length(self):
            return self._max_pkt_size - len(self._pkt_ref)

    class CTRexVM(object):
        """
        This class defines the TRex VM which represents how TRex will regenerate packets.
        The packets will be regenerated based on the built packet containing this class.
        """
        def __init__(self):
            """
            Instantiate a CTRexVM object

            :parameters:
                None
            """
            super(CTRexPktBuilder.CTRexVM, self).__init__()
            self.vm_variables = {}

        def set_vm_var_field(self, var_name, field_name, val):
            """
            Set VM variable field. Only existing variables are allowed to be changed.

            :parameters:
                var_name : str
                    a string representing the name of the VM variable to be changed.
                field_name : str
                    a string representing the field name of the VM variable to be changed.
                val :
                    a value to be applied to field_name field of the var_name VM variable.

            :raises:
                + :exc:`KeyError`, in case invalid var_name has been specified.
                + :exc:`CTRexPktBuilder.VMVarFieldTypeError`, in case mismatch between `val` and allowed type.
                + :exc:`CTRexPktBuilder.VMVarValueError`, in case val isn't one of allowed options of field_name.

            """
            return self.vm_variables[var_name].set_field(field_name, val)

        def add_flow_man_simple(self, name, **kwargs):
            """
            Adds a new flow manipulation object to the VM instance.

            :parameters:
                name : str
                    name of the manipulation, must be distinct.
                    Example: 'source_ip_change'

                **kwargs : dict
                    optional, set flow_man fields on initialization (key = field_name, val = field_val).
                    Must be used with legit fields, see :func:`CTRexPktBuilder.CTRexVM.CTRexVMVariable.set_field`.

            :return:
                None

            :raises:
                + :exc:`CTRexPktBuilder.VMVarNameExistsError`, in case of desired flow_man name already taken.
                + Exceptions from :func:`CTRexPktBuilder.CTRexVM.CTRexVMVariable.set_field` method.
                  Will rise when VM variables were misconfiguration.
            """
            if name not in self.vm_variables.keys():
                self.vm_variables[name] = self.CTRexVMVariable(name)
                # try configuring VM var attributes
                for (field, value) in kwargs.items():
                    self.vm_variables[name].set_field(field, value)
            else:
                raise CTRexPktBuilder.VMVarNameExistsError(name)

        def load_flow_man(self, flow_obj):
            """
            Loads an outer VM variable (instruction) into current VM.
            The outer VM variable must contain different name than existing VM variables currently registered on VM.

            :parameters:
                flow_obj : CTRexVMVariable
                    a CTRexVMVariable to be loaded into VM variable sets.

            :return:
                list holds variables data of VM

            """
            assert isinstance(flow_obj, CTRexPktBuilder.CTRexVM.CTRexVMVariable)
            if flow_obj.name not in self.vm_variables.keys():
                self.vm_variables[flow_obj.name] = flow_obj
            else:
                raise CTRexPktBuilder.VMVarNameExistsError(flow_obj.name)

        def dump(self):
            """
            dumps a VM variables (instructions) into an list data structure.

            :parameters:
                None

            :return:
                list holds variables data of VM

            """
            return [var.dump()
                    for key, var in self.vm_variables.items()]

        class CTRexVMVariable(object):
            """
            This class defines a single VM variable to be used as part of CTRexVar object.
            """
            VALID_SIZE = [1, 2, 4, 8]
            VALID_OPERATION = ["inc", "dec", "random"]

            def __init__(self, name):
                """
                Instantiate a CTRexVMVariable object

                :parameters:
                    name : str
                        a string representing the name of the VM variable.
                """
                super(CTRexPktBuilder.CTRexVM.CTRexVMVariable, self).__init__()
                self.name = name
                self.size = 4
                self.big_endian = True
                self.operation = "inc"
                self.split_by_core = False
                self.init_value = 1
                self.min_value = self.init_value
                self.max_value = self.init_value

            def set_field(self, field_name, val):
                """
                Set VM variable field. Only existing variables are allowed to be changed.

                :parameters:
                    field_name : str
                        a string representing the field name of the VM variable to be changed.
                    val :
                        a value to be applied to field_name field of the var_name VM variable.

                :return:
                    None

                :raises:
                    + :exc:`CTRexPktBuilder.VMVarNameError`, in case of illegal field name.
                    + :exc:`CTRexPktBuilder.VMVarFieldTypeError`, in case mismatch between `val` and allowed type.
                    + :exc:`CTRexPktBuilder.VMVarValueError`, in case val isn't one of allowed options of field_name.

                """
                if not hasattr(self, field_name):
                    raise CTRexPktBuilder.VMVarNameError(field_name)
                elif field_name == "size":
                    if type(val) != int:
                        raise CTRexPktBuilder.VMVarFieldTypeError("size", int)
                    elif val not in self.VALID_SIZE:
                        raise CTRexPktBuilder.VMVarValueError("size", self.VALID_SIZE)
                elif field_name == "init_value":
                    if type(val) != int:
                        raise CTRexPktBuilder.VMVarFieldTypeError("init_value", int)
                elif field_name == "operation":
                    if type(val) != str:
                        raise CTRexPktBuilder.VMVarFieldTypeError("operation", str)
                    elif val not in self.VALID_OPERATION:
                        raise CTRexPktBuilder.VMVarValueError("operation", self.VALID_OPERATION)
                elif field_name == "split_by_core":
                    val = bool(val)
                # update field value on success
                setattr(self, field_name, val)

            def is_valid(self):
                if self.size not in self.VALID_SIZE:
                    return False
                if self.type not in self.VALID_OPERATION:
                    return False
                return True

            def dump(self):
                """
                dumps a variable fields in a dictionary data structure.

                :parameters:
                    None

                :return:
                    dictionary holds variable data of VM variable

                """
                return {"ins_name": "flow_man_simple",  # VM variable dump always refers to manipulate instruction.
                        "flow_variable_name": self.name,
                        "object_size": self.size,
                        # "big_endian": self.big_endian,
                        "Operation": self.operation,
                        "split_by_core": self.split_by_core,
                        "init_value": self.init_value,
                        "min_value": self.min_value,
                        "max_value": self.max_value}

    class CPacketBuildException(Exception):
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

    class IPAddressError(CPacketBuildException):
        """
        This exception is used to indicate an error on the IP addressing part of the packet.
        """
        def __init__(self, message=''):
            self._default_message = 'Illegal type or value of IP address has been provided.'
            self.message = message or self._default_message
            super(CTRexPktBuilder.IPAddressError, self).__init__(-11, self.message)

    class MACAddressError(CPacketBuildException):
        """
        This exception is used to indicate an error on the MAC addressing part of the packet.
        """
        def __init__(self, message=''):
            self._default_message = 'Illegal MAC address has been provided.'
            self.message = message or self._default_message
            super(CTRexPktBuilder.MACAddressError, self).__init__(-11, self.message)

    class VMVarNameExistsError(CPacketBuildException):
        """
        This exception is used to indicate a duplicate usage of VM variable.
        """
        def __init__(self, name, message=''):
            self._default_message = 'The given VM name ({0}) already exists as part of the stream.'.format(name)
            self.message = message or self._default_message
            super(CTRexPktBuilder.VMVarNameExistsError, self).__init__(-21, self.message)

    class VMVarNameError(CPacketBuildException):
        """
        This exception is used to indicate that an undefined VM var field name has been accessed.
        """
        def __init__(self, name, message=''):
            self._default_message = "The given VM field name ({0}) is not defined and isn't legal.".format(name)
            self.message = message or self._default_message
            super(CTRexPktBuilder.VMVarNameError, self).__init__(-22, self.message)

    class VMVarFieldTypeError(CPacketBuildException):
        """
        This exception is used to indicate an illegal value has type has been given to VM variable field.
        """
        def __init__(self, name, ok_type, message=''):
            self._default_message = 'The desired value of field {field_name} is of type {field_type}, \
            and not of the allowed {allowed_type}.'.format(field_name=name,
                                                           field_type=type(name).__name__,
                                                           allowed_type=ok_type.__name__)
            self.message = message or self._default_message
            super(CTRexPktBuilder.VMVarFieldTypeError, self).__init__(-31, self.message)

    class VMVarValueError(CPacketBuildException):
        """
        This exception is used to indicate an error an illegal value has been assigned to VM variable field.
        """
        def __init__(self, name, ok_opts, message=''):
            self._default_message = 'The desired value of field {field_name} is illegal.\n \
            The only allowed options are: {allowed_opts}.'.format(field_name=name,
                                                                  allowed_opts=ok_opts)
            self.message = message or self._default_message
            super(CTRexPktBuilder.VMVarValueError, self).__init__(-32, self.message)


if __name__ == "__main__":
    pass
