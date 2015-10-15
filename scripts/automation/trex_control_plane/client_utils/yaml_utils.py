
'''
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
'''

import external_packages
import yaml

def yaml_obj_validator(evaluated_obj, yaml_reference_file_path, root_obj, fill_defaults=True):
    """
    validate SINGLE ROOT object with yaml reference file.
    Fills up default values if hasn't been assigned by user and available.

    :param evaluated_obj: python object that should match the yaml reference file
    :param yaml_reference_file_path:
    :param fill_defaults:
    :return: a python representation object of the YAML file if OK
    """
    type_dict = {"double":float,
                 "int":int,
                 "array":list,
                 "string":str,
                 "boolean":bool}

    def validator_rec_helper(obj_to_eval, ref_obj, root_key):
        ref_item = ref_obj.get(root_key)
        if ref_item is not None:
            if "type" in obj_to_eval:
                ref_item = ref_item[obj_to_eval.get("type")]
            if isinstance(ref_item, dict) and "type" not in ref_item:   # this is not a terminal
                result_obj = {}
                # iterate over key-value pairs
                for k, v in ref_item.items():
                    if k in obj_to_eval:
                        # need to validate with ref obj
                        tmp_type = v.get('type')
                        if tmp_type == "object":
                            # go deeper into nesting hierarchy
                            result_obj[k] = validator_rec_helper(obj_to_eval.get(k), ref_obj, k)
                        elif isinstance(tmp_type, list):
                            # item can be one of multiple types
                            python_types = set()
                            for t in tmp_type:
                                if t in type_dict:
                                    python_types.add(type_dict.get(t))
                                else:
                                    raise TypeError("Unknown resolving for type {0}".format(t))
                            if type(obj_to_eval[k]) not in python_types:
                                raise TypeError("Type of object field '{0}' is not allowed".format(k))

                        else:
                            # this is a single type field
                            python_type = type_dict.get(tmp_type)
                            if not isinstance(obj_to_eval[k], python_type):
                                raise TypeError("Type of object field '{0}' is not allowed".format(k))
                            else:
                                # WE'RE OK!
                                result_obj[k] = obj_to_eval[k]
                    else:
                        # this is an object field that wasn't specified by the user
                        if v.get('has_default'):
                            # WE'RE OK!
                            result_obj[k] = v.get('default')
                        else:
                            # This is a mandatory field!
                            raise ValueError("The {0} field is mandatory and must be specified explicitly".format(v))
                return result_obj
            elif isinstance(ref_item, list):
                return []
            else:



        else:
            raise KeyError("The given key is not ")




                    pass
                pass
            elif isinstance(obj_to_eval, list):
                # iterate as list sequence
                pass
            else:
                # evaluate as single item
                pass
        pass

    try:
        yaml_ref = yaml.load(file(yaml_reference_file_path, 'r'))
        result = validator_rec_helper(evaluated_obj, yaml_ref, root_obj)

    except yaml.YAMLError as e:
        raise
    except Exception:
        raise

def yaml_loader(file_path):
    pass

def yaml_exporter(file_path):
    pass

if __name__ == "__main__":
    pass
