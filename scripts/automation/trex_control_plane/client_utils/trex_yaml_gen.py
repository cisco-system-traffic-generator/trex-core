#!/router/bin/python

import pprint
import yaml
import os
# import bisect

class CTRexYaml(object):
    """
    This class functions as a YAML generator according to TRex YAML format.

    CTRexYaml is compatible with both Python 2 and Python 3.
    """
    YAML_TEMPLATE = [{'cap_info': [],
                      'duration': 10.0,
                      'generator': {'clients_end': '16.0.1.255',
                                    'clients_per_gb': 201,
                                    'clients_start': '16.0.0.1',
                                    'distribution': 'seq',
                                    'dual_port_mask': '1.0.0.0',
                                    'min_clients': 101,
                                    'servers_end': '48.0.0.255',
                                    'servers_start': '48.0.0.1',
                                    'tcp_aging': 1,
                                    'udp_aging': 1},
                      'mac' :  [0x00,0x00,0x00,0x01,0x00,0x00]}]
    PCAP_TEMPLATE = {'cps': 1.0,
                    'ipg': 10000,
                    'name': '',
                    'rtt': 10000,
                    'w': 1}

    def __init__ (self, trex_files_path):
        """
        The initialization of this class creates a CTRexYaml object with **empty** 'cap-info', and with default client-server configuration.

        Use class methods to add and assign pcap files and export the data to a YAML file.

        :parameters:  
            trex_files_path : str
                a path (on TRex server side) for the pcap files using which TRex can access it.

        """
        self.yaml_obj        = list(CTRexYaml.YAML_TEMPLATE)
        self.empty_cap       = True
        self.file_list       = []
        self.yaml_dumped     = False
        self.trex_files_path = trex_files_path

    def add_pcap_file (self, local_pcap_path):
        """
        Adds a .pcap file with recorded traffic to the yaml object by linking the file with 'cap-info' template key fields.
                
        :parameters:  
            local_pcap_path : str
                a path (on client side) for the pcap file to be added.

        :return: 
            + The index of the inserted item (as int) if item added successfully
            + -1 if pcap file already exists in 'cap_info'.

        """
        new_pcap = dict(CTRexYaml.PCAP_TEMPLATE)
        new_pcap['name'] = self.trex_files_path + os.path.basename(local_pcap_path)
        if self.get_pcap_idx(new_pcap['name']) != -1:
            # pcap already exists in 'cap_info'
            return -1
        else:
            self.yaml_obj[0]['cap_info'].append(new_pcap)
            if self.empty_cap:
                self.empty_cap = False
            self.file_list.append(local_pcap_path)
            return ( len(self.yaml_obj[0]['cap_info']) - 1)


    def get_pcap_idx (self, pcap_name):
        """
        Checks if a certain .pcap file has been added into the yaml object.
                
        :parameters:  
            pcap_name : str
                the name of the pcap file to be searched

        :return: 
            + The index of the pcap file (as int) if exists
            + -1 if not exists.

        """
        comp_pcap = pcap_name if pcap_name.startswith(self.trex_files_path) else (self.trex_files_path + pcap_name)
        for idx, pcap in enumerate(self.yaml_obj[0]['cap_info']):
            print (pcap['name'] == comp_pcap)
            if pcap['name'] == comp_pcap:
                return idx
        # pcap file wasn't found
        return -1

    def dump_as_python_obj (self):
        """
        dumps with nice indentation the pythonic format (dictionaries and lists) of the currently built yaml object.
                
        :parameters:  
            None

        :return: 
            None

        """
        pprint.pprint(self.yaml_obj)

    def dump(self):
        """
        dumps with nice indentation the YAML format of the currently built yaml object.
                
        :parameters:  
            None

        :return:
            None

        """
        print (yaml.safe_dump(self.yaml_obj, default_flow_style = False))

    def to_yaml(self, filename):
        """
        Exports to YAML file the built configuration into an actual YAML file.
                
        :parameters:  
            filename : str
                a path (on client side, including filename) to store the generated yaml file.

        :return: 
            None

        :raises:
            + :exc:`ValueError`, in case no pcap files has been added to the object.
            + :exc:`EnvironmentError`, in case of any IO error of writing to the files or OSError when trying to open it for writing.
        
        """
        if self.empty_cap:
            raise ValueError("No .pcap file has been assigned to yaml object. Must add at least one")
        else:
            try:
                with open(filename, 'w') as yaml_file:
                    yaml_file.write( yaml.safe_dump(self.yaml_obj, default_flow_style = False) )
                self.yaml_dumped = True
                self.file_list.append(filename)
            except EnvironmentError as inst:
                raise 

    def set_cap_info_param (self, param, value, seq):
        """
        Set cap-info parameters' value of a specific pcap file.
                
        :parameters:  
            param : str
                the name of the parameters to be set.
            value : int/float
                the desired value to be set to `param` key.
            seq : int
                an index to the relevant caps array to be changed (index supplied when adding new pcap file, see :func:`add_pcap_file`).

        :return: 
            **True** on success

        :raises:
            :exc:`IndexError`, in case an out-of range index was given.
        
        """
        try:
            self.yaml_obj[0]['cap_info'][seq][param] = value

            return True
        except IndexError:
            return False

    def set_generator_param (self, param, value):
        """
        Set generator parameters' value of the yaml object.
                
        :parameters:  
            param : str
                the name of the parameters to be set.
            value : int/float/str
                the desired value to be set to `param` key.

        :return: 
            None
        
        """
        self.yaml_obj[0]['generator'][param] = value

    def get_file_list(self):
        """
        Returns a list of all files related to the YAML object, including the YAML filename itself.

        .. tip:: This method is especially useful for listing all the files that should be pushed to TRex server as part of the same yaml selection.
                
        :parameters:  
            None

        :return: 
            a list of filepaths, each is a local client-machine file path.
        
        """
        if not self.yaml_dumped:
            print ("WARNING: .yaml file wasn't dumped yet. Files list contains only .pcap files")
        return self.file_list



if __name__ == "__main__":
    pass
