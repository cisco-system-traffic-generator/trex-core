#!/router/bin/python

from texttable import Texttable
import yaml


def handle_data_items(field_yaml_dict):
	data_items = field_yaml_dict['data']
	return [ [json_key, meaning['type'], meaning['exp'] ] 
			 for json_key,meaning in data_items.items() ]


def json_dict_to_txt_table(dict_yaml_file):

	# table = Texttable(max_width=120)
	with open(dict_yaml_file, 'r') as f:
		yaml_stream = yaml.safe_load(f)

	for main_field, sub_key in yaml_stream.items():
		print(main_field + ' field' '\n' + '~'*len(main_field+' field') + '\n')

		field_data_rows = handle_data_items(sub_key)
		table = Texttable(max_width=120)
		table.set_cols_align(["l", "c", "l"])
		table.set_cols_valign(["t", "m", "m"])
		# create table's header 
		table.add_rows([["Sub-key", "Type", "Meaning"]])
		table.add_rows(field_data_rows, header=False)


		print(table.draw() + "\n")





json_dict_to_txt_table("json_dictionary.yaml")