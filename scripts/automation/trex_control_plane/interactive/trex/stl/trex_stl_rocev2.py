"""Counter validation for finite stateless RoCEv2 traffic.

Basic flow statistics leave the RoCEv2 payload intact. Latency and tagged-packet
statistics overwrite its trailer and must not be used with RoCEv2 ICRC streams.
"""

import math


def rocev2_stats_expectations(streams):
    """Return per-PGID packet/byte totals for one finite burst of each stream.

    Byte totals include Ethernet FCS, matching TRex's flow-stat TX counters.
    The caller must run these streams once, without a duration or packet limit
    that truncates the bursts, and clear counters before starting traffic.
    """
    expected = {}
    for stream in streams:
        data = stream.to_json()
        mode = data['mode']
        if mode['type'] != 'single_burst' or data.get('action_count', 0):
            raise ValueError('RoCEv2 expectations require finite single bursts without action loops')
        if not stream.has_flow_stats() or stream.get_flow_stats_type() != 'stats':
            raise ValueError('RoCEv2 validation requires basic flow stats on every stream')
        count = mode['total_pkts']
        if isinstance(count, bool) or not isinstance(count, int) or count <= 0:
            raise ValueError('Burst packet count must be a positive integer')
        pgid = stream.get_pg_id()
        entry = expected.setdefault(pgid, {'tx_pkts': 0, 'tx_bytes': 0})
        entry['tx_pkts'] += count
        entry['tx_bytes'] += count * stream.get_pkt_len()
    if not expected:
        raise ValueError('No RoCEv2 streams to validate')
    return expected


def validate_rocev2_stats(stats, expected, tx_port, rx_port, max_loss_pct=0.0,
                         check_rx_bytes=False):
    """Validate a fresh ``STLClient.get_pgid_stats()`` result.

    Missing, malformed, or negative counters fail validation. RX must be at
    most TX and within the requested loss percentage; TX must exactly match
    the finite-burst expectation. RX bytes are optional because some NICs do
    not supply them. Enabling their check requires exact, lossless byte totals.
    This checks traffic accounting, not RDMA completions or remote memory.
    """
    if not math.isfinite(max_loss_pct) or not 0 <= max_loss_pct <= 100:
        raise ValueError('max_loss_pct must be finite and in range 0..100')
    if not expected:
        raise ValueError('No expected RoCEv2 counters')
    if not isinstance(stats, dict):
        return {'passed': False, 'errors': ['Missing or invalid flow statistics'], 'flows': {}}
    flows = stats.get('flow_stats', {})
    errors = []
    results = {}

    def counter(values, key, port, label):
        counters = values.get(key, {})
        value = counters.get(port, counters.get(str(port))) if isinstance(counters, dict) else None
        if isinstance(value, bool) or not isinstance(value, int) or value < 0:
            errors.append('{0}: missing or invalid {1} on port {2}'.format(label, key, port))
            return None
        return value

    for pgid, target in expected.items():
        label = 'PGID {0}'.format(pgid)
        flow = flows.get(pgid, flows.get(str(pgid))) if isinstance(flows, dict) else None
        if not isinstance(flow, dict):
            errors.append('{0}: no flow statistics'.format(label))
            continue
        tx = counter(flow, 'tx_pkts', tx_port, label)
        rx = counter(flow, 'rx_pkts', rx_port, label)
        tx_bytes = counter(flow, 'tx_bytes', tx_port, label)
        if tx is not None and tx != target['tx_pkts']:
            errors.append('{0}: TX packets {1}, expected {2}'.format(label, tx, target['tx_pkts']))
        if tx_bytes is not None and tx_bytes != target['tx_bytes']:
            errors.append('{0}: TX bytes {1}, expected {2}'.format(label, tx_bytes, target['tx_bytes']))
        loss = None
        if tx is not None and rx is not None:
            loss = 100.0 * (tx - rx) / tx if tx else None
            if not tx or rx > tx or loss > max_loss_pct:
                errors.append('{0}: TX/RX packets {1}/{2}, allowed loss {3}%'.format(label, tx, rx, max_loss_pct))
        if check_rx_bytes:
            rx_bytes = counter(flow, 'rx_bytes', rx_port, label)
            if rx_bytes is not None and rx_bytes != target['tx_bytes']:
                errors.append('{0}: RX bytes {1}, expected {2}'.format(label, rx_bytes, target['tx_bytes']))
        results[pgid] = {'tx_pkts': tx, 'rx_pkts': rx, 'tx_bytes': tx_bytes, 'loss_pct': loss}

    global_stats = flows.get('global', {}) if isinstance(flows, dict) else {}
    if not isinstance(global_stats, dict):
        errors.append('Invalid global flow statistics')
        global_stats = {}
    for key in ('rx_err', 'tx_err'):
        values = global_stats.get(key, {})
        if not isinstance(values, dict):
            errors.append('Invalid global {0} counters'.format(key))
            continue
        for port, value in values.items():
            if value != 0:
                errors.append('Flow-stat {0} on port {1}: {2}'.format(key, port, value))
    return {'passed': not errors, 'errors': errors, 'flows': results}
