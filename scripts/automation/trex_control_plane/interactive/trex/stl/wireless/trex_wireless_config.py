import yaml
import collections
import os

def load_config(filename=None):
    # Load default config
    with open(os.path.join(os.path.dirname(os.path.realpath(__file__)), "trex_wireless_default_config.yaml")) as f:
        yaml_data_default = yaml.load(f)

    # Then open user's one if any
    if filename:
        with open(filename) as f:
            yaml_data = yaml.load(f)
    else:
        yaml_data = {}
    return read_config(yaml_data, yaml_data_default)


def dict_merge(dct, merge_dct, add_keys=True):
    """ Recursive dict merge. Inspired by :meth:``dict.update()``, instead of
    updating only top-level keys, dict_merge recurses down into dicts nested
    to an arbitrary depth, updating keys. The ``merge_dct`` is merged into
    ``dct``.

    This version will return a copy of the dictionary and leave the original
    arguments untouched.

    The optional argument ``add_keys``, determines whether keys which are
    present in ``merge_dict`` but not ``dct`` should be included in the
    new dict.

    Args:
        dct (dict) onto which the merge is executed
        merge_dct (dict): dct merged into dct
        add_keys (bool): whether to add new keys

    Returns:
        dict: updated dict
    """
    dct = dct.copy()
    if not add_keys:
        merge_dct = {
            k: merge_dct[k]
            for k in set(dct).intersection(set(merge_dct))
        }

    for k, v in merge_dct.items():
        if (k in dct and isinstance(dct[k], dict)
                and isinstance(merge_dct[k], collections.Mapping)):
            dct[k] = dict_merge(dct[k], merge_dct[k], add_keys=add_keys)
        else:
            dct[k] = merge_dct[k]

    return dct

def read_config(yaml_data, default_yaml):
    def mapping_to_namedtuple(mapping):
        """Converts a mapping into a recursive namedtuple.:"""
        def namedtuple_from_mapping(mapping):
            nt = collections.namedtuple("config", mapping.keys())
            return nt(**mapping)

        if isinstance(mapping, collections.Mapping):
            for key, value in mapping.items():
                mapping[key] = mapping_to_namedtuple(value)
            return namedtuple_from_mapping(mapping)

        return mapping
    d = dict_merge(default_yaml, yaml_data)
    global config
    config = mapping_to_namedtuple(d)
    return config
