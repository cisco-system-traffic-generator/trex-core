import multiprocessing
import queue
import sys
import unittest
import os
from unittest.mock import patch

from wireless.pubsub.pubsub import PubSub
from wireless.trex_wireless_ap import AP
from wireless.trex_wireless_manager import APMode
from wireless.trex_wireless_manager_private import *
from wireless.trex_wireless_rpc_message import *
from wireless.trex_wireless_worker import *
from wireless.trex_wireless_worker_rpc import *

from wireless.trex_wireless_config import *

global config

config = load_config()


class UtilTest(unittest.TestCase):
    """Tests methods for the utils functions of the trex_wireless_worker file."""

    def test_get_ap_per_port(self):
        """Test the get_ap_per_port function."""

        rsa_priv_file, rsa_cert_file = 1, 2  # mocks

        expected = {}  # build the expected dict
        aps = []  # input of function
        for i in range(100):
            # create an AP with port_id = i mod 3
            port_id = i%3
            ap = APInfo(port_id=port_id, ip="2.2.2.2", mac="bb:bb:bb:bb:bb:bb", radio_mac="bb:bb:bb:bb:bb:00", udp_port=12345, wlc_ip='1.1.1.1',
                gateway_ip='1.1.1.2', ap_mode=APMode.LOCAL, rsa_ca_priv_file=None, rsa_priv_file=rsa_priv_file, rsa_cert_file=rsa_cert_file)
            if port_id not in expected:
                expected[port_id] = []
            expected[port_id].append(ap)
            aps.append(ap)

        got = get_ap_per_port(aps)

        # check that expected = got
        self.assertTrue(set(got.keys()) == set(expected.keys()))

        for port_id in got.keys():
            self.assertTrue(port_id in expected)
            got_for_port = set(got[port_id])
            expected_for_port = set(expected[port_id])
            self.assertEqual(got_for_port, expected_for_port)

        
    def test_get_ap_per_port_none(self):
        """Test the get_ap_per_port function for empty input."""
        expected = {}
        got = get_ap_per_port([])
        self.assertEqual(expected, got, "get_ap_per_port([]) should be {{}}, got %s" % got)

@patch('wireless.trex_wireless_ap.AP._create_ssl', lambda x,y: None)
class WirelessWorkerTest(unittest.TestCase):
    """Tests methods for the WirelessWorker class."""

    def setUp(self):
        self.worker_id = 1 # will only test one worker
        self.cmd_pipe_manager, cmd_pipe = multiprocessing.Pipe()  # one end to the 'manager', one to the worker
        self.pkt_pipe_th, pkt_pipe = multiprocessing.Pipe()  # one end to the 'TrafficHandler', one to the worker
        self.port_id = 0
        self.ssl_ctx = SSL_Context()
        global config
        self.worker = WirelessWorker(
            worker_id=self.worker_id,
            cmd_pipe=cmd_pipe,
            pkt_pipe=pkt_pipe,
            port_id=self.port_id,
            config=config,
            ssl_ctx=self.ssl_ctx,
            log_queue=queue.Queue(),
            log_level=logging.NOTSET,
            log_filter=None,
            pubsub=PubSub(),
        )
        self.rsa_priv_file, self.rsa_cert_file = 1, 2 # mocks

        # create some APs
        self.num_aps = 9
        self.aps = []
        for i in range(self.num_aps):
            self.aps.append(APInfo(port_id=1, ip="2.2.2.{}".format(i), mac="bb:bb:bb:bb:b{}:bb".format(i), radio_mac="bb:bb:bb:bb:b{}:00".format(i), udp_port=12345, wlc_ip='1.1.1.1',
                gateway_ip='1.1.1.2', ap_mode=APMode.LOCAL, rsa_ca_priv_file=None, rsa_priv_file=self.rsa_priv_file, rsa_cert_file=self.rsa_cert_file))

    def tearDown(self):
        ...

    def test_wirelessworker_init(self):
        """Test the init() method of the worker, should pass."""
        try:
            self.worker.init()
        except Exception as e:
            self.fail("init() method of WirelessWorker has raised an exception: {}".format(e))

    def test_wirelessworker_create_aps(self):
        """Test the 'create_aps' method on WirelessWorker."""
        self.worker.init()

        self.worker.create_aps(port_layer_cfg=None, aps=self.aps)

        # check for correct stored information
        class LookupTableTestInfo:
            def __init__(self, name, table, attr):
                self.name = name
                self.table = table
                self.attr = attr

        lookup_tables_info = {
            LookupTableTestInfo("ap_by_name", self.worker.ap_by_name, "name"),
            LookupTableTestInfo("ap_by_ip", self.worker.ap_by_ip, "ip"),
            LookupTableTestInfo("ap_by_mac", self.worker.ap_by_mac, "mac"),
            LookupTableTestInfo("ap_by_radio_mac", self.worker.ap_by_radio_mac, "radio_mac"),
        }

        for table_info in lookup_tables_info:
            self.assertEqual(self.num_aps, len(table_info.table), "{} is not constructed correctly, expected {} aps indexed stored got {}".format(table_info.name, self.num_aps, len(table_info.table)))
            for key, ap in table_info.table.items():
                self.assertEqual(key, getattr(ap, table_info.attr), "{} is not indexed properly, key should be {}, got {}".format(ap, getattr(ap, table_info.attr), key))

        # check that the aps have been stored
        self.assertEquals(self.num_aps, len(self.worker.aps), "aps should be stored in aps list, expected {} aps, got {}".format(self.num_aps, len(self.worker.aps)))

    def test_wirelessworker_create_clients(self):
        """Test the 'create_clients' method on WirelessWorker."""
        num_clients_per_ap = 5
        num_aps = 2

        self.worker.init()

        aps = self.aps[:num_aps]
        clients_per_ap_mac = {}
        clients = []
        # create aps and clients
        for i in range(num_aps):
            ap = aps[i]

            clients_per_ap_mac[ap.mac] = []
            for j in range(num_clients_per_ap):
                client = ClientInfo(mac="cc:cc:cc:cc:{}{}:cc".format(i,j), ip="3.3.{}.{}".format(i,j), ap_info=ap)
                clients_per_ap_mac[ap.mac].append(client)
                clients.append(client)

        self.worker.create_clients(clients)

        # check for correct stored information

        # check that all aps received their clients
        for ap in self.worker.aps:
            self.assertEqual(num_clients_per_ap, len(ap.clients), "creating clients should register the clients on each ap, expected {} clients, got {}".format(num_clients_per_ap, len(ap.clients)))
            for client in ap.clients:
                self.assertEqual(client.ap_info.mac, ap.mac, "creating clients should register the clients on correct ap")

        # check that clients are indexed on worker
        for client in self.worker.clients:
            self.assertTrue(client.ip in self.worker.client_by_id, "client ip {} should be indexed on worker".format(client.ip))
            self.assertTrue(client.mac in self.worker.client_by_id, "client mac {} should be indexed on worker".format(client.mac))

    def test_wirelessworker_add_services_non_existing_device(self):
        """Test the 'add_services' method on WirelessWorker
        when asked to add a service on a non_existing device, should raise ValueError."""
        self.worker.init()
        ap = (APInfo(port_id=1, ip="2.2.2.2", mac="bb:bb:bb:bb:bb:bb", radio_mac="bb:bb:bb:bb:bb:00", udp_port=12345, wlc_ip='1.1.1.1',
            gateway_ip='1.1.1.2', ap_mode=APMode.LOCAL, rsa_ca_priv_file=None, rsa_priv_file=self.rsa_priv_file, rsa_cert_file=self.rsa_cert_file))

        self.worker.create_aps(port_layer_cfg=None, aps=[ap])

        with self.assertRaises(ValueError):
            self.worker.add_services(["aa:bb:cc:cc:bb:aa"], "wireless.services.client.client_service_association.ClientServiceAssociation")
    
    def test_wirelessworker_add_services_non_existing_service_class(self):
        """Test the 'add_services' method on WirelessWorker
        when asked to add a service that does not exist in module, should raise ValueError."""
        self.worker.init()
        ap = self.aps[0]
        self.worker.create_aps(port_layer_cfg=None, aps=[ap])

        with self.assertRaises(ValueError):
            self.worker.add_services([ap.mac], "wireless.ClientServiceAssociation")

    def test_wirelessworker_add_services_non_existing_module(self):
        """Test the 'add_services' method on WirelessWorker
        when asked to add a service which module does not exist, should raise ValueError."""
        self.worker.init()
        ap = self.aps[0]
        self.worker.create_aps(port_layer_cfg=None, aps=[ap])

        with self.assertRaises(ValueError):
            self.worker.add_services([ap.mac], "this_module_does_not_exist.ClientServiceAssociation")

    def test_wirelessworker_add_services_on_one_ap(self):
        """Test the 'add_services' method on WirelessWorker
        when asked to add a service on a single ap."""
        self.worker.init()

        ap1, ap2 = self.aps[:2]
        self.worker.create_aps(port_layer_cfg=None, aps=[ap1, ap2])
        ap1 = self.worker._get_ap_by_id(ap1.mac)
        ap2 = self.worker._get_ap_by_id(ap2.mac)

        # adding service on ap1 only
        self.worker.add_services([ap1.mac], "wireless.services.ap.ap_service_dhcp.APServiceDHCP")

        # check that the service is registered on ap1
        self.assertTrue("APServiceDHCP" in ap1.services)
        # check that the sevice is not registered on ap2
        self.assertTrue("APServiceDHCP" not in ap2.services)

    def test_wirelessworker_add_services_on_one_ap(self):
        """Test the 'add_services' method on WirelessWorker
        when asked to add a service on a ap two times, should raise ValueError."""
        self.worker.init()

        ap1 = self.aps[0]

        self.worker.create_aps(port_layer_cfg=None, aps=[ap1])
        ap1 = self.worker._get_ap_by_id(ap1.mac)

        self.worker.add_services([ap1.mac], "wireless.services.ap.ap_service_dhcp.APServiceDHCP")

        # check that the service is registered on ap1
        self.assertTrue("APServiceDHCP" in ap1.services)

        with self.assertRaises(ValueError):
            self.worker.add_services([ap1.mac], "wireless.services.ap.ap_service_dhcp.APServiceDHCP")

    def test_wirelessworker_add_two_services_on_one_client(self):
        """Test the 'add_services' method on WirelessWorker
        when asked to add two different services on one ap, should pass."""
        self.worker.init()

        max_concurrent = 10

        ap1 = self.aps[0]
        self.worker.create_aps(port_layer_cfg=None, aps=[ap1])

        client = ClientInfo(mac="cc:cc:cc:cc:cc:cc", ip="3.3.3.3", ap_info=ap1)

        self.worker.create_clients([client])

        ap1 = self.worker._get_ap_by_id(ap1.mac)
        # set SSID for service
        ap1.create_vap(ssid=b"1", vap_id=1, slot_id=0)
        client = self.worker.device_by_mac("cc:cc:cc:cc:cc:cc")

        try:
            self.worker.add_services(["cc:cc:cc:cc:cc:cc"], "wireless.services.client.client_service_association.ClientServiceAssociation")
            # set max_concurrent 
            self.worker.add_services(["cc:cc:cc:cc:cc:cc"], "wireless.services.client.client_service_dhcp.ClientServiceDHCP")
        except:
            (etype, _, _) = sys.exc_info()
            import traceback
            print(traceback.format_exc())
            self.fail("launching two different services on a device should not fail, got exception %s" % etype)

        # check that the service are registered on client
        self.assertTrue("ClientServiceAssociation" in client.services)
        self.assertTrue("ClientServiceDHCP" in client.services)

    def test_wirelessworker_add_general_service(self):
        """Test the 'add_services' method on WirelessWorker
        when asked to add a GeneralService on a subset of aps."""
        self.worker.init()

        service_class = "wireless.services.unit_tests.general_service_ap_example.GeneralAPServiceExample"
        service_name = "GeneralAPServiceExample"
        
        num_aps = 5
        num_aps_service = 2 # number of aps to run the service
        aps = self.aps[:num_aps]
        service_aps = aps[:num_aps_service]

        self.worker.create_aps(port_layer_cfg=None, aps=aps)
        for i in range(num_aps):
            aps[i] = self.worker._get_ap_by_id(aps[i].mac)
        service_aps = aps[:num_aps_service]

        self.worker.add_services([ap.mac for ap in service_aps], service_class)

        self.assertTrue(service_name in self.worker.services, "added service should be registered on worker")
        service = self.worker.services[service_name]
        # check that the service really launched on specified aps
        self.assertEquals(service.devices, service_aps, "service was not started with given aps, expected: {}, got: {}".format(service_aps, service.devices))

    def test_wirelessworker_stop_general_service(self):
        """Test the 'add_services' method on WirelessWorker
        when asked to add a GeneralService on APs and stopping it."""
        self.test_wirelessworker_add_general_service()
        service_name = "GeneralAPServiceExample"
        self.worker._WirelessWorker__stop_general_service(service_name)
        self.assertTrue(service_name not in self.worker.services)
        self.assertTrue(service_name not in self.worker._WirelessWorker__service_processes)

    def test_wirelessworker_restart_general_service(self):
        """Test the 'add_services' method on WirelessWorker
        when asked to add a GeneralService on APs, stopping it and relaunching it."""
        self.test_wirelessworker_stop_general_service()
        self.test_wirelessworker_add_general_service()

class WirelessWorkerManagementTest(WirelessWorkerTest):
    """Test methods for the Worker management thread."""

    def setUp(self):
        super().setUp()
        self.worker.init()
        self.stop_queue = multiprocessing.Queue()
        self.management = threading.Thread(
            target=self.worker._WirelessWorker__management, args=(self.stop_queue, ), daemon=True)
        self.management.start()
        ...
    
    def tearDown(self):
        super().tearDown()
        ...

    def test_wirelessworker_management_is_on(self):
        """Test the management thread of worker when asked to report its status."""
        self.assertTrue(self.management.is_alive())
        self.cmd_pipe_manager.send(WorkerCall_is_on(self.worker))

        resp = self.cmd_pipe_manager.recv()
        print(resp)
        self.assertEqual(resp.ret, True, "management thread of worker should be launched")
        self.assertEqual(resp.code, RPCResponse.SUCCESS)

    def test_wireless_management_stop(self):
        """Test the management thread of worker when asked to stop."""
        self.cmd_pipe_manager.send(WorkerCall_stop(self.worker))
        self.management.join()

    def test_wireless_management_stop_queue(self):
        """Test the management thread of worker when asked to stop by the worker via the stop_queue."""
        self.stop_queue.put(None)
        self.management.join()


    @patch('wireless.trex_wireless_ap.AP._create_ssl', lambda x,y: None)
    def test_wireless_management_two_blocking_calls(self):
        """Test the management thread of worker when called with two blocking remote calls."""

        num_clients_per_ap = 5
        num_aps = 2

        aps = self.aps[:num_aps]
        clients_per_ap_mac = {}
        clients = []
        # create aps and clients
        for i in range(num_aps):
            ap = aps[i]
            clients_per_ap_mac[ap.mac] = []
            for j in range(num_clients_per_ap):
                client = ClientInfo(mac="cc:cc:cc:cc:{}{}:cc".format(i,j), ip="3.3.{}.{}".format(i,j), ap_info=ap)
                clients_per_ap_mac[ap.mac].append(client)
                clients.append(client)

        self.cmd_pipe_manager.send(WorkerCall_create_aps(self.worker, None, aps))
        self.cmd_pipe_manager.send(WorkerCall_create_clients(self.worker, clients))

        resp = self.cmd_pipe_manager.recv()
        self.assertEqual(resp.code, RPCResponse.SUCCESS)

        resp = self.cmd_pipe_manager.recv()
        self.assertEqual(resp.code, RPCResponse.SUCCESS)


    @patch('wireless.trex_wireless_ap.AP._create_ssl', lambda x,y: None)
    def test_wireless_management_three_calls(self):
        """Test the management thread of worker when called with one blocking call and two thread safe."""
        aps = self.aps[:2]
        # create aps
        self.cmd_pipe_manager.send(WorkerCall_create_aps(self.worker, None, aps))
        self.cmd_pipe_manager.send(WorkerCall_get_ap_states(self.worker))
        self.cmd_pipe_manager.send(WorkerCall_get_ap_states(self.worker))

        r1 = self.cmd_pipe_manager.recv()
        r2 = self.cmd_pipe_manager.recv()
        r3 = self.cmd_pipe_manager.recv()
        self.assertEqual(r1.code, RPCResponse.SUCCESS)
        self.assertEqual(r2.code, RPCResponse.SUCCESS)
        self.assertEqual(r3.code, RPCResponse.SUCCESS)

class WirelessWorkerTrafficTest(WirelessWorkerTest):
    """Test methods for the Worker traffic handler thread."""

    def setUp(self):
        super().setUp()
        self.worker.init()
        self.stop_queue = multiprocessing.Queue()
        self.th = threading.Thread(
            target=self.worker._WirelessWorker__worker_traffic_handler, args=(self.stop_queue, ), daemon=True)
        self.th.start()
        ...
    
    def tearDown(self):
        super().tearDown()
        ...

    def test_wireless_worker_traffic_handler_stop_queue(self):
        """Test the worker traffic handler thread of worker when asked to stop by the worker via the stop_queue."""
        self.stop_queue.put(None)
        self.th.join()

    def test_wireless_worker_traffic_handler_rubbish_packet(self):
        """Test the worker traffic handler thread when sent a bad packet."""
        self.pkt_pipe_th.send(b'010001010100111111110110010100101')
        self.assertTrue(self.th.is_alive)
        
