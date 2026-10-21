"""Native RC endpoint schema and client RPC contract regressions.

Copyright (c) 2026 Cisco Systems, Inc. Licensed under the Apache License, Version 2.0.
"""

import copy
import json
import math
from types import SimpleNamespace
import unittest
from unittest import mock

from trex.astf.api import (
    ASTFClient, ASTFRoceV2Connection, ASTFRoceV2Profile, ASTFRoceV2QP,
    ASTFRoceV2Receive, ASTFRoceV2Work, ASTFRoceV2Host, ASTFRoceV2Device, ASTFRoceV2Scale,
)
from trex.common.trex_exceptions import TRexError
from trex.common.trex_types import RC_ERR, RC_OK


def endpoint_qp(**options):
    values = dict(port=0, local_qpn=2, remote_qpn=3,
                  src_mac='02:00:00:00:00:01', dst_mac='02:00:00:00:00:02',
                  src_ip='192.0.2.1', dst_ip='192.0.2.2', message_size=16)
    values.update(options)
    return ASTFRoceV2QP(**values)


class RoCEv2EndpointSchemaTest(unittest.TestCase):
    def test_mixed_program_serializes_every_baseline_operation(self):
        operations = [
            ASTFRoceV2Work('send', repeat=2, expected_seed=13),
            ASTFRoceV2Work('send_imm', immediate_data=0xaabbccdd),
            ASTFRoceV2Work('send_inv', invalidate_rkey=0x12345678),
            ASTFRoceV2Work('write', length=8, local_offset=8, remote_offset=48),
            ASTFRoceV2Work('write_imm', immediate_data=0xffffffff),
            ASTFRoceV2Work('read', fenced=True),
            ASTFRoceV2Work('compare_swap', compare=0x1122334455667788,
                          swap_add=0x8877665544332211, expected_original=7),
            ASTFRoceV2Work('fetch_add', swap_add=3, expected_original=0xffffffffffffffff),
        ]
        receives = [ASTFRoceV2Receive(10, [dict(address=0x100020, length=8, lkey=9),
                                           dict(address=0x100028, length=8, lkey=9)])]
        qp = endpoint_qp(memory_size=64, lkey=9, program=operations, receives=receives)
        data = ASTFRoceV2Profile([qp], generation=7, duration=5).to_json()
        self.assertEqual(data['schema_version'], 2)
        self.assertEqual(data['generation'], 7)
        self.assertEqual(data['qps'][0]['dscp'], 26)
        program = data['qps'][0]['program']
        self.assertEqual([entry['operation'] for entry in program], [
            'send', 'send_imm', 'send_inv', 'write', 'write_imm', 'read',
            'compare_swap', 'fetch_add'])
        self.assertEqual(program[0]['repeat'], 2)
        self.assertEqual(program[0]['expected_seed'], 13)
        self.assertEqual(program[1]['immediate_data'], 0xaabbccdd)
        self.assertEqual(program[2]['invalidate_rkey'], 0x12345678)
        self.assertEqual(program[3]['remote_offset'], 48)
        self.assertTrue(program[5]['fenced'])
        self.assertEqual(program[6]['length'], 8)
        self.assertEqual(program[6]['compare'], 0x1122334455667788)
        self.assertEqual(program[7]['expected_original'], 0xffffffffffffffff)
        self.assertEqual(data['qps'][0]['receives'], [receives[0].to_json()])
        self.assertEqual(json.loads(json.dumps(data)), data)

    def test_default_offsets_follow_source_result_and_remote_buffer_layout(self):
        qp = endpoint_qp(program=[ASTFRoceV2Work('send'), ASTFRoceV2Work('write'),
                                  ASTFRoceV2Work('read'), ASTFRoceV2Work('fetch_add')])
        steps = qp.to_json()['program']
        self.assertEqual([(s['local_offset'], s['remote_offset'], s['length']) for s in steps],
                         [(0, 0, 16), (0, 16, 16), (16, 0, 16), (16, 16, 8)])

    def test_nested_constructor_dictionaries_roundtrip_without_mutating_inputs(self):
        receive = dict(wr_id=0xffffffffffffffff,
                       sges=[dict(address=0x100010, length=16, lkey=1)])
        program = [dict(operation='send_imm', immediate_data=42)]
        options = dict(program=program, receives=[receive])
        original = copy.deepcopy(options)
        qp = endpoint_qp(**options)
        data = ASTFRoceV2Profile([qp.to_json()]).to_json()
        self.assertEqual(options, original)
        data['qps'][0]['program'][0]['immediate_data'] = 99
        data['qps'][0]['receives'][0]['sges'][0]['length'] = 0
        self.assertEqual(qp.to_json()['program'][0]['immediate_data'], 42)
        self.assertEqual(qp.to_json()['receives'][0]['sges'][0]['length'], 16)

    def test_receive_descriptor_keeps_wrid_sge_order_and_copy_isolation(self):
        sges = [dict(address=0x100010, length=3, lkey=1),
                dict(address=0x100020, length=5, lkey=1)]
        receive = ASTFRoceV2Receive(0xffffffffffffffff, sges)
        sges[0]['length'] = 999
        values = receive.to_json()
        self.assertEqual(values['wr_id'], 0xffffffffffffffff)
        self.assertEqual([entry['length'] for entry in values['sges']], [3, 5])
        values['sges'].reverse()
        self.assertEqual(receive.to_json()['sges'][0]['length'], 3)

    def test_receive_rejects_empty_excess_malformed_and_overflowing_sges(self):
        invalid = [[], [{}], [1], [dict(address=1, length=2, lkey=0)],
                   [dict(address=True, length=2, lkey=1)],
                   [dict(address=1, length=-1, lkey=1)],
                   [dict(address=2**64 - 1, length=1, lkey=1)],
                   [dict(address=1, length=1, lkey=1, extra=0)],
                   [dict(address=1, length=1, lkey=1)] * 17]
        for sges in invalid:
            with self.subTest(sges=sges), self.assertRaises(TRexError):
                ASTFRoceV2Receive(1, sges)
        for wr_id in (-1, 2**64, True):
            with self.subTest(wr_id=wr_id), self.assertRaises(TRexError):
                ASTFRoceV2Receive(wr_id, [dict(address=1, length=1, lkey=1)])
        sixteen = [dict(address=0x100010 + offset, length=1, lkey=1) for offset in range(16)]
        self.assertEqual(len(ASTFRoceV2Receive(1, sixteen).to_json()['sges']), 16)

    def test_receive_qp_rejects_duplicate_wrid_wrong_key_and_out_of_window(self):
        good = ASTFRoceV2Receive(1, [dict(address=0x100010, length=16, lkey=1)])
        bad_key = ASTFRoceV2Receive(2, [dict(address=0x100010, length=16, lkey=2)])
        too_long = ASTFRoceV2Receive(3, [dict(address=0x100010, length=17, lkey=1)])
        before = ASTFRoceV2Receive(4, [dict(address=0x0fffff, length=1, lkey=1)])
        for receives in ([good, good], [bad_key], [too_long], [before]):
            with self.subTest(receives=receives), self.assertRaises(TRexError):
                endpoint_qp(receives=receives)

    def test_work_rejects_unknown_operations_flags_and_integer_overflows(self):
        invalid = [dict(operation='atomic_write'), dict(operation='send', repeat=0),
                   dict(operation='send', repeat=True), dict(operation='read', fenced=1),
                   dict(operation='send_imm', immediate_data=2**32),
                   dict(operation='send_inv', invalidate_rkey=0),
                   dict(operation='fetch_add', swap_add=2**64),
                   dict(operation='compare_swap', compare=-1),
                   dict(operation='fetch_add', length=7),
                   dict(operation='read', local_offset=2**64),
                   dict(operation='fetch_add', expected_original=2**64),
                   dict(operation='send', expected_seed=2**32)]
        for values in invalid:
            with self.subTest(values=values), self.assertRaises(TRexError):
                ASTFRoceV2Work(**values)

    def test_program_rejects_outside_memory_and_unaligned_atomic_destination(self):
        for step in (dict(operation='write', local_offset=17),
                     dict(operation='read', remote_offset=17),
                     dict(operation='fetch_add', remote_offset=1),
                     dict(operation='compare_swap', local_offset=25)):
            with self.subTest(step=step), self.assertRaises(TRexError):
                endpoint_qp(program=[step])
        with self.assertRaises(TRexError):
            endpoint_qp(remote_address=0x100001, program=[dict(operation='fetch_add')])

    def test_legacy_single_operation_checks_atomic_destination_alignment(self):
        for operation in ('fetch_add', 'compare_swap'):
            with self.subTest(operation=operation), self.assertRaises(TRexError):
                endpoint_qp(operation=operation, message_size=15, memory_size=32)

    def test_atomic_work_always_serializes_eight_bytes(self):
        for operation in ('fetch_add', 'compare_swap'):
            self.assertEqual(ASTFRoceV2Work(operation).to_json()['length'], 8)
        self.assertNotIn('expected_seed', ASTFRoceV2Work('send').to_json())

    def test_automatic_receives_cannot_alias_explicit_receive_ids(self):
        descriptor = ASTFRoceV2Receive(1, [dict(address=0x100010, length=16, lkey=9)])
        with self.assertRaises(TRexError):
            endpoint_qp(lkey=9, memory_size=48, receive_depth=1, receives=[descriptor])
        self.assertEqual(endpoint_qp(lkey=9, receive_depth=1).to_json()['lkey'], 9)

    def test_program_and_receive_count_limits(self):
        for program in ([], [1], [dict(operation='send')] * 4097, 'send'):
            with self.subTest(program_type=type(program)), self.assertRaises(TRexError):
                endpoint_qp(program=program)
        for receives in ([1], [dict(wr_id=1, sges=[])] * 1025, 'receive'):
            with self.subTest(receives_type=type(receives)), self.assertRaises(TRexError):
                endpoint_qp(receives=receives)

    def test_qp_rejects_memory_overflow_bad_addresses_and_receive_budget(self):
        for values in (dict(local_address=2**64 - 16), dict(remote_address=2**64 - 16),
                       dict(memory_size=31), dict(memory_id=0), dict(local_qpn=1),
                       dict(local_qpn=True), dict(src_ip='::1'), dict(dst_mac='invalid'),
                       dict(receive_depth=2), dict(memory_size=64 * 1024 * 1024 + 1)):
            with self.subTest(values=values), self.assertRaises(TRexError):
                endpoint_qp(**values)
        self.assertEqual(endpoint_qp(receive_depth=2, memory_size=48).to_json()['receive_depth'], 2)

    def test_profile_rejects_duplicate_identities_and_memory_budget(self):
        qp = endpoint_qp()
        with self.assertRaises(TRexError):
            ASTFRoceV2Profile([qp, qp])
        with self.assertRaises(TRexError):
            ASTFRoceV2Profile([qp, endpoint_qp(local_qpn=4)], max_memory_bytes=63)
        different_ports = ASTFRoceV2Profile([qp, endpoint_qp(port=1)], max_memory_bytes=64)
        self.assertEqual(len(different_ports.to_json()['qps']), 2)

    def test_shared_memory_id_counts_one_region_and_rejects_conflicting_configuration(self):
        first = endpoint_qp(memory_id=7)
        second = endpoint_qp(local_qpn=4, memory_id=7, operation='read')
        shared = ASTFRoceV2Profile([first, second], max_memory_bytes=32).to_json()
        self.assertEqual([qp['memory_id'] for qp in shared['qps']], [7, 7])
        for changes in (dict(seed=2), dict(lkey=2), dict(local_rkey=2),
                        dict(protection_domain=8), dict(port=1), dict(local_address=0x200000)):
            with self.subTest(changes=changes), self.assertRaises(TRexError):
                ASTFRoceV2Profile([first, endpoint_qp(local_qpn=4, memory_id=7, **changes)])

    def test_default_protection_domain_is_derived_by_server_from_memory_identity(self):
        self.assertNotIn('protection_domain', endpoint_qp().to_json())
        self.assertNotIn('protection_domain', endpoint_qp(memory_id=7).to_json())
        self.assertEqual(endpoint_qp(memory_id=7, protection_domain=3).to_json()['protection_domain'], 3)
        with self.assertRaises(TRexError):
            endpoint_qp(protection_domain=0)

    def test_gateway_prefix_fields_normalize_and_reject_bad_family_or_bounds(self):
        ipv4 = endpoint_qp(prefix_length=24, gateway='192.0.2.254').to_json()
        self.assertEqual((ipv4['prefix_length'], ipv4['gateway']), (24, '192.0.2.254'))
        ipv6 = endpoint_qp(src_ip='2001:db8::1', dst_ip='2001:db8::2',
                           prefix_length=64, gateway='2001:db8:0:0::ffff').to_json()
        self.assertEqual((ipv6['prefix_length'], ipv6['gateway']), (64, '2001:db8::ffff'))
        for values in (dict(prefix_length=33), dict(prefix_length=-1), dict(prefix_length=True),
                       dict(gateway='::1'), dict(gateway='invalid'),
                       dict(src_ip='::1', dst_ip='::2', prefix_length=129),
                       dict(src_ip='::1', dst_ip='::2', gateway='192.0.2.1')):
            with self.subTest(values=values), self.assertRaises(TRexError):
                endpoint_qp(**values)

    def test_profile_rejects_unbounded_or_nonfinite_sessions(self):
        for duration in (0, -1, math.inf, math.nan, True, 3601):
            with self.subTest(duration=duration), self.assertRaises(TRexError):
                ASTFRoceV2Profile([endpoint_qp()], duration=duration)
        for values in (dict(generation=0), dict(generation=2**64),
                       dict(ingress_queue_size=0), dict(max_memory_bytes=256 * 1024 * 1024 + 1)):
            with self.subTest(values=values), self.assertRaises(TRexError):
                ASTFRoceV2Profile([endpoint_qp()], **values)
        for qps in ([], [None], [endpoint_qp()] * 4097):
            with self.subTest(qp_count=len(qps)), self.assertRaises(TRexError):
                ASTFRoceV2Profile(qps)

    def test_cm_active_passive_policies_serialize_without_changing_static_qps(self):
        active = ASTFRoceV2Connection('active', 0x01061234,
                                     source_port=50000, destination_port=4660)
        passive = ASTFRoceV2Connection('passive', 0x01061234, backlog=64)
        profile = ASTFRoceV2Profile([
            endpoint_qp(connection=active),
            endpoint_qp(port=1, connection=passive.to_json())])
        policies = [qp['connection'] for qp in profile.to_json()['qps']]
        self.assertEqual(policies, [active.to_json(), passive.to_json()])
        self.assertEqual(policies[0]['destination_port'], 4660)
        self.assertEqual(policies[1]['backlog'], 64)
        self.assertNotIn('connection', endpoint_qp().to_json())
        policies[0]['mode'] = 'passive'
        self.assertEqual(active.to_json()['mode'], 'active')

    def test_cm_rejects_invalid_modes_ids_ports_and_backlogs(self):
        invalid = [dict(mode='automatic'), dict(service_id=-1), dict(service_id=0), dict(service_id=2**64),
                   dict(service_id=True), dict(source_port=-1), dict(source_port=65536),
                   dict(destination_port=True), dict(destination_port=65536),
                   dict(service_id=1, destination_port=4660),
                   dict(backlog=0), dict(backlog=4097)]
        for options in invalid:
            values = dict(mode='active', service_id=1)
            values.update(options)
            with self.subTest(values=values), self.assertRaises(TRexError):
                ASTFRoceV2Connection(**values)
        for connection in (None, [], {}, {'mode': 'active'},
                           dict(mode='active', service_id=1, unknown=True)):
            with self.subTest(connection=connection), self.assertRaises(TRexError):
                endpoint_qp(connection=connection)

    def test_cm_private_data_limits_and_ip_cma_inference(self):
        active_ip = ASTFRoceV2Connection('active', 0x01061234, source_port=50000,
            destination_port=4660, private_data=b'\x00\x80\xff', auto_accept=False).to_json()
        self.assertTrue(active_ip['ip_cma'])
        self.assertFalse(active_ip['auto_accept'])
        self.assertEqual(active_ip['private_data'], '0080ff')
        passive_ip = ASTFRoceV2Connection('passive', 0x01061234,
            destination_port=4660, private_data='ABCD').to_json()
        self.assertTrue(passive_ip['ip_cma'])
        self.assertEqual(passive_ip['private_data'], 'abcd')
        for mode, ip_cma, maximum in (('active', True, 56), ('active', False, 92),
                                       ('passive', True, 196), ('passive', False, 196)):
            options = dict(mode=mode, service_id=1, ip_cma=ip_cma,
                           source_port=50000 if mode == 'active' and ip_cma else 0)
            values = ASTFRoceV2Connection(private_data=b'x' * maximum, **options).to_json()
            self.assertEqual(len(values['private_data']), maximum * 2)
            with self.subTest(mode=mode, ip_cma=ip_cma), self.assertRaises(TRexError):
                ASTFRoceV2Connection(private_data=b'x' * (maximum + 1), **options)
        self.assertFalse(ASTFRoceV2Connection('active', 1).to_json()['ip_cma'])

    def test_cm_rejects_missing_ip_source_nonboolean_policy_and_malformed_hex(self):
        invalid = [dict(ip_cma=True), dict(source_port=0, destination_port=4660,
                                          service_id=0x01061234),
                   dict(auto_accept=1), dict(ip_cma=1), dict(private_data='0'),
                   dict(private_data='zz'), dict(private_data=17), dict(private_data=None)]
        for options in invalid:
            values = dict(mode='active', service_id=1)
            values.update(options)
            with self.subTest(values=values), self.assertRaises(TRexError):
                ASTFRoceV2Connection(**values)


class RoCEv2DeviceProfileTest(unittest.TestCase):
    @staticmethod
    def device(device_id=1, port=0, ipv4='192.0.2.1', ipv6=None, **options):
        host = ASTFRoceV2Host(port, '02:00:00:00:00:%02x' % device_id,
                             ipv4=ipv4, ipv6=ipv6)
        values = dict(local_qpn=2, remote_qpn=2, dst_mac='02:00:00:00:ff:01',
                      dst_ip='192.0.2.254', message_size=16)
        values.update(options)
        return ASTFRoceV2Device(device_id, host, qps=[values])

    def test_default_scale_builds_1024_qps_and_keeps_queue_depths_independent(self):
        data = ASTFRoceV2Scale(environ={}).paired_profile().to_json()
        self.assertEqual(data['schema_version'], 3)
        self.assertEqual(data['scale'], dict(devices_per_port=32, qps_per_device=16))
        self.assertEqual(len(data['devices']), 64)
        self.assertEqual(len(data['qps']), 1024)
        self.assertEqual(data['max_memory_bytes'], 128 * 1024 * 1024)
        self.assertEqual(data['max_native_storage_bytes'], 512 * 1024 * 1024)
        self.assertEqual({entry['outstanding'] for entry in data['qps']}, {1})
        self.assertEqual({entry['receive_depth'] for entry in data['qps']}, {0})
        self.assertEqual({entry['local_qpn'] for entry in data['qps']}, set(range(2, 18)))
        self.assertEqual({entry['local_rkey'] for entry in data['qps']}, {1})
        for device in data['devices']:
            self.assertNotIn('qps', device)
            qps = [entry for entry in data['qps'] if entry['device_id'] == device['device_id']]
            self.assertEqual(len(qps), 16)
            self.assertEqual(len({entry['memory_id'] for entry in qps}), 16)
            self.assertEqual(len({entry['protection_domain'] for entry in qps}), 16)

    def test_scale_environment_precedence_and_selected_value_validation(self):
        env = {'TREX_ROCEV2_DEVICES_PER_PORT': '3', 'TREX_ROCEV2_QPS_PER_DEVICE': '5'}
        self.assertEqual(ASTFRoceV2Scale(environ=env).to_json(),
                         dict(devices_per_port=3, qps_per_device=5))
        env['TREX_ROCEV2_DEVICES_PER_PORT'] = 'invalid-unused-value'
        self.assertEqual(ASTFRoceV2Scale(devices_per_port=2, environ=env).to_json(),
                         dict(devices_per_port=2, qps_per_device=5))
        for value in ('', '-1', '1.5', 'true', ' 2 ', '0', '4097', None, True):
            with self.subTest(value=value), self.assertRaises(TRexError):
                ASTFRoceV2Scale(environ={'TREX_ROCEV2_QPS_PER_DEVICE': value})
        for value in (0, True, 1.5, '2', 4097):
            with self.subTest(value=value), self.assertRaises(TRexError):
                ASTFRoceV2Scale(qps_per_device=value, environ={})
        with mock.patch.dict('os.environ', {'TREX_ROCEV2_DEVICES_PER_PORT': '2',
                                            'TREX_ROCEV2_QPS_PER_DEVICE': '3'}):
            self.assertEqual(ASTFRoceV2Scale().to_json(), dict(devices_per_port=2, qps_per_device=3))

    def test_scale_is_deterministic_dual_stack_and_preserves_total_qp_count(self):
        scale = ASTFRoceV2Scale(2, 4, environ={})
        first = scale.paired_profile(ip_version='dual', vlan=100).to_json()
        self.assertEqual(first, scale.paired_profile(ip_version='dual', vlan=100).to_json())
        self.assertEqual(len(first['qps']), 16)
        self.assertEqual(len({device['host']['mac'] for device in first['devices']}), 4)
        for device in first['devices']:
            qps = [qp for qp in first['qps'] if qp['device_id'] == device['device_id']]
            self.assertEqual(len(qps), 4)
            self.assertEqual([qp['local_gid_index'] for qp in qps], [0, 1, 0, 1])
            self.assertEqual({qp['vlan'] for qp in qps}, {100})
            self.assertEqual(len({qp['udp_sport'] for qp in qps}), 4)
            self.assertTrue(all(49152 <= qp['udp_sport'] <= 65535 for qp in qps))
            self.assertEqual([gid['index'] for gid in device['gids']], [0, 1])
        ipv6 = scale.paired_profile(ip_version='ipv6').to_json()
        self.assertTrue(all(':' in qp['src_ip'] and qp['local_gid_index'] == 0 for qp in ipv6['qps']))
        read_only = scale.paired_profile(seed=11, program=[ASTFRoceV2Work('read')]).to_json()
        self.assertTrue(all(qp['seed'] == qp['peer_seed'] == 11 and not qp['expect_writes']
                            for qp in read_only['qps']))

    def test_requested_receive_storage_and_scale_are_never_silently_trimmed(self):
        scale = ASTFRoceV2Scale(environ={})
        data = scale.paired_profile(outstanding=16, receive_depth=16).to_json()
        self.assertEqual(len(data['qps']), 1024)
        self.assertEqual(sum(qp['memory_size'] for qp in data['qps']), 68 * 1024 * 1024)
        with self.assertRaises(TRexError):
            scale.paired_profile(receive_depth=16, max_memory_bytes=64 * 1024 * 1024)
        with self.assertRaises(TRexError):
            ASTFRoceV2Scale(129, 16, environ={}).paired_profile()
        for ports in ([], [0], [0, 0], [0, True], [0, 256]):
            with self.subTest(ports=ports), self.assertRaises(TRexError):
                ASTFRoceV2Scale(1, 1, environ={}).paired_profile(ports=ports)
        with self.assertRaises(TRexError):
            scale.paired_profile(connection=ASTFRoceV2Connection('passive', 123))

    def test_device_scoped_qpn_pd_mr_and_keys_can_repeat_between_devices(self):
        first = self.device(1, memory_id=7, protection_domain=1)
        second = self.device(2, ipv4='192.0.2.2', memory_id=7, protection_domain=1)
        data = ASTFRoceV2Profile(devices=[first, second], max_memory_bytes=64).to_json()
        self.assertEqual([qp['local_qpn'] for qp in data['qps']], [2, 2])
        self.assertEqual([qp['memory_id'] for qp in data['qps']], [7, 7])
        self.assertEqual([qp['protection_domain'] for qp in data['qps']], [1, 1])
        with self.assertRaises(TRexError):
            ASTFRoceV2Profile(devices=[first, second], max_memory_bytes=63)
        with self.assertRaises(TRexError):
            ASTFRoceV2Profile(devices=[first, self.device(2)])
        with self.assertRaises(TRexError):
            ASTFRoceV2Profile([endpoint_qp(device_id=1)])

    def test_same_device_qpn_and_globally_duplicate_ids_guids_are_rejected(self):
        device = self.device()
        with self.assertRaises(TRexError):
            ASTFRoceV2Profile(devices=[device, device])
        values = device.to_json()
        duplicate = ASTFRoceV2Device(2, ASTFRoceV2Host(0, '02:00:00:00:00:02', ipv4='192.0.2.2'),
                                    qps=[dict(local_qpn=2, remote_qpn=3, dst_ip='192.0.2.254',
                                              dst_mac='02:00:00:00:ff:01')], node_guid=values['node_guid'])
        with self.assertRaises(TRexError):
            ASTFRoceV2Profile(devices=[device, duplicate])
        values['qps'].append(copy.deepcopy(values['qps'][0]))
        with self.assertRaises(TRexError):
            ASTFRoceV2Profile(devices=[values])

    def test_device_sources_and_gid_indices_must_match_the_host(self):
        for options in (dict(src_ip='192.0.2.99'), dict(src_mac='02:00:00:00:ff:ff'),
                        dict(vlan=100), dict(local_gid_index=7), dict(prefix_length=24),
                        dict(gateway='192.0.2.254')):
            with self.subTest(options=options), self.assertRaises(TRexError):
                self.device(**options)
        for options in (dict(port=1), dict(device_id=2), dict(prefix_length=16),
                        dict(gateway='192.0.2.99')):
            host = ASTFRoceV2Host(0, '02:00:00:00:00:01', ipv4='192.0.2.1',
                                   ipv4_prefix=24, ipv4_gateway='192.0.2.254')
            qp = dict(local_qpn=2, remote_qpn=3, dst_mac='02:00:00:00:00:02', dst_ip='192.0.2.2')
            qp.update(options)
            with self.subTest(options=options), self.assertRaises(TRexError):
                ASTFRoceV2Device(1, host, qps=[qp])
        host = ASTFRoceV2Host(0, '02:00:00:00:00:01', ipv4='192.0.2.1', ipv6='2001:db8::1')
        device = ASTFRoceV2Device(1, host, qps=[dict(local_qpn=2, remote_qpn=3,
            dst_mac='02:00:00:00:00:02', dst_ip='2001:db8::2', local_gid_index=1)])
        self.assertEqual(device.to_json()['qps'][0]['src_ip'], '2001:db8::1')
        for gids in ([dict(index=0, ip='192.0.2.1', gid='::1')],
                     [dict(index=True, ip='192.0.2.1', gid='::ffff:192.0.2.1')]):
            with self.subTest(gids=gids), self.assertRaises(TRexError):
                ASTFRoceV2Device(1, host, gids=gids)

    def test_profile_flattened_schema_and_copy_isolation(self):
        device = self.device()
        data = ASTFRoceV2Profile(devices=[device]).to_json()
        profile = ASTFRoceV2Profile(qps=data['qps'], devices=data['devices'],
                                    scale=dict(devices_per_port=1, qps_per_device=1))
        data['devices'][0]['host']['ipv4'] = '192.0.2.99'
        data['qps'][0]['local_rkey'] = 99
        self.assertEqual(profile.to_json()['devices'][0]['host']['ipv4'], '192.0.2.1')
        self.assertEqual(profile.to_json()['qps'][0]['local_rkey'], 1)
        result = profile.to_json()
        result['devices'][0]['gids'][0]['ip'] = '192.0.2.99'
        self.assertEqual(profile.to_json()['devices'][0]['gids'][0]['ip'], '192.0.2.1')
        self.assertEqual(ASTFRoceV2Profile([endpoint_qp()]).to_json()['max_memory_bytes'], 64 * 1024 * 1024)
        with self.assertRaises(TRexError):
            ASTFRoceV2Profile(devices=[ASTFRoceV2Device(1,
                ASTFRoceV2Host(0, '02:00:00:00:00:01', ipv4='192.0.2.1'))])
        with self.assertRaises(TRexError):
            ASTFRoceV2Profile(devices=[self.device(), ASTFRoceV2Device(2,
                ASTFRoceV2Host(0, '02:00:00:00:00:02', ipv4='192.0.2.2'))])

    def test_resource_limits_reject_invalid_values_and_accept_explicit_storage_override(self):
        profile = ASTFRoceV2Profile(devices=[self.device()], max_native_storage_bytes=1024**3)
        self.assertEqual(profile.to_json()['max_native_storage_bytes'], 1024**3)
        for value in (0, -1, True, '512', 16 * 1024**3 + 1):
            with self.subTest(value=value), self.assertRaises(TRexError):
                ASTFRoceV2Profile(devices=[self.device()], max_native_storage_bytes=value)
        for value in (0, 256 * 1024 * 1024 + 1):
            with self.subTest(value=value), self.assertRaises(TRexError):
                ASTFRoceV2Profile(devices=[self.device()], max_memory_bytes=value)

    def test_emu_client_conversion_copies_static_dual_stack_and_single_vlan(self):
        from trex.emu.trex_emu_profile import EMUClientObj, EMUNamespaceKey
        client = EMUClientObj([2, 0, 0, 0, 0, 1], ipv4=[192, 0, 2, 1],
                              ipv4_dg=[192, 0, 2, 254],
                              ipv6=list(bytes.fromhex('20010db8000000000000000000000001')),
                              dg_ipv6=list(bytes.fromhex('20010db80000000000000000000000fe')),
                              plugs={'arp': {}, 'icmp': {}})
        namespace = EMUNamespaceKey(0, tci=[100, 0], tpid=[0x8100, 0])
        values = ASTFRoceV2Host.from_emu_client(client, namespace, ipv4_prefix=24, ipv6_prefix=64).to_json()
        self.assertEqual(values, dict(port=0, mac='02:00:00:00:00:01', ipv4='192.0.2.1',
            ipv4_prefix=24, ipv4_gateway='192.0.2.254', ipv6='2001:db8::1',
            ipv6_prefix=64, ipv6_gateway='2001:db8::fe', vlan=100))
        self.assertNotIn('plugs', values)
        self.assertEqual(client.to_json()['plugs'], {'arp': {}, 'icmp': {}})
        untagged = ASTFRoceV2Host.from_emu_client(dict(mac=[2, 0, 0, 0, 0, 2],
            ipv4=[192, 0, 2, 2], ipv4_dg=[0, 0, 0, 0]), port=1).to_json()
        self.assertNotIn('ipv4_gateway', untagged)
        self.assertNotIn('vlan', untagged)

    def test_emu_conversion_rejects_qinq_unsupported_shapes_and_port_conflicts(self):
        client = dict(mac='02:00:00:00:00:01', ipv4='192.0.2.1')
        for namespace in (dict(vport=0, tci=[100, 200], tpid=[0x8100, 0x8100]),
                          dict(vport=0, tci=[100], tpid=[0x88a8]),
                          dict(vport=0, tci=[4096]), dict(vport=0, unsupported=1)):
            with self.subTest(namespace=namespace), self.assertRaises(TRexError):
                ASTFRoceV2Host.from_emu_client(client, namespace)
        with self.assertRaises(TRexError):
            ASTFRoceV2Host.from_emu_client(client, dict(vport=0), port=1)
        with self.assertRaises(TRexError):
            ASTFRoceV2Host.from_emu_client(dict(client, dhcp=True), port=0)
        with self.assertRaises(TRexError):
            ASTFRoceV2Host.from_emu_client(dict(mac=client['mac'], ipv4=[1, 2]), port=0)

    def test_capture_filter_uses_direction_device_addresses_and_vlan(self):
        host = ASTFRoceV2Host(0, '02:00:00:00:00:01', ipv4='192.0.2.1', ipv6='2001:db8::1', vlan=100)
        device = ASTFRoceV2Device(1, host)
        both = device.capture_filter()
        self.assertIn('vlan 100', both)
        self.assertIn('ether src 02:00:00:00:00:01', both)
        self.assertIn('ether dst 02:00:00:00:00:01', both)
        self.assertIn('src host 2001:db8::1', both)
        self.assertIn('dst host 192.0.2.1', both)
        self.assertNotIn('ether dst', device.capture_filter('tx'))
        self.assertNotIn('ether src', device.capture_filter('rx'))
        with self.assertRaises(TRexError):
            device.capture_filter('invalid')


class RoCEv2EndpointClientTest(unittest.TestCase):
    def setUp(self):
        self.client = ASTFClient.__new__(ASTFClient)
        self.client.handler = 'owner-handler'
        self.rpc = mock.Mock()
        self.rpc.get_timeout_sec.return_value = 3
        self.client.conn = SimpleNamespace(
            rpc=self.rpc, is_connected=mock.Mock(return_value=True),
            is_marked_for_disconnect=mock.Mock(return_value=False),
            sigint_on_conn_lost_enable=mock.Mock(), sigint_on_conn_lost_disable=mock.Mock())
        self.client._transmit = mock.Mock(return_value=RC_OK({'workers': []}))

    def test_configure_transmits_schema_and_acquisition_handler(self):
        profile = ASTFRoceV2Profile([endpoint_qp(program=[ASTFRoceV2Work('send')],
            receives=[ASTFRoceV2Receive(9, [dict(address=0x100010, length=16, lkey=1)])])])
        self.client.configure_rocev2(profile)
        self.client._transmit.assert_called_once_with('rocev2_configure',
            dict(handler='owner-handler', config=profile.to_json()))
        self.assertEqual(self.rpc.set_timeout_sec.call_args_list, [mock.call(15), mock.call(3)])
        self.client.conn.sigint_on_conn_lost_disable.assert_called_once_with()

    def test_configure_accepts_constructor_dict_and_rejects_invalid_before_rpc(self):
        self.client.configure_rocev2(dict(qps=[endpoint_qp().to_json()], generation=3))
        request = self.client._transmit.call_args[0][1]
        self.assertEqual(request['config']['generation'], 3)
        self.assertEqual(request['config']['schema_version'], 2)
        self.client._transmit.reset_mock()
        for profile in (None, [], object(), dict(qps=[])):
            with self.subTest(profile=profile), self.assertRaises(TRexError):
                self.client.configure_rocev2(profile)
        self.client._transmit.assert_not_called()

    def test_cm_policy_reaches_native_configure_request(self):
        connection = ASTFRoceV2Connection('active', 0x01061234,
                                         source_port=50000, destination_port=4660)
        self.client.configure_rocev2(dict(qps=[endpoint_qp(connection=connection).to_json()]))
        payload = self.client._transmit.call_args[0][1]
        self.assertEqual(payload['config']['qps'][0]['connection'], connection.to_json())
        self.assertEqual(payload['handler'], 'owner-handler')

    def test_lifecycle_stats_and_validation_preserve_worker_response(self):
        workers = dict(passed=True, done=True, workers=[dict(core_id=1, qps=[
            dict(local_qpn=2, send_completions=3, receive_completions=2,
                 atomic_completions=1, errors=0)])])
        self.client._transmit.return_value = RC_OK(workers)
        for method, command in ((self.client.start_rocev2, 'start'),
                                (self.client.stop_rocev2, 'stop'),
                                (self.client.disconnect_rocev2, 'disconnect'),
                                (self.client.cancel_rocev2, 'cancel'),
                                (self.client.get_rocev2_events, 'get_events'),
                                (self.client.get_rocev2_stats, 'get_stats'),
                                (self.client.validate_rocev2, 'validate')):
            self.client._transmit.reset_mock()
            self.assertIs(method(), workers)
            self.client._transmit.assert_called_once_with('rocev2_' + command,
                                                         {'handler': 'owner-handler'})

    def test_cm_accept_reject_send_bounded_hex_control_with_ownership_handler(self):
        self.client.accept_rocev2(0xffffffff, private_data=b'\x00\x80\xff')
        self.client._transmit.assert_called_once_with('rocev2_cm_control', dict(
            handler='owner-handler', command=dict(action='accept', connection_id=0xffffffff,
                                                  private_data='0080ff')))
        self.client._transmit.reset_mock()
        self.client.reject_rocev2(0x01000001, reason=65535, private_data='ABCD')
        self.client._transmit.assert_called_once_with('rocev2_cm_control', dict(
            handler='owner-handler', command=dict(action='reject', connection_id=0x01000001,
                                                  reason=65535, private_data='abcd')))
        self.client._transmit.reset_mock()
        self.client.reject_rocev2(1)
        self.client._transmit.assert_called_once_with('rocev2_cm_control', dict(
            handler='owner-handler', command=dict(action='reject', connection_id=1,
                                                  reason=28, private_data='')))

    def test_cm_control_rejects_bad_ids_reasons_and_private_data_before_rpc(self):
        for method in (self.client.accept_rocev2, self.client.reject_rocev2):
            for connection_id in (0, -1, 2**32, True, 1.0, '1'):
                with self.subTest(method=method, connection_id=connection_id), self.assertRaises(TRexError):
                    method(connection_id)
        for data in ('0', 'zz', None, 1, b'x' * 197):
            with self.subTest(data=data), self.assertRaises(TRexError):
                self.client.accept_rocev2(1, private_data=data)
        with self.assertRaises(TRexError):
            self.client.reject_rocev2(1, private_data=b'x' * 149)
        for reason in (-1, 0, 65536, True):
            with self.subTest(reason=reason), self.assertRaises(TRexError):
                self.client.reject_rocev2(1, reason=reason)
        self.client._transmit.assert_not_called()

    def test_cm_events_return_worker_events_without_client_cache(self):
        request = dict(workers=[dict(thread_id=0, events=[dict(
            type='connect_request', sequence=1, connection_id=0x01000001,
            private_data='0080ff', wire_private_data='000000000080ff')])])
        history = copy.deepcopy(request)
        history['workers'][0]['events'].append(dict(type='established', sequence=2,
            connection_id=0x01000001, private_data='', wire_private_data=''))
        self.client._transmit.side_effect = [RC_OK(request), RC_OK(history)]
        self.assertIs(self.client.get_rocev2_events(), request)
        self.assertIs(self.client.get_rocev2_events(), history)
        self.assertEqual(self.client._transmit.call_args_list, [
            mock.call('rocev2_get_events', {'handler': 'owner-handler'}),
            mock.call('rocev2_get_events', {'handler': 'owner-handler'})])

    def test_capabilities_query_preserves_supported_and_unqualified_status(self):
        capabilities = dict(schema_versions=[1, 2], transport='RC',
            operations=['send', 'send_imm', 'send_inv', 'write', 'write_imm',
                        'read', 'compare_swap', 'fetch_add'],
            max_receive_sge=16, max_rd_atomic=16, carriers=['dpdk', 'af_packet'],
            unsupported=['UC', 'UD', 'SRQ', 'XRC'], conformance_status='unqualified')
        self.client._transmit.return_value = RC_OK(capabilities)
        self.assertIs(self.client.get_rocev2_capabilities(), capabilities)
        self.client._transmit.assert_called_once_with('rocev2_capabilities',
                                                     {'handler': 'owner-handler'})

    def test_rpc_failure_restores_timeout_and_reports_backend_error(self):
        self.client._transmit.return_value = RC_ERR('RoCEv2 session owner mismatch')
        with self.assertRaisesRegex(TRexError, 'owner mismatch'):
            self.client.start_rocev2()
        self.assertEqual(self.rpc.set_timeout_sec.call_args_list, [mock.call(15), mock.call(3)])
        self.client.conn.sigint_on_conn_lost_disable.assert_called_once_with()

    def test_transmit_exception_restores_existing_long_timeout(self):
        self.rpc.get_timeout_sec.return_value = 40
        self.client._transmit.side_effect = RuntimeError('socket closed')
        with self.assertRaisesRegex(RuntimeError, 'socket closed'):
            self.client.get_rocev2_stats()
        self.assertEqual(self.rpc.set_timeout_sec.call_args_list, [mock.call(40), mock.call(40)])
        self.client.conn.sigint_on_conn_lost_disable.assert_called_once_with()

    def test_disconnected_client_rejects_endpoint_commands_without_transmit(self):
        self.client.conn.is_connected.return_value = False
        with self.assertRaisesRegex(TRexError, 'disconnected'):
            self.client.start_rocev2()
        self.client._transmit.assert_not_called()
        self.client.conn.sigint_on_conn_lost_disable.assert_called_once_with()

    def test_wait_returns_validation_only_after_done(self):
        verdict = dict(passed=True, workers=[dict(core_id=0, qps=[])])
        self.client.get_rocev2_stats = mock.Mock(side_effect=[dict(done=False), dict(done=True)])
        self.client.validate_rocev2 = mock.Mock(return_value=verdict)
        with mock.patch('time.sleep'):
            self.assertIs(self.client.wait_on_rocev2(), verdict)
        self.assertEqual(self.client.get_rocev2_stats.call_count, 2)
        self.client.validate_rocev2.assert_called_once_with()

    def test_wait_rejects_errors_failed_validation_and_timeout(self):
        self.client.get_rocev2_stats = mock.Mock(return_value=dict(done=False, errors=1))
        self.client.validate_rocev2 = mock.Mock()
        with self.assertRaisesRegex(TRexError, 'reported errors'):
            self.client.wait_on_rocev2()
        self.client.validate_rocev2.assert_not_called()
        self.client.get_rocev2_stats.return_value = dict(done=True)
        self.client.validate_rocev2.return_value = dict(passed=False, errors=['receive mismatch'])
        with self.assertRaisesRegex(TRexError, 'receive mismatch'):
            self.client.wait_on_rocev2()
        self.client.get_rocev2_stats.return_value = dict(done=False)
        with mock.patch('time.monotonic', side_effect=[0, 2]):
            with self.assertRaisesRegex(TRexError, 'Timed out'):
                self.client.wait_on_rocev2(timeout=1)

    def test_wait_rejects_invalid_limits_without_polling(self):
        self.client.get_rocev2_stats = mock.Mock()
        for limits in (dict(timeout=0), dict(timeout=math.inf), dict(timeout=True),
                       dict(poll_interval=-1), dict(poll_interval=math.nan)):
            with self.subTest(limits=limits), self.assertRaises(TRexError):
                self.client.wait_on_rocev2(**limits)
        self.client.get_rocev2_stats.assert_not_called()
