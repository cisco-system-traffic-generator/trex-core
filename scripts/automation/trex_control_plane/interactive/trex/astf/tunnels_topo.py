import imp
import json
import yaml
import os
import sys
from ..common.trex_exceptions import *
from ..utils.common import *
from ..utils.text_tables import TRexTextTable, print_table_with_header
from ..utils import parsing_opts

class TopoTunnelLatency(object):
    def __init__(self, client_port_id, client_ip, server_ip):
        if client_port_id % 2:
            raise TRexError("client_port_id must be even number")
        sides = ["client_ip", "server_ip"]
        for idx, ip in enumerate([client_ip, server_ip]):
            if not is_valid_ipv4(ip):
                raise TRexError("Illegal IPv4 addr: %s = %s" % (sides[idx], ip))
        self.client_port_id = client_port_id
        self.client_ip      = client_ip
        self.server_ip      = server_ip

    def get_data(self):
        d = {}
        d["client_port_id"]   = self.client_port_id
        d["client_ip"]        = self.client_ip
        d["server_ip"]        = self.server_ip

        return d


class TopoTunnelCtx(object):
    def __init__(self, src_start, src_end, initial_teid, teid_jump, sport, version, tunnel_type, src_ip, dst_ip, activate):
        if version == 4 and (not is_valid_ipv4(src_ip) or not is_valid_ipv4(dst_ip)):
            raise TRexError('src_ip and dst_ip are not a valid IPv4 addresses: %s, %s' % (src_ip, dst_ip))
        elif version == 6 and (not is_valid_ipv6(src_ip) or not is_valid_ipv6(dst_ip)):
            raise TRexError('src_ip and dst_ip are not a valid IPv6 addresses: %s, %s' % (src_ip, dst_ip))
        if not is_valid_ipv4(src_start) or not is_valid_ipv4(src_end):
            raise TRexError('src_start and src_end are not a valid IPv4 addresses: %s, %s' % (src_start, src_end))
        fields = [initial_teid, teid_jump, sport, version, tunnel_type]
        fields_str = ["initial_teid", 'teid_jump', "sport", "version", "tunnel_type"]
        for idx, val in enumerate(fields):
            if type(val) is not int:
                raise TRexError("The type of '%s' field should be int" % (fields_str[idx]))

        if type(activate) is not bool:
            raise TRexError("The type of 'activate' field should be bool" % (fields_str[idx]))

        self.src_start = src_start
        self.src_end = src_end
        self.initial_teid = initial_teid
        self.teid_jump = teid_jump
        self.sport = sport
        self.version = version
        self.tunnel_type = tunnel_type
        self.src_ip = src_ip
        self.dst_ip = dst_ip
        self.activate = activate


    def get_data(self):
        d = {}
        d['src_start']            = self.src_start
        d['src_end']              = self.src_end
        d['initial_teid']         = self.initial_teid
        d['teid_jump']            = self.teid_jump
        d['sport']                = self.sport
        d['version']              = self.version
        d['tunnel_type']          = self.tunnel_type
        d['src_ip']               = self.src_ip
        d['dst_ip']               = self.dst_ip
        d['activate']             = self.activate
        return d


class TunnelsTopo(object):
    def __init__(self, client = None, tunnel_ctxs = None, tunnel_latency = None):
        self.client = client
        self.tunnel_ctxs = tunnel_ctxs or []
        self.tunnel_latency = tunnel_latency or []


    def add_tunnel_ctx_obj(self, tunnel_ctx):
        ''' Add tunnel context object '''
        validate_type('tunnel_ctx', tunnel_ctx, TopoTunnelCtx)
        self.tunnel_ctxs.append(tunnel_ctx)


    def add_tunnel_ctx(self, *a, **k):
        ''' Add tunnel context '''
        tunnel_ctx = TopoTunnelCtx(*a, **k)
        self.tunnel_ctxs.append(tunnel_ctx)


    def add_tunnel_latency_obj(self, tunnel_latency):
        ''' Add latency context object '''
        validate_type('tunnel_latency', tunnel_latency, TopoTunnelLatency)
        self.tunnel_latency.append(tunnel_latency)


    def add_tunnel_latency(self, *a, **k):
        ''' Add latency context '''
        tunnel_latency = TopoTunnelLatency(*a, **k)
        self.tunnel_latency.append(tunnel_latency)


    def is_empty(self):
        ''' Return True if nothing is added '''
        return (len(self.tunnel_ctxs) + len(self.tunnel_latency)) == 0


    def info(self, msg=None, rc=None):
        if msg:
            self.client.logger.pre_cmd(msg +"\n")
        if rc:
            self.client.logger.post_cmd(rc)

    def sync_with_server(self):
        rc = self.client._transmit('tunnel_topo_get')
        if not rc:
            raise TRexError('Could not get tunnel topology from server: %s' % rc.err())

        topo_data = rc.data().get('tunnel_topo_data')
        if topo_data is None:
            raise TRexError('Server response is expected to have "tunnel_topo_data" key')

        topo_data = json.loads(topo_data)
        topology = self.from_data(topo_data)
        self.tunnel_ctxs = topology.tunnel_ctxs
        self.tunnel_latency = topology.tunnel_latency


    def validate_topo(self):
        start_end_tc = []
        for tunnel_ctx in self.tunnel_ctxs:
            start = tuple(map(int, tunnel_ctx.src_start.split('.')))
            end   = tuple(map(int, tunnel_ctx.src_end.split('.')))
            if start > end:
                raise TRexError('Tunnel context src_start: %s is higher than src_end: %s' % (tunnel_ctx.src_start, tunnel_ctx.src_end))
            start_end_tc.append((start, end, tunnel_ctx))

        start_end_tc.sort(key = lambda t:t[0]) # N*log(N)
        p_tunnel_ctx = None
        for start, end, tunnel_ctx in start_end_tc:
            if p_tunnel_ctx:
                if start == p_start:
                    raise TRexError('At least two Tunnel contexts start range with: %s' % (p_tunnel_ctx.src_start))
                if p_end >= start:
                    raise TRexError('Tunnel context ranges intersect: %s-%s, %s-%s' % (p_tunnel_ctx.src_start, p_tunnel_ctx.src_end, p_tunnel_ctx.src_start, p_tunnel_ctx.src_end))
            p_start, p_end, p_tunnel_ctx = start, end, tunnel_ctx

        #validate latency
        port_set = set()
        if len(self.tunnel_latency) and not len(self.tunnel_ctxs):
            raise TRexError('There is no tunnel contexts')
        num_dual_ports = int(len(self.client.get_all_ports()) / 2)
        max_port_id = len(self.client.get_all_ports()) - 1
        if len(self.tunnel_latency) and num_dual_ports != len(self.tunnel_latency):
            raise TRexError("The amount of latency clients: %s must be equal to the num of dual ports: %s" %(len(self.tunnel_latency), num_dual_ports))
        for latency in self.tunnel_latency:
            if latency.client_port_id > max_port_id:
                raise TRexError("client_port_id: %s exceeds max_port_id: %s" % (latency.client_port_id, max_port_id))
            if latency.client_port_id in port_set:
                raise TRexError("At least two instances of client_port_id %s" % (latency.client_port_id))
            port_set.add(latency.client_port_id)
            matched = False
            client_ip = tuple(map(int, latency.client_ip.split('.')))
            for start, end, _ in start_end_tc:
                if client_ip >= start and client_ip <= end:
                    matched = True
                    break
            if not matched:
                raise TRexError('There is no tunnel context for the client_ip : %s' % (latency.client_ip))


    def load(self, topology, **kw):
        self.sync_with_server()
        if not self.is_empty():
            raise TRexError('Tunnels topology is already loaded, clear it first')

        if not isinstance(topology, TunnelsTopo):
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

        self.tunnel_ctxs = topology.tunnel_ctxs
        self.tunnel_latency = topology.tunnel_latency
        self.validate_topo()

        if self.is_empty():
            raise TRexError('Loaded topology is empty!')

        self.info(msg = 'Uploading to server...')
        rc = self.client._upload_fragmented('tunnel_topo_fragment', self.to_json())
        self.info(rc = rc)
        if not rc:
            raise TRexError('Could not upload topology: %s' % rc.err())


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

            topo = module.get_topo(**kw)
            if not isinstance(topo, TunnelsTopo):
                raise TRexError('Loaded topology type is not ASTFTopology')

            return topo

        except Exception as e:
            raise TRexError('Could not load topology: %s' % e)

        finally:
            sys.path.pop(0)


    @staticmethod
    def from_data(data):
        tunnels = data.get('tunnels', [])
        latency = data.get('latency', [])

        if type(tunnels) is not list:
            raise TRexError("Type of tunnels section in JSON must be a list")
        if type(latency) is not list:
            raise TRexError("Type of latency section in JSON must be a list")

        topo = TunnelsTopo()
        for tunnel in tunnels:
            topo.add_tunnel_ctx(**tunnel)
        for l in latency:
            topo.add_tunnel_latency(**l)
        return topo


    @staticmethod
    def load_json(filename):
        if not os.path.isfile(filename):
            raise TRexError("File '%s' does not exist" % filename)

        # read the content
        with open(filename) as f:
            file_str = f.read()

        data = json.loads(file_str)
        return TunnelsTopo.from_data(data)


    @staticmethod
    def load_yaml(filename):
        if not os.path.isfile(filename):
            raise TRexError("File '%s' does not exist" % filename)

        # read the content
        with open(filename) as f:
            file_str = f.read()

        data = yaml.load(file_str)
        return TunnelsTopo.from_data(data)


    def clear(self):
        self.tunnel_ctxs = []
        self.tunnel_latency = []
        if not self.client.handler:
            raise TRexError('Cleared client, but not server (not owned)')
        params = {
            'handler': self.client.handler,
        }
        self.info(msg = "Clearing tunnels topo...")
        rc = self.client._transmit('tunnel_topo_clear', params)
        self.info(rc = rc)
        if not rc:
            raise TRexError('Could not clear topology: %s' % rc.err())


    def show(self):
        self.sync_with_server()
        if self.is_empty():
            self.info(msg = 'Topology is empty!')
            return

        tunnel_table = TRexTextTable('Tunnel Topology')
        tunnel_table.set_cols_align(['c'] * 10)
        tunnel_table.set_cols_dtype(['t'] * 10)
        tunnel_table.header(['Src_start', 'Src_end', 'Initial-Teid', 'Teid-jump', 'Sport', 'Version', 'Type', 'Src_ip', 'Dest_ip', 'Activate'])

        tunnel_dict = dict([(ip2int(tunnel.src_start), index) for index, tunnel in enumerate(self.tunnel_ctxs)])
        max_src_ip = len("Src_ip")
        max_dst_ip = len("Dst_ip")
        max_start_ip = len("Src_start")
        max_src_end = len("Src_end")
        max_type = len("Type")
        for tunnel_src_int in sorted(tunnel_dict.keys()):
            tunnel_ctx = self.tunnel_ctxs[tunnel_dict[tunnel_src_int]]
            tunnel_type = parsing_opts.get_tunnel_type_str(tunnel_ctx.tunnel_type)
            #updating columns width
            max_src_ip = max(max_src_ip, len(tunnel_ctx.src_ip))
            max_dst_ip = max(max_dst_ip, len(tunnel_ctx.dst_ip))
            max_start_ip = max(max_start_ip, len(tunnel_ctx.src_start))
            max_src_end = max(max_src_end, len(tunnel_ctx.src_end))
            max_type = max(max_type, len(tunnel_type))
            #adding a new row
            tunnel_table.add_row([tunnel_ctx.src_start, tunnel_ctx.src_end, tunnel_ctx.initial_teid, tunnel_ctx.teid_jump, tunnel_ctx.sport, tunnel_ctx.version, tunnel_type, tunnel_ctx.src_ip, tunnel_ctx.dst_ip, tunnel_ctx.activate])


        tunnel_table.set_cols_width([max_start_ip, max_src_end, 13, 9, 5, 7, 5, max_src_ip, max_dst_ip, 8])
        print_table_with_header(tunnel_table, untouched_header = tunnel_table.title, buffer = sys.stdout)

        latency_table = TRexTextTable('Tunnel Latency')
        latency_table.set_cols_align(['c'] * 3)
        latency_table.set_cols_dtype(['t'] * 3)
        latency_table.header(['Client_port_id', 'Client_ip', 'Server_ip'])
        max_start_ip = max(max_start_ip, len("client_ip"))
        max_server_ip = len("Server_ip")
        port_len = len("Client_port_id")
        for l in self.tunnel_latency:
            max_server_ip = max(max_server_ip, len(l.server_ip))
            latency_table.add_row([l.client_port_id, l.client_ip, l.server_ip])
        latency_table.set_cols_width([port_len, max_start_ip, max_server_ip])
        print_table_with_header(latency_table, untouched_header = latency_table.title, buffer = sys.stdout)

    def get_data(self):
        tunnel_ctxs = []
        tunnel_ctxs.extend([tunnel_ctx.get_data() for tunnel_ctx in self.tunnel_ctxs])
        latency = []
        latency.extend([tunnel_latency.get_data() for tunnel_latency in self.tunnel_latency])
        return {'tunnels' : tunnel_ctxs, 'latency' : latency}


    def to_json(self):
        topo = self.get_data()
        return json.dumps(topo, sort_keys = True)