
import stl_path
from trex.stl.api import *

from pprint import pformat


def example(tx_port, rx_port):

    c = STLClient()

    # connect to server
    c.connect()

    # prepare our ports
    c.reset(ports=[tx_port, rx_port])

    initial_tags = [
        {
            "type": "QinQ",
            "value": {
                "vlans": [1, 2]
            }
        },
        {
            "type": "Dot1Q",
            "value": {
                "vlan": 1
            }
        },
        {
            "type": "Dot1Q",
            "value": {
                "vlan": 2
            }
        }
    ]

    # enable tpg
    c.enable_tpg(num_tpgids=10, tags=initial_tags, rx_ports=[rx_port])

    want = [
        {'type': 'QinQ', 'value': {'vlans': [1, 2]}},
        {'type': 'Dot1Q', 'value': {'vlan': 1}},
        {'type': 'Dot1Q', 'value': {'vlan': 2}}
    ]

    have = c.get_tpg_tags()

    assert have == want, "\nFailed!\n Want = {}\n Have =  {}".format(pformat(want), pformat(have))

    # First update
    tags_to_update = [
        {
            "type": None,
            "tag_id": 2
        }
    ]

    c.update_tpg_tags(tags_to_update)

    want = [
        {'type': 'QinQ', 'value': {'vlans': [1, 2]}},
        {'type': 'Dot1Q', 'value': {'vlan': 1}},
        None
    ]


    have = c.get_tpg_tags()

    assert have == want, "\nFailed!'\n Want = {}\n Have =  {}".format(pformat(want), pformat(have))

    # Second update
    tags_to_update = [
        {
            "type": None,
            "tag_id": 0
        },
        {
            "type": "QinQ",
            "value": {
                "vlans": [1, 2]
            },
            "tag_id": 2
        },
    ]

    c.update_tpg_tags(tags_to_update)

    want = [
        None,
        {'type': 'Dot1Q', 'value': {'vlan': 1}},
        {'type': 'QinQ', 'value': {'vlans': [1, 2]}}
    ]

    have = c.get_tpg_tags()

    assert have == want, "\nFailed!\n Want = {}\n Have =  {}".format(pformat(want), pformat(have))

    print("\nTest passed successfully!\n")


def main():
    example(tx_port=0, rx_port=1)


if __name__ == "__main__":
    main()
