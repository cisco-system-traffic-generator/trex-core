import argparse


MIN_VLAN, MAX_VLAN = 1, (1 << 12) - 1


class TPGTagConf():
    """ A simple Tagged Packet Group Tag Configuration containing Dot1Q entries only. """

    def get_tpg_conf(self, tunables, **kwargs):
        parser = argparse.ArgumentParser(description="TPG Tag Configuration File for Dot1Q",
                                         formatter_class=argparse.ArgumentDefaultsHelpFormatter)

        parser.add_argument("--min-vlan", type=int, default=1, help="Min Vlan Tag to have in the configuration")
        parser.add_argument("--max-vlan", type=int, default=MAX_VLAN-1, help="Max Vlan Tag to have in configuration")
        args = parser.parse_args(tunables)

        if args.min_vlan < MIN_VLAN:
            raise Exception("Min Vlan must be greater or equal to {}".format(MIN_VLAN))

        if args.max_vlan >= MAX_VLAN:
            raise Exception("Max Vlan must be smaller than {}".format(MAX_VLAN))

        tpg_conf = [
            {
                "type": "Dot1Q",
                "value": {
                    "vlan": i
                }
            } for i in range(args.min_vlan, args.max_vlan + 1)]
        return tpg_conf


def register():
    return TPGTagConf()
