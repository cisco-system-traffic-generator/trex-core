
"""
Dan Klein
Cisco Systems, Inc.

Copyright (c) 2015-2015 Cisco Systems, Inc.
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
"""

import external_packages
import yaml


class CTRexYAMLLoader(object):
    TYPE_DICT = {"double":float,
                 "int":int,
                 "array":list,
                 "string":str,
                 "boolean":bool}

    def __init__(self, yaml_ref_file_path):
        self.yaml_path = yaml_ref_file_path
        self.ref_obj = None

    def check_term_param_type(self, val, val_field, ref_val, multiplier):
        # print val, val_field, ref_val
        tmp_type = ref_val.get('type')
        if isinstance(tmp_type, list):
            # item can be one of multiple types
            # print "multiple choice!"
            python_types = set()
            for t in tmp_type:
                if t in self.TYPE_DICT:
                    python_types.add(self.TYPE_DICT.get(t))
                else:
                    return False, TypeError("Unknown resolving for type {0}".format(t))
            # print "python legit types: ", python_types
            if type(val) not in python_types:
                return False, TypeError("Type of object field '{0}' is not allowed".format(val_field))
            else:
                # WE'RE OK!
                return True, CTRexYAMLLoader._calc_final_value(val, multiplier, ref_val.get('multiply', False))
        else:
            # this is a single type field
            python_type = self.TYPE_DICT.get(tmp_type)
            if not isinstance(val, python_type):
                return False, TypeError("Type of object field '{0}' is not allowed".format(val_field))
            else:
                # WE'RE OK!
                return True, CTRexYAMLLoader._calc_final_value(val, multiplier, ref_val.get('multiply', False))

    def get_reference_default(self, root_obj, sub_obj, key):
        # print root_obj, sub_obj, key
        if sub_obj:
            ref_field = self.ref_obj.get(root_obj).get(sub_obj).get(key)
        else:
            ref_field = self.ref_obj.get(root_obj).get(key)
        if 'has_default' in ref_field:
            if ref_field.get('has_default'):
                # WE'RE OK!
                return True, ref_field.get('default')
            else:
                # This is a mandatory field!
                return False, ValueError("The {0} field is mandatory and must be specified explicitly".format(key))
        else:
            return False, ValueError("The {0} field has no indication about default value".format(key))

    def validate_yaml(self, evaluated_obj, root_obj, fill_defaults=True, multiplier=1):
        if isinstance(evaluated_obj, dict) and evaluated_obj.keys() == [root_obj]:
            evaluated_obj = evaluated_obj.get(root_obj)
        if not self.ref_obj:
            self.ref_obj = load_yaml_to_obj(self.yaml_path)
            # self.load_reference()
        ref_item = self.ref_obj.get(root_obj)
        if ref_item is not None:
            try:
                typed_obj = [False, None]   # first item stores validity (multiple object "shapes"), second stored type
                if "type" in evaluated_obj:
                    ref_item = ref_item[evaluated_obj.get("type")]
                    # print "lower resolution with typed object"
                    typed_obj = [True, evaluated_obj.get("type")]
                if isinstance(ref_item, dict) and "type" not in ref_item:   # this is not a terminal
                    result_obj = {}
                    if typed_obj[0]:
                        result_obj["type"] = typed_obj[1]
                    # print "processing dictionary non-terminal value"
                    for k, v in ref_item.items():
                        # print "processing element '{0}' with value '{1}'".format(k,v)
                        if k in evaluated_obj:
                            # validate with ref obj
                            # print "found in evaluated object!"
                            tmp_type = v.get('type')
                            # print tmp_type
                            # print evaluated_obj
                            if tmp_type == "object":
                                # go deeper into nesting hierarchy
                                # print "This is an object type, recursion!"
                                result_obj[k] = self.validate_yaml(evaluated_obj.get(k), k, fill_defaults, multiplier)
                            else:
                                # validation on terminal type
                                # print "Validating terminal type %s" % k
                                res_ok, data = self.check_term_param_type(evaluated_obj.get(k), k, v, multiplier)
                                # print "Validating: ", res_ok
                                if res_ok:
                                    # data field contains the value to save
                                    result_obj[k] = data
                                else:
                                    # data var contains the exception to throw
                                    raise data
                        elif fill_defaults:
                            # complete missing values with default value, if exists
                            sub_obj = typed_obj[1] if typed_obj[0] else None
                            res_ok, data = self.get_reference_default(root_obj, sub_obj, k)
                            if res_ok:
                                # data field contains the value to save
                                result_obj[k] = data
                            else:
                                # data var contains the exception to throw
                                raise data
                    return result_obj
                elif isinstance(ref_item, list):
                    # currently not handling list objects
                    return NotImplementedError("List object are currently unsupported")
                else:
                    raise TypeError("Unknown parse tree object type.")
            except KeyError as e:
                raise
        else:
            raise KeyError("The given root_key '{key}' does not exists on reference object".format(key=root_obj))

    @staticmethod
    def _calc_final_value(val, multiplier, multiply):
        if multiply:
            return (val * multiplier)
        else:
            return val


def load_yaml_to_obj(file_path):
    try:
        return yaml.load(file(file_path, 'r'))
    except yaml.YAMLError as e:
        raise
    except Exception as e:
        raise

def yaml_exporter(file_path):
    pass

if __name__ == "__main__":
    pass
