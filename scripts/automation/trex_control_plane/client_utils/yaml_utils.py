
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

    def load_reference(self):
        try:
            self.ref_obj = yaml.load(file(self.yaml_path, 'r'))
        except yaml.YAMLError as e:
            raise
        except Exception as e:
            raise

    def check_term_param_type(self, val, val_field, ref_val, multiplier):
        print val, val_field, ref_val
        tmp_type = ref_val.get('type')
        if isinstance(tmp_type, list):
            # item can be one of multiple types
            print "multiple choice!"
            python_types = set()
            for t in tmp_type:
                if t in self.TYPE_DICT:
                    python_types.add(self.TYPE_DICT.get(t))
                else:
                    return False, TypeError("Unknown resolving for type {0}".format(t))
            print "python legit types: ", python_types
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
        print root_obj, sub_obj, key
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
            self.load_reference()
        ref_item = self.ref_obj.get(root_obj)
        if ref_item is not None:
            try:
                typed_obj = [False, None]   # first item stores validity (multiple object "shapes"), second stored type
                if "type" in evaluated_obj:
                    ref_item = ref_item[evaluated_obj.get("type")]
                    print "lower resolution with typed object"
                    typed_obj = [True, evaluated_obj.get("type")]
                if isinstance(ref_item, dict) and "type" not in ref_item:   # this is not a terminal
                    result_obj = {}
                    if typed_obj[0]:
                        result_obj["type"] = typed_obj[1]
                    print "processing dictionary non-terminal value"
                    for k, v in ref_item.items():
                        print "processing element '{0}' with value '{1}'".format(k,v)
                        if k in evaluated_obj:
                            # validate with ref obj
                            print "found in evaluated object!"
                            tmp_type = v.get('type')
                            print tmp_type
                            print evaluated_obj
                            if tmp_type == "object":
                                # go deeper into nesting hierarchy
                                print "This is an object type, recursion!"
                                result_obj[k] = self.validate_yaml(evaluated_obj.get(k), k, fill_defaults, multiplier)
                            else:
                                # validation on terminal type
                                print "Validating terminal type %s" % k
                                res_ok, data = self.check_term_param_type(evaluated_obj.get(k), k, v, multiplier)
                                print "Validating: ", res_ok
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






#
# def yaml_obj_validator(evaluated_obj, yaml_reference_file_path, root_obj, fill_defaults=True):
#     """
#     validate SINGLE ROOT object with yaml reference file.
#     Fills up default values if hasn't been assigned by user and available.
#
#     :param evaluated_obj: python object that should match the yaml reference file
#     :param yaml_reference_file_path:
#     :param fill_defaults:
#     :return: a python representation object of the YAML file if OK
#     """
#     type_dict = {"double":float,
#                  "int":int,
#                  "array":list,
#                  "string":str,
#                  "boolean":bool}
#
#     def validator_rec_helper(obj_to_eval, ref_obj, root_key):
#         ref_item = ref_obj.get(root_key)
#         if ref_item is not None:
#             if "type" in obj_to_eval:
#                 ref_item = ref_item[obj_to_eval.get("type")]
#             if isinstance(ref_item, dict) and "type" not in ref_item:   # this is not a terminal
#                 result_obj = {}
#                 # iterate over key-value pairs
#                 for k, v in ref_item.items():
#                     if k in obj_to_eval:
#                         # need to validate with ref obj
#                         tmp_type = v.get('type')
#                         if tmp_type == "object":
#                             # go deeper into nesting hierarchy
#                             result_obj[k] = validator_rec_helper(obj_to_eval.get(k), ref_obj, k)
#                         elif isinstance(tmp_type, list):
#                             # item can be one of multiple types
#                             python_types = set()
#                             for t in tmp_type:
#                                 if t in type_dict:
#                                     python_types.add(type_dict.get(t))
#                                 else:
#                                     raise TypeError("Unknown resolving for type {0}".format(t))
#                             if type(obj_to_eval[k]) not in python_types:
#                                 raise TypeError("Type of object field '{0}' is not allowed".format(k))
#
#                         else:
#                             # this is a single type field
#                             python_type = type_dict.get(tmp_type)
#                             if not isinstance(obj_to_eval[k], python_type):
#                                 raise TypeError("Type of object field '{0}' is not allowed".format(k))
#                             else:
#                                 # WE'RE OK!
#                                 result_obj[k] = obj_to_eval[k]
#                     else:
#                         # this is an object field that wasn't specified by the user
#                         if v.get('has_default'):
#                             # WE'RE OK!
#                             result_obj[k] = v.get('default')
#                         else:
#                             # This is a mandatory field!
#                             raise ValueError("The {0} field is mandatory and must be specified explicitly".format(v))
#                 return result_obj
#
#             elif isinstance(ref_item, list):
#                 return []
#             else:
#
#
#
#         else:
#             raise KeyError("The given key is not ")
#
#
#
#
#                     pass
#                 pass
#             elif isinstance(obj_to_eval, list):
#                 # iterate as list sequence
#                 pass
#             else:
#                 # evaluate as single item
#                 pass
#         pass
#
#     try:
#         yaml_ref = yaml.load(file(yaml_reference_file_path, 'r'))
#         result = validator_rec_helper(evaluated_obj, yaml_ref, root_obj)
#
#     except yaml.YAMLError as e:
#         raise
#     except Exception:
#         raise

def yaml_loader(file_path):
    pass

def yaml_exporter(file_path):
    pass

if __name__ == "__main__":
    pass
