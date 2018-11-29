import imp
import json
import yaml
import os
import sys
import math

from ..common.services.trex_service_arp import ServiceARP
from ..common.trex_exceptions import *
from ..common.trex_types import listify, validate_type, basestring
from ..utils.common import *
from ..utils.text_tables import TRexTextTable, print_table_with_header

DST_MAC, DST_IPv4, DST_IPv6 = range(3)
MAX_VIF_ID = 100000

def split_port_str(port_id):
    validate_type('port_id', port_id, basestring)
    port_ids = port_id.split('.')
    try:
        if len(port_ids) == 1:
            trex_port = int(port_id)
            sub_if    = 0
        elif len(port_ids) == 2:
            trex_port = int(port_ids[0])
            sub_if    = int(port_ids[1])
        else:
            raise ValueError('')
    except ValueError:
        raise TRexError("Invalid port_id %s, valid examples: '1' for TRex port 1 or '0.2' for TRex port 0 and sub-interface 2" % port_id)

    if sub_if < 0 or sub_if > MAX_VIF_ID:
        raise TRexError('sub_if should be between 1 and %s, got: %s' % (MAX_VIF_ID, sub_if))

    return trex_port, sub_if


class TopoGW(object):
    def __init__(self, port_id, src_start, src_end, dst, dst_mac = ''):
        trex_port, sub_if = split_port_str(port_id)

        if not is_valid_ipv4(src_start):
            raise TRexError("src_start is not a valid IPv4 address: '%s'" % src_start)
        if not is_valid_ipv4(src_end):
            raise TRexError("src_end is not a valid IPv4 address: '%s'" % src_end)

        if is_valid_ipv4(dst):
            self.dst_type = DST_IPv4
        elif is_valid_ipv6(dst):
            self.dst_type = DST_IPv6
        elif is_valid_mac(dst):
            self.dst_type = DST_MAC
        else:
            raise TRexError('dst should be either IPv4, IPv6 or MAC address, got: %s' % dst)

        self.port_id     = port_id
        self.trex_port   = trex_port
        self.sub_if      = sub_if
        self.src_start   = src_start
        self.src_end     = src_end
        self.dst         = dst
        if dst_mac:
            self.dst_mac = dst_mac
        elif self.dst_type == DST_MAC:
            self.dst_mac = dst
        else:
            self.dst_mac = None

    def get_data(self, to_server):
        d = {}
        d['src_start'] = self.src_start
        d['src_end']   = self.src_end
        d['dst']       = self.dst
        if to_server:
            d['trex_port'] = self.trex_port
            d['sub_if']    = self.sub_if
            d['dst_mac'] = self.dst_mac
        else:
            d['port_id']   = self.port_id
        return d

    def to_code(self):
        data = self.get_data(False)
        return "TopoGW('{port_id}', '{src_start}', '{src_end}', {sub_if}, '{dst}')".format(**data)


class TopoVIF(object):
    def __init__(self, port_id, src_mac, src_ipv4 = '', src_ipv6 = '', vlan = 0):
        trex_port, sub_if = split_port_str(port_id)

        if not is_valid_mac(src_mac):
            raise TRexError('src_mac is not valid MAC address: %s' % src_mac)
        if src_ipv4 and not is_valid_ipv4(src_ipv4):
            raise TRexError('src_ipv4 is not valid IPv4 address: %s' % src_ipv4)
        if src_ipv6 and not is_valid_ipv6(src_ipv6):
            raise TRexError('src_ipv6 is not valid IPv6 address: %s' % src_ipv6)
        validate_type('vlan', vlan, int)
        if vlan < 0 or vlan > 4096:
            raise TRexError('Invalid value for VLAN: %s' % vlan)

        self.port_id   = port_id
        self.trex_port = trex_port
        self.sub_if    = sub_if
        self.src_mac   = src_mac
        self.src_ipv4  = src_ipv4
        self.src_ipv6  = src_ipv6
        self.vlan      = vlan

    def get_data(self, to_server):
        d = {}
        d['src_mac']   = self.src_mac
        d['src_ipv4']  = self.src_ipv4
        d['src_ipv6']  = self.src_ipv6
        d['vlan']      = self.vlan
        if to_server:
            d['trex_port'] = self.trex_port
            d['sub_if']    = self.sub_if
        else:
            d['port_id']   = self.port_id
        return d

    def to_code(self):
        data = self.get_data(False)
        return "TopoVIF('{port_id}', '{src_mac}', '{src_ipv4}', '{src_ipv6}', {vlan})".format(**data)


class ASTFTopology(object):
    """ ASTF topology

       .. code-block:: python

       TODO: add example
    """

    def __init__(self, vifs = None, gws = None):
        self.vifs = vifs or []
        self.gws  = gws or []

    def add_vif_obj(self, vif):
        validate_type('vif', vif, TopoVIF)
        self.vifs.append(vif)

    def add_gw_obj(self, gw):
        validate_type('gw', gw, TopoGW)
        self.gws.append(gw)

    def add_vif(self, *a, **k):
        trex_port = k.get('trex_port')
        if trex_port is not None:
            k['port_id'] = '%s.%s' % (trex_port, k['sub_if'])
            del k['sub_if']
            del k['trex_port']
        vif = TopoVIF(*a, **k)
        self.vifs.append(vif)

    def add_gw(self, *a, **k):
        trex_port = k.get('trex_port')
        if trex_port is not None:
            k['port_id'] = '%s.%s' % (trex_port, k['sub_if'])
            del k['sub_if']
            del k['trex_port']
        gw = TopoGW(*a, **k)
        self.gws.append(gw)

    def is_empty(self):
        return len(self.gws) + len(self.vifs) == 0


    def get_data(self, to_server = True):
        data = {}
        data['vifs'] = [vif.get_data(to_server) for vif in self.vifs]
        data['gws'] = [gw.get_data(to_server) for gw in self.gws]
        return data


class ASTFTopologyManager(object):
    def __init__(self, client):
        self.client = client


    def is_empty(self):
        for port in self.client.ports.values():
            if not port.topo.is_empty():
                return False
        return True


    def split_per_port(self, topo):
        topo_per_port = {}
        for port_id, port in self.client.ports.items():
            topo_per_port[port_id] = ASTFTopology()

        for vif in topo.vifs:
            trex_port_id = vif.trex_port
            trex_port = self.client.ports.get(trex_port_id)
            if not trex_port:
                raise TRexError('VIF has port_id %s, which requires TRex interface %s' % (vif.port_id, trex_port_id))
            topo_per_port[trex_port_id].add_vif_obj(vif)

        for gw in topo.gws:
            trex_port_id = gw.trex_port
            trex_port = self.client.ports.get(trex_port_id)
            if not trex_port:
                raise TRexError('GW has port_id %s requires TRex interface %s' % (gw.port_id, trex_port_id))
            topo_per_port[trex_port_id].add_gw_obj(gw)
        return topo_per_port


    def warn(self, msg):
        self.client.logger.warning('WARNING: %s' % msg)


    def validate_topo(self, topo_per_port):
        start_end_gw = []
        for topo in topo_per_port.values():
            for gw in topo.gws:
                start = tuple(map(int, gw.src_start.split('.')))
                end   = tuple(map(int, gw.src_end.split('.')))
                if start > end:
                    raise TRexError('GW src_start: %s is higher than src_end: %s' % (gw.src_start, gw.src_end))
                start_end_gw.append((start, end, gw))

        start_end_gw.sort(key = lambda t:t[0]) # N*log(N)
        p_gw = None
        for start, end, gw in start_end_gw:
            if p_gw:
                if start == p_start:
                    raise TRexError('At least two GWs start range with: %s' % (p_gw.src_start))
                if p_end >= start:
                    raise TRexError('GW ranges intersect: %s-%s, %s-%s' % (p_gw.src_start, p_gw.src_end, gw.src_start, gw.src_end))
            p_start, p_end, p_gw = start, end, gw

        prom_warnings = {}
        for port_id, port in self.client.ports.items():
            port_attr = port.get_ts_attr()
            prom_enabled = port_attr['promiscuous']['enabled']
            port_src_mac = port_attr['layer_cfg']['ether']['src']
            port_has_ipv4 = port_attr['layer_cfg']['ipv4']['state'] != 'none'
            port_has_ipv6 = port_attr['layer_cfg']['ipv6']['enabled']

            vif_ids = {}
            vif_macs = {}
            port_topo = topo_per_port[port_id]
            for vif in port_topo.vifs:
                vif_id = vif.sub_if
                if vif_id in vif_ids:
                    raise TRexError('Duplicate VIF - %s' % vif.port_str())
                vif_ids[vif_id] = vif
                vif_mac = vif.src_mac
                if vif_mac in vif_macs:
                    raise TRexError('Duplicate VIF MAC: %s' % vif_mac)
                vif_macs[vif_mac] = vif
                if not prom_enabled and vif_mac != port_src_mac:
                    prom_warnings[port_id] = 1

            for gw in port_topo.gws:
                gw_sub_if = gw.sub_if
                if gw_sub_if:
                    port = vif_ids.get(gw_sub_if)
                    if not port:
                        raise TRexError('Invalid port in GW - %s' % gw.port_str())
                    if gw.dst_type == DST_IPv4 and not port.src_ipv4:
                        raise TRexError("VIF %s does not have IPv4 configured, can't set GW %s" % (gw_port_id, gw.dst))
                    elif gw.dst_type == DST_IPv6 and not port.src_ipv6:
                        raise TRexError("VIF %s does not have IPv6 configured, can't set GW %s" % (gw_port_id, gw.dst))
                else:
                    if gw.dst_type == DST_IPv4 and not port_has_ipv4:
                        raise TRexError("Port %s does not have IPv4 configured, can't set GW %s" % (gw_port_id, gw.dst))
                    elif gw.dst_type == DST_IPv6 and not port_has_ipv6:
                        raise TRexError("Port %s does not have IPv6 configured, can't set GW %s" % (gw_port_id, gw.dst))
        if prom_warnings:
            self.warn('Promiscuous mode must be enabled on port(s) %s for VIFs to work' % list(prom_warnings.keys()))


    def load(self, topology, **kw):
        if not self.is_empty():
            raise TRexError('Topology is already loaded, clear it first')

        if not isinstance(topology, ASTFTopology):
            x = os.path.basename(topology).split('.')
            suffix = x[1] if len(x) == 2 else topology

            if suffix == 'py':
                topology = self.load_py(topology, **kw)

            elif suffix == 'json':
                topology = self.load_json(topology)

            elif suffix == 'yaml':
                topology = self.load_yaml(topology)

            else:
                raise TRexError("Unknown topology file type: '%s'" % suffix)

        topo_per_port = self.split_per_port(topology)
        self.validate_topo(topo_per_port)
        for port_id, port in self.client.ports.items():
            port.topo = topo_per_port[port_id]

        if self.is_empty():
            raise TRexError('Loaded topology is empty!')


    @staticmethod
    def get_module_tunables(module):
        # remove self and variables
        func = module.get_topo
        argc = func.__code__.co_argcount
        tunables = func.__code__.co_varnames[1:argc]

        # fetch defaults
        defaults = func.__defaults__
        if defaults is None:
            return {}
        if len(defaults) != (argc - 1):
            raise TRexError("Module should provide default values for all arguments on get_topo()")

        output = {}
        for t, d in zip(tunables, defaults):
            output[t] = d

        return output


    @staticmethod
    def load_py(python_file, **kw):
        # check filename
        if not os.path.isfile(python_file):
            raise TRexError("File '%s' does not exist" % python_file)

        basedir = os.path.dirname(python_file)
        sys.path.insert(0, basedir)

        try:
            file   = os.path.basename(python_file).split('.')[0]
            module = __import__(file, globals(), locals(), [], 0)
            imp.reload(module) # reload the update

            ASTFTopologyManager.get_module_tunables(module)
            topo = module.get_topo(**kw)
            if not isinstance(topo, ASTFTopology):
                raise Exception('loaded topology type is not ASTFTopology')

            return topo

        #except Exception as e:
        #    raise TRexError('Could not load topology: %s' % e)

        finally:
            sys.path.pop(0)


    @staticmethod
    def from_data(data):
        gws = data.get('gws')
        if not gws:
            gws = []
        elif type(gws) is not list:
            raise TRexError("Type of gws section in JSON must be a list")
        vifs = data.get('vifs')
        if not vifs:
            vifs = []
        elif type(vifs) is not list:
            raise TRexError("Type of vifs section in JSON must be a list")

        topo = ASTFTopology()
        for vif_data in vifs:
            topo.add_vif(**vif_data)
        for gw_data in gws:
            topo.add_gw(**gw_data)
        return topo


    @staticmethod
    def load_json(filename):
        if not os.path.isfile(filename):
            raise TRexError("File '%s' does not exist" % filename)

        # read the content
        with open(filename) as f:
            file_str = f.read()

        data = json.loads(file_str)
        return ASTFTopologyManager.from_data(data)


    @staticmethod
    def load_yaml(filename):
        if not os.path.isfile(filename):
            raise TRexError("File '%s' does not exist" % filename)

        # read the content
        with open(filename) as f:
            file_str = f.read()

        data = yaml.load(file_str)
        return ASTFTopologyManager.from_data(data)


    def clear(self):
        for port in self.client.ports.values():
            port.topo = ASTFTopology()
        if not self.client.handler:
            raise TRexError('Cleared client, but not server (not owned)')
        params = {
            'handler': self.client.handler,
        }
        rc = self.client._transmit('topo_clear', params)
        if not rc:
            raise TRexError('Could not clear topology: %s' % rc.err())


    def resolve(self, ports = None):
        gw_need_resolve_cnt = 0
        services_cnt = 0
        unresolved = []

        if ports is None:
            port_ids = self.client.ports.keys()
        else:
            port_ids = listify(ports)

        for port_id in port_ids:
            port = self.client.get_port(port_id)
            ctx = self.client.create_service_ctx(port_id)
            port_attr = port.get_ts_attr()
            ipv4 = port_attr['layer_cfg']['ipv4']
            port_ipv4 = None if ipv4['state'] == 'none' else ipv4['src']
            port_vlan = port_attr['vlan']['tags']

            vifs = {}
            for vif in port.topo.vifs:
                vifs[vif.port_id] = vif

            service_per_dest = {}
            for gw in port.topo.gws:
                if gw.dst_mac:
                    continue
                gw_need_resolve_cnt += 1
                dst = gw.dst
                if gw.dst_type == DST_IPv4:
                    if gw.sub_if:
                        vif = vifs[gw.port_id]
                        vlan = vif.vlan
                        service_key = '%s - %s' % (dst, [vlan])
                        service = service_per_dest.get(service_key)
                        if service:
                            service.gws.append(gw)
                            continue
                        service = ServiceARP(ctx, dst, vif.src_ipv4, vlan, vif.src_mac, timeout_sec = 1)
                    else:
                        service_key = '%s - %s' % (dst, port_vlan)
                        service = service_per_dest.get(service_key)
                        if service:
                            service.gws.append(gw)
                            continue
                        service = ServiceARP(ctx, dst, port_ipv4, port_vlan, timeout_sec = 1)
                    service.gws = [gw]
                    service_per_dest[service_key] = service
                elif gw.dst_type == DST_IPv6 and not port.has_ipv6():
                    raise TRexError('Not supported')

            if service_per_dest:
                services_cnt += len(service_per_dest)
                services = list(service_per_dest.values())
                ctx.run(services)
                for service in services:
                    record = service.get_record()
                    if record:
                        for gw in service.gws:
                            gw.dst_mac = record.dst_mac
                    else:
                        unresolved.extend(service.gws)

        if unresolved:
            first_unresolved = [gw.dst for gw in unresolved[:5]]
            raise TRexError('Could not resolve %s GWs, first are: %s' % (len(unresolved), first_unresolved))

        if services_cnt:
            status = '%s dest(s) resolved for %s GW(s)' % (services_cnt, gw_need_resolve_cnt)
        else:
            status = 'No need to resolve anything'

        if ports is None:
            print(status + ', uploading to server')
            rc = self.client._upload_fragmented('topo_fragment', self.to_json())
            if not rc:
                raise TRexError('Could not upload topology: %s' % rc.err())
        else:
            print(status)


    def show(self, ports = None):
        if self.is_empty():
            print('Topology is empty!')
            return

        if ports is None:
            port_ids = self.client.ports.keys()
        else:
            port_ids = listify(ports)

        sorted_ports = [self.client.ports[port_id] for port_id in sorted(port_ids)]
        sorted_topo = [port.topo for port in sorted_ports]

        vifs_table = TRexTextTable('Virtual interfaces')
        vifs_table.set_cols_align(['c'] * 5)
        vifs_table.set_cols_dtype(['t'] * 5)
        vifs_table.header(['Port', 'MAC', 'VLAN', 'IPv4', 'IPv6'])
        max_ipv6_len = 5
        max_port_id = 4
        for topo in sorted_topo:
            if not topo.vifs:
                continue
            vifs_dict = dict([(vif.port_id, index) for index, vif in enumerate(topo.vifs)])
            for vif_id in sorted(vifs_dict.keys()):
                vif = topo.vifs[vifs_dict[vif_id]]
                vlan = vif.vlan or '-'
                ipv4 = vif.src_ipv4 or '-'
                ipv6 = vif.src_ipv6 or '-'
                max_ipv6_len = max(max_ipv6_len, len(ipv6))
                max_port_id  = max(max_port_id, len(vif_id))
                vifs_table.add_row([vif_id, vif.src_mac, vlan, ipv4, ipv6])
        vifs_table.set_cols_width([max_port_id, 17, 4, 15, max_ipv6_len])
        print_table_with_header(vifs_table, untouched_header = vifs_table.title)

        gws_table = TRexTextTable('Gateways for traffic')
        gws_table.set_cols_align(['c'] * 5)
        gws_table.set_cols_dtype(['t'] * 5)
        gws_table.header(['Port', 'Range start', 'Range end', 'Dest', 'Resolved'])
        max_dst_len = 5
        max_res_len = 8
        for topo in sorted_topo:
            if topo.gws:
                gws_dict = dict([(ip2int(gw.src_start), index) for index, gw in enumerate(topo.gws)])
                for gw_src_int in sorted(gws_dict.keys()):
                    gw = topo.gws[gws_dict[gw_src_int]]
                    dst = gw.dst
                    dst_mac = gw.dst_mac or '-'
                    max_dst_len = max(max_dst_len, len(dst))
                    max_res_len = max(max_res_len, len(dst_mac))
                    gws_table.add_row([gw.port_id, gw.src_start, gw.src_end, dst, dst_mac])
        gws_table.set_cols_width([5, 15, 15, max_dst_len, max_res_len])
        print_table_with_header(gws_table, untouched_header = gws_table.title)


    def get_merged_data(self, to_server = True):
        gws = []
        vifs = []
        for port in self.client.ports.values():
            gws.extend([gw.get_data(to_server) for gw in port.topo.gws])
            vifs.extend([vif.get_data(to_server) for vif in port.topo.vifs])
        return {'gws': gws, 'vifs': vifs}


    def to_json(self, to_server = True):
        topo = self.get_merged_data(to_server)
        if to_server:
            return json.dumps(topo, sort_keys = True)
        return json.dumps(topo, indent=4, separators=(',', ': '), sort_keys = True)


    def to_yaml(self):
        topo = self.get_merged_data()
        return yaml.dump(topo, default_flow_style=False)


    def to_code(self):
        code = '''# !!! Auto-generated code !!!\n\nfrom trex.astf.topo import *\n\ndef get_topo():\n'''

        gws = []
        vifs = []
        for port in self.client.ports.values():
            gws.extend([gw.to_code() for gw in port.topo.gws])
            vifs.extend([vif.to_code() for vif in port.topo.vifs])

        code += '    vifs = [\n'
        for vif in vifs:
            code += '        %s,\n' % vif
        code += '        ]\n'

        code += '    gws = [\n'
        for gw in gws:
            code += '        %s,\n' % gw
        code += '        ]\n'

        code += '\n    return ASTFTopology(vifs = vifs, gws = gws)\n'
        return code


    def sync_with_server(self):
        rc = self.client._transmit('topo_get')
        if not rc:
            raise TRexError('Could not get topology from server: %s' % rc.err())

        topo_data = rc.data().get('topo_data')
        if topo_data is None:
            raise TRexError('Server response is expected to have "topo_data" key')
        topology = self.from_data(topo_data)
        topo_per_port = self.split_per_port(topology)
        self.validate_topo(topo_per_port)
        for port_id, port in self.client.ports.items():
            port.topo = topo_per_port[port_id]



