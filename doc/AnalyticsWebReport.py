import os
import sys
import ELKConnect as ec
import AnalyticsConnect as ac
import TRexDataAnalysis as tr
import time
import datetime


def main(verbose=False, source='elk', detailed_test_stats='yes'):
    current_date = time.strftime("%Y-%m-%d")
    k_days_ago = datetime.datetime.now() - datetime.timedelta(days=15)
    start_date = str(k_days_ago.date())
    if source == 'ga':
        if verbose:
            print('Retrieving data from Google Analytics')
        analytics = ac.initialize_analyticsreporting()
        response = ac.get_report(analytics, start_date, current_date)
        all_data_dict, setups = ac.export_to_tuples(response)
    if source == 'elk':
        elk_manager = ec.ELKManager(hostname='sceasr-b20', index='trex_perf-000004', port=9200)
        all_data_dict = elk_manager.fetch_and_parse()
    dest_path = os.path.join(os.getcwd(), 'build', 'images')
    if verbose:
        print('Saving data to %s' % dest_path)
        if detailed_test_stats:
            print('generating detailed table for test results')
    tr.create_all_data(all_data_dict, current_date, save_path=dest_path,
                       detailed_test_stats=detailed_test_stats)
    if verbose:
        print('Done without errors.')


if __name__ == "__main__":
    main()
