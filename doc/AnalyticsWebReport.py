import os
import sys
import AnalyticsConnect as ac
import TRexDataAnalysisV2 as tr
import time
import datetime


def main(verbose=False, source='ga', detailed_test_stats='yes'):
    if source == 'ga':
        if verbose:
            print('Retrieving data from Google Analytics')
        analytics = ac.initialize_analyticsreporting()
        current_date = time.strftime("%Y-%m-%d")
        k_days_ago = datetime.datetime.now() - datetime.timedelta(days=15)
        start_date = str(k_days_ago.date())
        response = ac.get_report(analytics, start_date, current_date)
        all_data_dict, setups = ac.export_to_tuples(response)
    if source == 'elk':
        all_data_dict = 0  # INSERT JSON FROM ELK HERE
    dest_path = os.path.join(os.getcwd(), 'build', 'images')
    if verbose:
        print('Saving data to %s' % dest_path)
        if detailed_test_stats:
            print('generating detailed table for test results')
    tr.create_all_data(all_data_dict, start_date, current_date, save_path=dest_path,
                       detailed_test_stats=detailed_test_stats)
    if verbose:
        print('Done without errors.')


if __name__ == "__main__":
    main()
