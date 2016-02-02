#!/router/bin/python

import external_packages
import dpkt
import socket
import binascii
import copy
import random
import string
import struct
import re
import itertools
from abc import ABCMeta, abstractmethod
from collections import namedtuple
import base64

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
        self.vm = CTRexPktBuilder.CTRexVM()
        self.metadata = ""

    def clone (self):
        return copy.deepcopy(self)

    def add_pkt_layer(self, layer_name, pkt_layer):
        """
        This method adds additional header to the already existing packet

        :parameters:
            layer_name: str
                a string representing the name of the layer.
                Example: "l2", "l4_tcp", etc.

            pkt_layer : dpkt.Packet obj
                a dpkt object, generally from higher layer, that will be added on top of existing layer.

        :raises:
            + :exc:`ValueError`, in case the desired layer_name already exists.

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
        """
        This method sets the IP address fields of an IP header (source or destination, for both IPv4 and IPv6)
        using a human readable addressing representation.

        :parameters:
            layer_name: str
                a string representing the name of the layer.
                Example: "l3_ip", etc.

            attr: str
                a string representation of the sub-field to be set:

                + "src" for source
                + "dst" for destination

            ip_addr: str
                a string representation of the IP address to be set.
                Example: "10.0.0.1" for IPv4, or "5001::DB8:1:3333:1:1" for IPv6

            ip_type : str
                a string representation of the IP version to be set:

                + "ipv4" for IPv4
                + "ipv6" for IPv6

                Default: **ipv4**

        :raises:
            + :exc:`ValueError`, in case the desired layer_name is not an IP layer
            + :exc:`KeyError`, in case the desired layer_name does not exists.

        """
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
        """
        This method sets the IPv6 address fields of an IP header (source or destination)

        :parameters:
            layer_name: str
                a string representing the name of the layer.
                Example: "l3_ip", etc.

            attr: str
                a string representation of the sub-field to be set:

                + "src" for source
                + "dst" for destination

            ip_addr: str
                a string representation of the IP address to be set.
                Example: "5001::DB8:1:3333:1:1"

        :raises:
            + :exc:`ValueError`, in case the desired layer_name is not an IPv6 layer
            + :exc:`KeyError`, in case the desired layer_name does not exists.

        """
        self.set_ip_layer_addr(layer_name, attr, ip_addr, ip_type="ipv6")

    def set_eth_layer_addr(self, layer_name, attr, mac_addr):
        """
        This method sets the ethernet address fields of an Ethernet header (source or destination)
        using a human readable addressing representation.

        :parameters:
            layer_name: str
                a string representing the name of the layer.
                Example: "l2", etc.

            attr: str
                a string representation of the sub-field to be set:
                + "src" for source
                + "dst" for destination

            mac_addr: str
                a string representation of the MAC address to be set.
                Example: "00:de:34:ef:2e:f4".

        :raises:
            + :exc:`ValueError`, in case the desired layer_name is not an Ethernet layer
            + :exc:`KeyError`, in case the desired layer_name does not exists.

        """
        try:
            layer = self._pkt_by_hdr[layer_name.lower()]
            if not isinstance(layer, dpkt.ethernet.Ethernet):
                raise ValueError("The specified layer '{0}' is not of Ethernet type.".format(layer_name))
            else:
                decoded_mac = CTRexPktBuilder._decode_mac_addr(mac_addr)
                setattr(layer, attr, decoded_mac)
        except KeyError:
            raise KeyError("Specified layer '{0}' doesn't exist on packet.".format(layer_name))

    def set_layer_attr(self, layer_name, attr, val, toggle_bit=False):
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

            toggle_bit : bool
                Indicating if trying to set a specific bit of a field, such as "do not fragment" bit of IP layer.

                Default: **False**

        :raises:
            + :exc:`KeyError`, in case of missing layer (the desired layer isn't part of packet)
            + :exc:`ValueError`, in case invalid attribute to the specified layer.

        """
        try:
            layer = self._pkt_by_hdr[layer_name.lower()]
            if attr == 'data' and not isinstance(val, dpkt.Packet):
                # Don't allow setting 'data' attribute
                raise ValueError("Set a data attribute with object that is not dpkt.Packet is not allowed using "
                                 "set_layer_attr method.\nUse set_payload method instead.")
            if hasattr(layer, attr):
                if toggle_bit:
                    setattr(layer, attr, val | getattr(layer, attr, 0))
                else:
                    setattr(layer, attr, val)
                    if attr == 'data':
                        # re-evaluate packet from the start, possible broken link between layers
                        self._reevaluate_packet(layer_name.lower())
            else:
                raise ValueError("Given attr name '{0}' doesn't exists on specified layer ({1}).".format(layer_name,
                                                                                                         attr))
        except KeyError:
            raise KeyError("Specified layer '{0}' doesn't exist on packet.".format(layer_name))

    def set_layer_bit_attr(self, layer_name, attr, val):
        """
        This method enables the user to set the value of a field smaller that 1 Byte in size.
        This method isn't used to set full-sized fields value (>= 1 byte).
        Use :func:`packet_builder.CTRexPktBuilder.set_layer_attr` instead.

        :parameters:
            layer_name: str
                a string representing the name of the layer.
                Example: "l2", "l4_tcp", etc.

            attr : str
                a string representing the attribute to be set on desired layer

            val : int
                value of attribute.
                This value will be set "ontop" of the existing value using bitwise "OR" operation.

                .. tip:: It is very useful to use dpkt constants to define the values of these fields.

        :raises:
            + :exc:`KeyError`, in case of missing layer (the desired layer isn't part of packet)
            + :exc:`ValueError`, in case invalid attribute to the specified layer.

        """
        return self.set_layer_attr(layer_name, attr, val, True)

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

    def load_packet_from_pcap(self, pcap_path):
        """
        This method loads a pcap file into a parsed packet builder object.

        :parameters:
            pcap_path: str
                a path to a pcap file, containing a SINGLE packet.

        :raises:
            + :exc:`IOError`, in case provided path doesn't exists.

        """
        with open(pcap_path, 'r') as f:
            pcap = dpkt.pcap.Reader(f)
            first_packet = True
            for _, buf in pcap:
                # this is an iterator, can't evaluate the number of files in advance
                if first_packet:
                    self.load_packet(dpkt.ethernet.Ethernet(buf))
                else:
                    raise ValueError("Provided pcap file contains more than single packet.")
        # arrive here ONLY if pcap contained SINGLE packet
        return

    def load_from_stream_obj(self, stream_obj):
        self.load_packet_from_byte_list(stream_obj['packet']['binary'])


    def load_packet_from_byte_list(self, byte_list):

        buf = base64.b64decode(byte_list)
        # thn, load it based on dpkt parsing
        self.load_packet(dpkt.ethernet.Ethernet(buf))

    def get_packet(self, get_ptr=False):
        """
        This method provides access to the built packet, as an instance or as a pointer to packet itself.

        :parameters:
            get_ptr : bool
                indicate whether to get a reference to packet or a copy.
                Use only in advanced modes
                if set to true, metadata for packet is cleared, and any further modification is not guaranteed.

                default value : False

        :return:
            + the current packet built by CTRexPktBuilder object.
            + None if packet is empty

        """
        if get_ptr:
            self._pkt_by_hdr = {}
            self._pkt_top_layer = None
            return self._packet
        else:
            return copy.copy(self._packet)

    def get_packet_length(self):
        return len(self._packet)

    def get_layer(self, layer_name):
        """
        This method provides access to a specific layer of the packet, as a **copy of the layer instance**.

        :parameters:
            layer_name : str
                the name given to desired layer

        :return:
            + a copy of the desired layer of the current packet if exists.
            + None if no such layer

        """
        layer = self._pkt_by_hdr.get(layer_name)
        return copy.copy(layer) if layer else None


    # VM access methods
    def set_vm_ip_range(self, ip_layer_name, ip_field,
                        ip_start, ip_end, operation,
                        ip_init = None, add_value = 0,
                        is_big_endian=True, val_size=4,
                        ip_type="ipv4", add_checksum_inst=True,
                        split = False):

        if ip_field not in ["src", "dst"]:
            raise ValueError("set_vm_ip_range only available for source ('src') or destination ('dst') ip addresses")
        # set differences between IPv4 and IPv6
        if ip_type == "ipv4":
            ip_class = dpkt.ip.IP
            ip_addr_size = val_size if val_size <= 4 else 4
        elif ip_type == "ipv6":
            ip_class = dpkt.ip6.IP6
            ip_addr_size = val_size if val_size <= 8 else 4
        else:
            raise CTRexPktBuilder.IPAddressError()

        self._verify_layer_prop(ip_layer_name, ip_class)
        trim_size = ip_addr_size*2
        start_val = int(binascii.hexlify(CTRexPktBuilder._decode_ip_addr(ip_start, ip_type))[-trim_size:], 16)
        end_val = int(binascii.hexlify(CTRexPktBuilder._decode_ip_addr(ip_end, ip_type))[-trim_size:], 16)

        if ip_init == None:
            init_val = start_val
        else:
            init_val = int(binascii.hexlify(CTRexPktBuilder._decode_ip_addr(ip_init, ip_type))[-trim_size:], 16)


        # All validations are done, start adding VM instructions
        flow_var_name = "{layer}__{field}".format(layer=ip_layer_name, field=ip_field)

        hdr_offset, field_abs_offset = self._calc_offset(ip_layer_name, ip_field, ip_addr_size)
        self.vm.add_flow_man_inst(flow_var_name, size=ip_addr_size, operation=operation,
                                  init_value=init_val,
                                  min_value=start_val,
                                  max_value=end_val)
        self.vm.add_write_flow_inst(flow_var_name, field_abs_offset)
        self.vm.set_vm_off_inst_field(flow_var_name, "add_value", add_value)
        self.vm.set_vm_off_inst_field(flow_var_name, "is_big_endian", is_big_endian)
        if ip_type == "ipv4" and add_checksum_inst:
            self.vm.add_fix_checksum_inst(self._pkt_by_hdr.get(ip_layer_name), hdr_offset)

        if split:
            self.vm.set_split_by_var(flow_var_name)


    def set_vm_eth_range(self, eth_layer_name, eth_field,
                         mac_init, mac_start, mac_end, add_value,
                         operation, val_size=4, is_big_endian=False):
        if eth_field not in ["src", "dst"]:
            raise ValueError("set_vm_eth_range only available for source ('src') or destination ('dst') eth addresses")
        self._verify_layer_prop(eth_layer_name, dpkt.ethernet.Ethernet)
        eth_addr_size = val_size if val_size <= 4 else 4
        trim_size = eth_addr_size*2
        init_val = int(binascii.hexlify(CTRexPktBuilder._decode_mac_addr(mac_init))[-trim_size:], 16)
        start_val = int(binascii.hexlify(CTRexPktBuilder._decode_mac_addr(mac_start))[-trim_size:], 16)
        end_val = int(binascii.hexlify(CTRexPktBuilder._decode_mac_addr(mac_end))[-trim_size:], 16)
        # All validations are done, start adding VM instructions
        flow_var_name = "{layer}__{field}".format(layer=eth_layer_name, field=eth_field)
        hdr_offset, field_abs_offset = self._calc_offset(eth_layer_name, eth_field, eth_addr_size)
        self.vm.add_flow_man_inst(flow_var_name, size=8, operation=operation,
                                  init_value=init_val,
                                  min_value=start_val,
                                  max_value=end_val)
        self.vm.add_write_flow_inst(flow_var_name, field_abs_offset)
        self.vm.set_vm_off_inst_field(flow_var_name, "add_value", add_value)
        self.vm.set_vm_off_inst_field(flow_var_name, "is_big_endian", is_big_endian)

    def set_vm_custom_range(self, layer_name, hdr_field,
                            init_val, start_val, end_val, add_val, val_size,
                            operation, is_big_endian=True, range_name="",
                            add_checksum_inst=True):
        # verify input validity for init/start/end values
        for val in [init_val, start_val, end_val]:
            if not isinstance(val, int):
                raise ValueError("init/start/end values are expected integers, but received type '{0}'".
                                 format(type(val)))
        self._verify_layer_prop(layer_name=layer_name, field_name=hdr_field)
        if not range_name:
            range_name = "{layer}__{field}".format(layer=layer_name, field=hdr_field)
        trim_size = val_size*2
        hdr_offset, field_abs_offset = self._calc_offset(layer_name, hdr_field, val_size)
        self.vm.add_flow_man_inst(range_name, size=val_size, operation=operation,
                                  init_value=init_val,
                                  min_value=start_val,
                                  max_value=end_val)
        self.vm.add_write_flow_inst(range_name, field_abs_offset)
        self.vm.set_vm_off_inst_field(range_name, "add_value", add_val)
        self.vm.set_vm_off_inst_field(range_name, "is_big_endian", is_big_endian)
        if isinstance(self._pkt_by_hdr.get(layer_name), dpkt.ip.IP) and add_checksum_inst:
            self.vm.add_fix_checksum_inst(self._pkt_by_hdr.get(layer_name), hdr_offset)

    def get_vm_data(self):
        return self.vm.dump()

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
            + :exc:`CTRexPktBuilder.EmptyPacketError`, in case packet is empty.

        """
        if self._packet is None:
            raise CTRexPktBuilder.EmptyPacketError()

        if encode:
            return {"binary": base64.b64encode(str(self._packet)),
                    "meta": self.metadata}
        return {"binary": str(self._packet),
                "meta": self.metadata}


    def dump_pkt_to_pcap(self, file_path, ts=None):
        """
        Dumps the packet as a decimal array of bytes (each item x gets value between 0-255)

        :parameters:
            file_path : str
                a path (including filename) to which to write to pcap file to.

            ts : int
                a timestamp to attach to the packet when dumped to pcap file.
                if ts in None, then time.time() is used to set the timestamp.

                Default: **None**

        :return:
            None

        :raises:
            + :exc:`CTRexPktBuilder.EmptyPacketError`, in case packet is empty.

        """
        if self._packet is None:
            raise CTRexPktBuilder.EmptyPacketError()
        try:
            with open(file_path, 'wb') as f:
                pcap_wr = dpkt.pcap.Writer(f)
                pcap_wr.writepkt(self._packet, ts)
                return
        except IOError:
            raise IOError(2, "The provided path could not be accessed")

    def get_packet_layers(self, depth_limit=Ellipsis):
        if self._packet is None:
            raise CTRexPktBuilder.EmptyPacketError()
        cur_layer = self._packet
        layer_types = []
        if depth_limit == Ellipsis:
            iterator = itertools.count(1)
        else:
            iterator = xrange(depth_limit)
        for _ in iterator:
            # append current layer type
            if isinstance(cur_layer, dpkt.Packet):
                layer_types.append(type(cur_layer).__name__)
            else:
                # if not dpkt layer, refer as payload
                layer_types.append("PLD")
            # advance to next layer
            if not hasattr(cur_layer, "data"):
                break
            else:
                cur_layer = cur_layer.data
        return layer_types

    def export_pkt(self, file_path, link_pcap=False, pcap_name=None, pcap_ts=None):
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

    def _calc_offset(self, layer_name, hdr_field, hdr_field_size):
        pkt_header = self._pkt_by_hdr.get(layer_name)
        hdr_offset = len(self._packet) - len(pkt_header)
        inner_hdr_offsets = []
        for field in pkt_header.__hdr__:
            if field[0] == hdr_field:
                field_size = struct.calcsize(field[1])
                if field_size == hdr_field_size:
                    break
                elif field_size < hdr_field_size:
                    raise CTRexPktBuilder.PacketLayerError(layer_name,
                                                           "The specified field '{0}' size is smaller than given range"
                                                           " size ('{1}')".format(hdr_field, hdr_field_size))
                else:
                    inner_hdr_offsets.append(field_size - hdr_field_size)
                    break
            else:
                inner_hdr_offsets.append(struct.calcsize(field[1]))
        return hdr_offset, hdr_offset + sum(inner_hdr_offsets)

    def _verify_layer_prop(self, layer_name, layer_type=None, field_name=None):
        if layer_name not in self._pkt_by_hdr:
            raise CTRexPktBuilder.PacketLayerError(layer_name)
        pkt_layer = self._pkt_by_hdr.get(layer_name)
        if layer_type:
            # check for layer type
            if not isinstance(pkt_layer, layer_type):
                raise CTRexPktBuilder.PacketLayerTypeError(layer_name, type(pkt_layer), layer_type)
        if field_name and not hasattr(pkt_layer, field_name):
            # check if field exists on certain header
            raise CTRexPktBuilder.PacketLayerError(layer_name, "The specified field '{0}' does not exists on "
                                                               "given packet layer ('{1}')".format(field_name,
                                                                                                   layer_name))
        return

    @property
    def payload_gen(self):
        return CTRexPktBuilder.CTRexPayloadGen(self._packet, self._max_pkt_size)

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
        InstStore = namedtuple('InstStore', ['type', 'inst'])

        def __init__(self):
            """
            Instantiate a CTRexVM object

            :parameters:
                None
            """
            super(CTRexPktBuilder.CTRexVM, self).__init__()
            self.vm_variables = {}
            self._inst_by_offset = {}   # this data structure holds only offset-related instructions, ordered in tuples
            self._off_inst_by_name = {}
            self.split_by_var = ''

        def set_vm_var_field(self, var_name, field_name, val, offset_inst=False):
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
            if offset_inst:
                return self._off_inst_by_name[var_name].inst.set_field(field_name, val)
            else:
                return self.vm_variables[var_name].set_field(field_name, val)

        def set_vm_off_inst_field(self, var_name, field_name, val):
            return self.set_vm_var_field(var_name, field_name, val, True)

        def add_flow_man_inst(self, name, **kwargs):
            """
            Adds a new flow manipulation object to the VM instance.

            :parameters:
                name : str
                    name of the manipulation, must be distinct.
                    Example: 'source_ip_change'

                **kwargs** : dict
                    optional, set flow_man fields on initialization (key = field_name, val = field_val).
                    Must be used with legit fields, see :func:`CTRexPktBuilder.CTRexVM.CTRexVMVariable.set_field`.

            :return:
                None

            :raises:
                + :exc:`CTRexPktBuilder.VMVarNameExistsError`, in case of desired flow_man name already taken.
                + Exceptions from :func:`CTRexPktBuilder.CTRexVM.CTRexVMVariable.set_field` method.
                  Will rise when VM variables were misconfiguration.
            """
            if name not in self.vm_variables:
                self.vm_variables[name] = self.CTRexVMFlowVariable(name)
                # try configuring VM instruction attributes
                for (field, value) in kwargs.items():
                    self.vm_variables[name].set_field(field, value)
            else:
                raise CTRexPktBuilder.VMVarNameExistsError(name)

        def add_fix_checksum_inst(self, linked_ipv4_obj, offset_to_obj=14, name=None):
            # check if specified linked_ipv4_obj is indeed an ipv4 object
            if not (isinstance(linked_ipv4_obj, dpkt.ip.IP)):
                raise ValueError("The provided layer object is not of IPv4.")
            if not name:
                name = "checksum_{off}".format(off=offset_to_obj)   # name will override previous checksum inst, OK
            new_checksum_inst = self.CTRexVMChecksumInst(name, offset_to_obj)
            # store the checksum inst in the end of the IP header (20 Bytes long)
            inst = self.InstStore('checksum', new_checksum_inst)
            self._inst_by_offset[offset_to_obj + 20] = inst
            self._off_inst_by_name[name] = inst

        def add_write_flow_inst(self, name, pkt_offset, **kwargs):
            if name not in self.vm_variables:
                raise KeyError("Trying to add write_flow_var instruction to a not-exists VM flow variable ('{0}')".
                               format(name))
            else:
                new_write_inst = self.CTRexVMWrtFlowVarInst(name, pkt_offset)
                # try configuring VM instruction attributes
                for (field, value) in kwargs.items():
                    new_write_inst.set_field(field, value)
                # add the instruction to the date-structure
                inst = self.InstStore('write', new_write_inst)
                self._inst_by_offset[pkt_offset] = inst
                self._off_inst_by_name[name] = inst

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
            assert isinstance(flow_obj, CTRexPktBuilder.CTRexVM.CTRexVMFlowVariable)
            if flow_obj.name not in self.vm_variables.keys():
                self.vm_variables[flow_obj.name] = flow_obj
            else:
                raise CTRexPktBuilder.VMVarNameExistsError(flow_obj.name)

        def set_split_by_var (self, var_name):
            if var_name not in self.vm_variables:
                raise KeyError("cannot set split by var to an unknown VM var ('{0}')".
                               format(var_name))

            self.split_by_var = var_name

        def dump(self):
            """
            dumps a VM variables (instructions) and split_by_var into a dict data structure.

            :parameters:
                None

            :return:
                dict with VM instructions as list and split_by_var as str

            """

            # at first, dump all CTRexVMFlowVariable instructions
            inst_array = [var.dump() if hasattr(var, 'dump') else var
                          for key, var in self.vm_variables.items()]
            # then, dump all the CTRexVMWrtFlowVarInst and CTRexVMChecksumInst instructions
            inst_array += [self._inst_by_offset.get(key).inst.dump()
                           for key in sorted(self._inst_by_offset)]
            return {'instructions': inst_array, 'split_by_var': self.split_by_var}

        class CVMAbstractInstruction(object):
            __metaclass__ = ABCMeta

            def __init__(self, name):
                """
                Instantiate a CTRexVMVariable object

                :parameters:
                    name : str
                        a string representing the name of the VM variable.
                """
                super(CTRexPktBuilder.CTRexVM.CVMAbstractInstruction, self).__init__()
                self.name = name

            def set_field(self, field_name, val):
                if not hasattr(self, field_name):
                    raise CTRexPktBuilder.VMFieldNameError(field_name)
                setattr(self, field_name, val)

            @abstractmethod
            def dump(self):
                pass

        class CTRexVMFlowVariable(CVMAbstractInstruction):
            """
            This class defines a single VM variable to be used as part of CTRexVar object.
            """
            VALID_SIZE = [1, 2, 4, 8]   # size in Bytes
            VALID_OPERATION = ["inc", "dec", "random"]

            def __init__(self, name):
                """
                Instantiate a CTRexVMVariable object

                :parameters:
                    name : str
                        a string representing the name of the VM variable.
                """
                super(CTRexPktBuilder.CTRexVM.CTRexVMFlowVariable, self).__init__(name)
                # self.name = name
                self.size = 4
                self.big_endian = True
                self.operation = "inc"
                # self.split_by_core = False
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
                    raise CTRexPktBuilder.VMFieldNameError(field_name)
                elif field_name == "size":
                    if type(val) != int:
                        raise CTRexPktBuilder.VMFieldTypeError("size", int)
                    elif val not in self.VALID_SIZE:
                        raise CTRexPktBuilder.VMFieldValueError("size", self.VALID_SIZE)
                elif field_name in ["init_value", "min_value", "max_value"]:
                    if type(val) != int:
                        raise CTRexPktBuilder.VMFieldTypeError(field_name, int)
                elif field_name == "operation":
                    if type(val) != str:
                        raise CTRexPktBuilder.VMFieldTypeError("operation", str)
                    elif val not in self.VALID_OPERATION:
                        raise CTRexPktBuilder.VMFieldValueError("operation", self.VALID_OPERATION)
                # elif field_name == "split_by_core":
                #     val = bool(val)
                # update field value on success
                setattr(self, field_name, val)

            def dump(self):
                """
                dumps a variable fields in a dictionary data structure.

                :parameters:
                    None

                :return:
                    dictionary holds variable data of VM variable

                """
                return {"type": "flow_var",  # VM variable dump always refers to manipulate instruction.
                        "name": self.name,
                        "size": self.size,
                        "op": self.operation,
                        # "split_by_core": self.split_by_core,
                        "init_value": self.init_value,
                        "min_value":  self.min_value,
                        "max_value":  self.max_value}

        class CTRexVMChecksumInst(CVMAbstractInstruction):

            def __init__(self, name, offset):
                """
                Instantiate a CTRexVMChecksumInst object

                :parameters:
                    name : str
                        a string representing the name of the VM variable.
                """
                super(CTRexPktBuilder.CTRexVM.CTRexVMChecksumInst, self).__init__(name)
                self.pkt_offset = offset

            def dump(self):
                return {"type": "fix_checksum_ipv4",
                        "pkt_offset": int(self.pkt_offset)}

        class CTRexVMWrtFlowVarInst(CVMAbstractInstruction):

            def __init__(self, name, pkt_offset):
                """
                Instantiate a CTRexVMWrtFlowVarInst object

                :parameters:
                    name : str
                        a string representing the name of the VM variable.
                """
                super(CTRexPktBuilder.CTRexVM.CTRexVMWrtFlowVarInst, self).__init__(name)
                self.pkt_offset = int(pkt_offset)
                self.add_value = 0
                self.is_big_endian = False

            def set_field(self, field_name, val):
                if not hasattr(self, field_name):
                    raise CTRexPktBuilder.VMFieldNameError(field_name)
                elif field_name == 'pkt_offset':
                    raise ValueError("pkt_offset value cannot be changed")
                cur_attr_type = type(getattr(self, field_name))
                if cur_attr_type == type(val):
                    setattr(self, field_name, val)
                else:
                    CTRexPktBuilder.VMFieldTypeError(field_name, cur_attr_type)

            def dump(self):
                return {"type": "write_flow_var",
                        "name": self.name,
                        "pkt_offset": self.pkt_offset,
                        "add_value": int(self.add_value),
                        "is_big_endian": bool(self.is_big_endian)
                        }

        class CTRexVMChecksumInst(CVMAbstractInstruction):

            def __init__(self, name, offset):
                """
                Instantiate a CTRexVMChecksumInst object

                :parameters:
                    name : str
                        a string representing the name of the VM variable.
                """
                super(CTRexPktBuilder.CTRexVM.CTRexVMChecksumInst, self).__init__(name)
                self.pkt_offset = offset

            def dump(self):
                return {"type": "fix_checksum_ipv4",
                        "pkt_offset": int(self.pkt_offset)}

        class CTRexVMWrtFlowVarInst(CVMAbstractInstruction):

            def __init__(self, name, pkt_offset):
                """
                Instantiate a CTRexVMWrtFlowVarInst object

                :parameters:
                    name : str
                        a string representing the name of the VM variable.
                """
                super(CTRexPktBuilder.CTRexVM.CTRexVMWrtFlowVarInst, self).__init__(name)
                self.pkt_offset = int(pkt_offset)
                self.add_value = 0
                self.is_big_endian = False

            def set_field(self, field_name, val):
                if not hasattr(self, field_name):
                    raise CTRexPktBuilder.VMFieldNameError(field_name)
                elif field_name == 'pkt_offset':
                    raise ValueError("pkt_offset value cannot be changed")
                cur_attr_type = type(getattr(self, field_name))
                if cur_attr_type == type(val):
                    setattr(self, field_name, val)
                else:
                    CTRexPktBuilder.VMFieldTypeError(field_name, cur_attr_type)

            def dump(self):
                return {"type": "write_flow_var",
                        "name": self.name,
                        "pkt_offset": self.pkt_offset,
                        "add_value": int(self.add_value),
                        "is_big_endian": bool(self.is_big_endian)
                        }

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

    class EmptyPacketError(CPacketBuildException):
        """
        This exception is used to indicate an error caused by operation performed on an empty packet.
        """
        def __init__(self, message=''):
            self._default_message = 'Illegal operation on empty packet.'
            self.message = message or self._default_message
            super(CTRexPktBuilder.EmptyPacketError, self).__init__(-10, self.message)

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
            super(CTRexPktBuilder.MACAddressError, self).__init__(-12, self.message)

    class PacketLayerError(CPacketBuildException):
        """
        This exception is used to indicate an error caused by operation performed on an non-exists layer of the packet.
        """
        def __init__(self, name, message=''):
            self._default_message = "The given packet layer name ({0}) does not exists.".format(name)
            self.message = message or self._default_message
            super(CTRexPktBuilder.PacketLayerError, self).__init__(-13, self.message)

    class PacketLayerTypeError(CPacketBuildException):
        """
        This exception is used to indicate an error caused by operation performed on an non-exists layer of the packet.
        """
        def __init__(self, name, layer_type, ok_type, message=''):
            self._default_message = "The type of packet layer {layer_name} is of type {layer_type}, " \
                                    "and not of the expected {allowed_type}.".format(layer_name=name,
                                                                                     layer_type=layer_type,
                                                                                     allowed_type=ok_type.__name__)
            self.message = message or self._default_message
            super(CTRexPktBuilder.PacketLayerTypeError, self).__init__(-13, self.message)

    class VMVarNameExistsError(CPacketBuildException):
        """
        This exception is used to indicate a duplicate usage of VM variable.
        """
        def __init__(self, name, message=''):
            self._default_message = 'The given VM name ({0}) already exists as part of the stream.'.format(name)
            self.message = message or self._default_message
            super(CTRexPktBuilder.VMVarNameExistsError, self).__init__(-21, self.message)

    class VMFieldNameError(CPacketBuildException):
        """
        This exception is used to indicate that an undefined VM var field name has been accessed.
        """
        def __init__(self, name, message=''):
            self._default_message = "The given VM field name ({0}) is not defined and isn't legal.".format(name)
            self.message = message or self._default_message
            super(CTRexPktBuilder.VMFieldNameError, self).__init__(-22, self.message)

    class VMFieldTypeError(CPacketBuildException):
        """
        This exception is used to indicate an illegal value has type has been given to VM variable field.
        """
        def __init__(self, name, ok_type, message=''):
            self._default_message = "The desired value of field {field_name} is of type {field_type}, " \
                                    "and not of the allowed {allowed_type}.".format(field_name=name,
                                                                                    field_type=type(name).__name__,
                                                                                    allowed_type=ok_type.__name__)
            self.message = message or self._default_message
            super(CTRexPktBuilder.VMFieldTypeError, self).__init__(-31, self.message)

    class VMFieldValueError(CPacketBuildException):
        """
        This exception is used to indicate an error an illegal value has been assigned to VM variable field.
        """
        def __init__(self, name, ok_opts, message=''):
            self._default_message = "The desired value of field {field_name} is illegal.\n" \
                                    "The only allowed options are: {allowed_opts}.".format(field_name=name,
                                                                                           allowed_opts=ok_opts)
            self.message = message or self._default_message
            super(CTRexPktBuilder.VMFieldValueError, self).__init__(-32, self.message)


if __name__ == "__main__":
    pass
