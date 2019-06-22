#!/router/bin/python
from .stl_general_test import CStlGeneral_Test, CTRexScenario
from nose.plugins.attrib import attr
from nose.tools import assert_raises
import time
import os
from trex.common.trex_exceptions import TRexError

@attr('wlc')
class CTRexWLC_Test(CStlGeneral_Test):
    """This class tests TRex WLC related code"""

    def get_pkts(self):
        stats = self.client.get_stats()
        return stats[0]['opackets'], stats[1]['ipackets']


    def clear_stats(self):
        self.client.clear_stats(clear_flow_stats = False, clear_latency_stats = False, clear_xstats = False)


    def test_basic_wlc(self):
        ''' Joins 1 AP, 1 client, sends traffic '''

        ap_count = 1
        client_count = 1

        self.start_trex()
        self.connect()
        if self.elk:
            self.update_elk_obj()
        self.client = CTRexScenario.stl_trex
        from trex.stl.trex_stl_wlc import AP_Manager
        ap_manager = AP_Manager(self.client)

        base_data = CTRexScenario.config_dict['base']
        ap_manager.set_base_values(
            mac = base_data['ap_mac'],
            ip = base_data['ap_ip'],
            client_mac = base_data['client_mac'],
            client_ip = base_data['client_ip'],
            )

        self.client.acquire([0, 1], force = True)
        ap_manager.init(0)
        for _ in range(ap_count):
            ap_params = ap_manager._gen_ap_params()
            ap_manager.create_ap(0, *ap_params)
            for _ in range(client_count):
                client_params = ap_manager._gen_client_params()
                ap_manager.create_client(*client_params, ap_id = ap_params[0])

        try:
            with assert_raises(AssertionError): # nothing is emulated yet
                ap_manager.enable_proxy_mode(wired_port = 0, wireless_port = 1)

            start_time = time.time()
            print('Joining APs')
            ap_manager.join_aps()
            print('Took: %gs' % round(time.time() - start_time, 2))
            assert ap_manager.aps[0].is_connected == True

            start_time = time.time()
            print('Re-joining APs')

            ap_manager.disconnect_aps()
            assert ap_manager.aps[0].is_connected == False

            ap_manager.join_aps()
            assert ap_manager.aps[0].is_connected == True
            print('Took: %gs' % round(time.time() - start_time, 2))

            with assert_raises(AssertionError): # clients are not emulated yet
                ap_manager.enable_proxy_mode(wired_port = 0, wireless_port = 1)

            start_time = time.time()
            print('Associating clients')
            ap_manager.join_clients()
            print('Took: %gs' % round(time.time() - start_time, 2))

            start_time = time.time()
            print('Enable/disable capwap proxy')

            self.client.set_service_mode(1, False)
            with assert_raises(TRexError): # port 1 not in service mode
                ap_manager.enable_proxy_mode(wired_port = 0, wireless_port = 1)

            self.client.set_service_mode(1, True)
            ap_manager.enable_proxy_mode(wired_port = 0, wireless_port = 1)

            with assert_raises(TRexError): # proxy already enabled
                ap_manager.enable_proxy_mode(wired_port = 0, wireless_port = 1)

            with assert_raises(TRexError): # proxy is enabled
                self.client.set_service_mode(0, False)

            ap_manager.disable_proxy_mode(ports = [0, 1])

            with assert_raises(TRexError): # proxy already active
                ap_manager.disable_proxy_mode(ports = [0, 1])
            print('Took: %gs' % round(time.time() - start_time, 2))

            print('Adding streams')
            profile = os.path.join(CTRexScenario.scripts_path, 'stl', 'imix_wlc.py')
            for client in ap_manager.clients:
                ap_manager.add_profile(client, profile, src = client.ip, dst = '48.0.0.1')

            duration = 10
            print('Starting traffic for %s sec' % duration)
            self.clear_stats()
            self.client.start(ports = [0], mult = '10kpps', force = True)
            time.sleep(duration)
            self.client.stop()
            opackets, ipackets = self.get_pkts()
            print('Sent: %s, received: %s' % (opackets, ipackets))

            if opackets * 2 < duration * 10000:
                self.fail('Too few output packets')
            if ipackets * 2 < opackets:
                self.fail('Too few input packets')

        finally:
            ap_manager.close()




