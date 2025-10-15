import argparse
import ipaddress

from trex.common.services.trex_service import Service
from trex.common.services.trex_service_arp import ServiceARP
from trex.common.services.trex_service_IPv6ND import ServiceIPv6ND
from trex_stl_lib.api import *


class L3Profile(object):

    def get_streams(self, direction=0, port_id=0, client=None, tunables=(), **kwargs):
        if client is None:
            raise ValueError("client is None, cannot resolve gateways mac addresses")
        parser = argparse.ArgumentParser()
        parser.add_argument(
            "--left-ip",
            type=ipaddress.ip_address,
            default=ipaddress.ip_address("172.16.0.1"),
        )
        parser.add_argument(
            "--left-gw",
            type=ipaddress.ip_address,
            default=ipaddress.ip_address("172.16.0.254"),
        )
        parser.add_argument(
            "--right-ip",
            type=ipaddress.ip_address,
            default=ipaddress.ip_address("172.16.1.1"),
        )
        parser.add_argument(
            "--right-gw",
            type=ipaddress.ip_address,
            default=ipaddress.ip_address("172.16.1.254"),
        )
        parser.add_argument("--size", type=int, default=60)
        args = parser.parse_args(tunables)

        if direction == 0:
            src_ip = args.left_ip
            gw_ip = args.left_gw
            dst_ip = args.right_ip
        else:
            src_ip = args.right_ip
            gw_ip = args.right_gw
            dst_ip = args.left_ip

        try:
            client.set_service_mode(ports=[port_id], enabled=True)
            ctx = client.create_service_ctx(port=port_id)
            if dst_ip.version == 4:
                resolv = ServiceARP(
                    ctx,
                    dst_ip=str(gw_ip),
                    src_ip=str(src_ip),
                    verbose_level=Service.INFO,
                )
            else:
                resolv = ServiceIPv6ND(
                    ctx,
                    dst_ip=str(gw_ip),
                    src_ip=str(src_ip),
                    verbose_level=Service.INFO,
                )
            ctx.run(resolv)
            record = resolv.get_record()
            if not record:
                raise ValueError(f"gateway IP {gw_ip} not resolved")

            gw_mac = record.dst_mac
        finally:
            client.set_service_mode(ports=[port_id], enabled=False)

        base_pkt = Ether(dst=gw_mac)
        if dst_ip.version == 4:
            base_pkt /= IP(src=str(src_ip), dst=str(dst_ip))
        else:
            base_pkt /= IPv6(src=str(src_ip), dst=str(dst_ip))
        base_pkt /= UDP()
        pad = max(0, args.size - len(base_pkt)) * b"x"
        pkt = STLPktBuilder(pkt=base_pkt / pad)

        return [STLStream(packet=pkt, mode=STLTXCont())]


def register():
    return L3Profile()
