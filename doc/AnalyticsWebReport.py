import os
import sys
import AnalyticsConnect as ac
import TRexDataAnalysis as tr
import time
import datetime


def main(verbose = False):
    if verbose:
        print('Retrieving data from Google Analytics')
    analytics = ac.initialize_analyticsreporting()
    current_date = time.strftime("%Y-%m-%d")
    k_days_ago = datetime.datetime.now() - datetime.timedelta(days=15)
    start_date = str(k_days_ago.date())
    response = ac.get_report(analytics, start_date, current_date)
    ga_all_data_dict, setups = ac.export_to_tuples(response)
    dest_path = os.path.join(os.getcwd(), 'build', 'images')
    if verbose:
        print('Saving data to %s' % dest_path)
    tr.create_all_data(ga_all_data_dict, setups, start_date, current_date, save_path = dest_path,
                       add_stats='yes')
    if verbose:
        print('Done without errors.')

if __name__ == "__main__":
    main()
