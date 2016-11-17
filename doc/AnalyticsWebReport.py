import os
import sys
import AnalyticsConnect as ac
import TRexDataAnalysis as tr
import time


def main(verbose = False):
    if verbose:
        print('Retrieving data from Google Analytics')
    analytics = ac.initialize_analyticsreporting()
    current_date = time.strftime("%Y-%m-%d")
    response = ac.get_report(analytics, '2016-11-06', current_date)
    ga_all_data_dict, setups = ac.export_to_tuples(response)
    dest_path = os.path.join(os.getcwd(), 'build', 'images')
    if verbose:
        print('Saving data to %s' % dest_path)
    tr.create_all_data(ga_all_data_dict, setups, '2016-11-06', current_date, save_path = dest_path,
                       add_stats='yes')
    if verbose:
        print('Done without errors.')

if __name__ == "__main__":
    main()
