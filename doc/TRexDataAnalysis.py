#!/scratch/Anaconda2.4.0/bin/python
import pandas as pd
import numpy as np
import matplotlib

matplotlib.use('Agg')
from matplotlib import pyplot as plt
import os
import time


def generate_dframe_for_test(setup_name, test_name, test_data):
    test_results = []
    test_dates = []
    test_build_ids = []
    test_mins = set()
    test_maxs = set()
    for query in test_data:
        test_results.append(float(query[5]))
        date_formatted = time.strftime("%d-%m-%Y", time.strptime(query[2], "%Y%m%d"))
        time_of_res = date_formatted + '-' + query[3] + ':' + query[4]
        test_dates.append(time_of_res)
        test_build_ids.append(query[8])
        test_mins.add(float(query[6]))
        test_maxs.add(float(query[7]))
    df = pd.DataFrame({test_name: test_results})
    df_detailed = pd.DataFrame({(test_name + ' Results'): test_results, (test_name + ' Date'): test_dates,
                                "Setup": ([setup_name] * len(test_results)), "Build Id": test_build_ids})
    stats = tuple([float(df.mean()), min(test_mins), max(test_maxs)])  # stats = (avg_mpps,min,max)
    return df, stats, df_detailed


def generate_dframe_arr_and_stats_of_tests_per_setup(date, setup_name, setup_dict):
    dframe_arr_trend = []
    stats_arr = []
    dframe_arr_latest = []
    dframe_arr_detailed = []
    test_names = setup_dict.keys()
    for test in test_names:
        df, stats, df_detailed = generate_dframe_for_test(setup_name, test, setup_dict[test])
        dframe_arr_detailed.append(df_detailed)
        dframe_arr_trend.append(df)
        stats_arr.append(stats)
        df_latest = float(setup_dict[test][-1][6])
        dframe_arr_latest.append(df_latest)
    dframe_arr_latest = pd.DataFrame({'Date': [date] * len(dframe_arr_latest),
                                      'Setup': [setup_name],
                                      'Test Name': test_names,
                                      'MPPS': dframe_arr_latest},
                                     index=range(1, len(dframe_arr_latest) + 1))
    stats_df = pd.DataFrame(stats_arr, index=setup_dict.keys(), columns=['Avg MPPS', 'Golden Min', 'Golden Max'])
    stats_df.index.name = 'Test Name'
    return dframe_arr_trend, stats_df, dframe_arr_latest, dframe_arr_detailed


def create_plot_for_dframe_arr(dframe_arr, setup_name, start_date, end_date, show='no', save_path='',
                               file_name='_trend_graph'):
    dframe_all = pd.concat(dframe_arr, axis=1)
    dframe_all = dframe_all.astype(float)
    dframe_all.plot()
    plt.legend(fontsize='small', loc='best')
    plt.ylabel('MPPS')
    plt.title('Setup: ' + setup_name)
    plt.tick_params(
        axis='x',
        which='both',
        bottom='off',
        top='off',
        labelbottom='off')
    plt.xlabel('Time Period: ' + start_date + ' - ' + end_date)
    if save_path:
        plt.savefig(os.path.join(save_path, setup_name + file_name + '.png'))
    if show == 'yes':
        plt.show()


def create_bar_plot_for_latest_runs_per_setup(dframe_all_tests_latest, setup_name, show='no', save_path=''):
    plt.figure()
    dframe_all_tests_latest['MPPS'].plot(kind='bar', legend=False)
    dframe_all_tests_latest = dframe_all_tests_latest[['Test Name', 'Setup', 'Date', 'MPPS']]
    plt.xticks(rotation='horizontal')
    plt.xlabel('Index of Tests')
    plt.ylabel('MPPS')
    plt.title("Test Runs for Setup: " + setup_name)
    if save_path:
        plt.savefig(os.path.join(save_path, setup_name + '_latest_test_runs.png'))
        dframe_all_tests_latest = dframe_all_tests_latest.round(2)
        dframe_all_tests_latest.to_csv(os.path.join(save_path, setup_name + '_latest_test_runs_stats.csv'))
    if show == 'yes':
        plt.show()


def create_all_data_per_setup(setup_dict, setup_name, start_date, end_date, show='no', save_path='', add_stats='',
                              detailed_test_stats=''):
    dframe_arr, stats_arr, dframe_latest_arr, dframe_detailed = generate_dframe_arr_and_stats_of_tests_per_setup(
        end_date, setup_name,
        setup_dict)
    if detailed_test_stats:
        detailed_table = create_detailed_table(dframe_detailed, setup_name, save_path)
    else:
        detailed_table = []
    create_bar_plot_for_latest_runs_per_setup(dframe_latest_arr, setup_name, show=show, save_path=save_path)
    create_plot_for_dframe_arr(dframe_arr, setup_name, start_date, end_date, show, save_path)
    if add_stats:
        stats_arr = stats_arr.round(2)
        stats_arr.to_csv(os.path.join(save_path, setup_name + '_trend_stats.csv'))
    plt.close('all')
    return detailed_table


def create_detailed_table(dframe_arr_detailed, setup_name, save_path=''):
    result = reduce(lambda x, y: pd.merge(x, y, on=('Build Id', 'Setup')), dframe_arr_detailed)
    return result


# WARNING: if the file _all_stats.csv already exists, this script deletes it, to prevent overflowing of data
#  since data is appended to the file
def create_all_data(ga_data, setup_names, start_date, end_date, save_path='', add_stats='', detailed_test_stats=''):
    total_detailed_data = []
    if detailed_test_stats:
        if os.path.exists(os.path.join(save_path, '_detailed_table.csv')):
            os.remove(os.path.join(save_path, '_detailed_table.csv'))
    for setup_name in setup_names:
        if setup_name == 'trex11':
            continue
        detailed_setup_data = create_all_data_per_setup(ga_data[setup_name], setup_name, start_date, end_date,
                                                        show='no', save_path=save_path,
                                                        add_stats=add_stats, detailed_test_stats=detailed_test_stats)
        total_detailed_data.append(detailed_setup_data)
    if detailed_test_stats:
        total_detailed_dframe = pd.DataFrame().append(total_detailed_data)
        total_detailed_dframe.to_csv(os.path.join(save_path, '_detailed_table.csv'))
