import os
import sys
import AnalyticsConnect as ac
import TRexDataAnalysis as tr
import time


def main():
	analytics = ac.initialize_analyticsreporting()
	# print 'retrieving data from Google Analytics'
	current_date = time.strftime("%Y-%m-%d")
	response = ac.get_report(analytics, '2016-11-06', current_date)
	ga_all_data_dict, setups = ac.export_to_tuples(response)
	tr.create_all_data(ga_all_data_dict, setups, '2016-11-06', current_date, save_path=os.getcwd() + '/images/',
					   add_stats='yes')

if __name__ == "__main__":
	main()
