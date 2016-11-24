#!/scratch/Anaconda2.4.0/bin/python
import pandas as pd
import numpy as np
import matplotlib
matplotlib.use('Agg')
from matplotlib import pyplot as plt
import os


def generate_dframe_for_test(test_name, test_data):
    test_results = []
    test_mins = set()
    test_maxs = set()
    for query in test_data:
        test_results.append(float(query[3]))
        test_mins.add(float(query[4]))
        test_maxs.add(float(query[5]))
    df = pd.DataFrame({test_name: test_results})
    stats = tuple([float(df.mean()), min(test_mins), max(test_maxs)])  # stats = (avg_mpps,min,max)
    return df, stats


def generate_dframe_arr_and_stats_of_tests_per_setup(date, setup_name, setup_dict):
    dframe_arr_trend = []
    stats_arr = []
    dframe_arr_latest = []
    test_names = setup_dict.keys()
    for test in test_names:
        df, stats = generate_dframe_for_test(test, setup_dict[test])
        dframe_arr_trend.append(df)
        stats_arr.append(stats)
        df_latest = float(setup_dict[test][-1][3])
        dframe_arr_latest.append(df_latest)
    dframe_arr_latest = pd.DataFrame({'Date': [date] * len(dframe_arr_latest),
                                      'Setup': [setup_name],
                                      'Test Name': test_names,
                                      'MPPS': dframe_arr_latest},
                                     index=range(1, len(dframe_arr_latest) + 1))
    stats_df = pd.DataFrame(stats_arr, index=setup_dict.keys(), columns=['Avg MPPS', 'Golden Min', 'Golden Max'])
    stats_df.index.name = 'Test Name'
    return dframe_arr_trend, stats_df, dframe_arr_latest


def create_plot_for_dframe_arr(dframe_arr, setup_name, start_date, end_date, show='no', save_path='',
                               file_name='trend_graph'):
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


def create_all_data_per_setup(setup_dict, setup_name, start_date, end_date, show='no', save_path='', add_stats=''):
    dframe_arr, stats_arr, dframe_latest_arr = generate_dframe_arr_and_stats_of_tests_per_setup(end_date, setup_name,
                                                                                                setup_dict)
    create_bar_plot_for_latest_runs_per_setup(dframe_latest_arr, setup_name, show=show, save_path=save_path)
    create_plot_for_dframe_arr(dframe_arr, setup_name, start_date, end_date, show, save_path)
    if add_stats:
        stats_arr = stats_arr.round(2)
        stats_arr.to_csv(os.path.join(save_path, setup_name + '_trend_stats.csv'))
    plt.close('all')


def create_all_data(ga_data, setup_names, start_date, end_date, save_path='', add_stats=''):
    for setup_name in setup_names:
        if setup_name == 'trex11':
            continue
        create_all_data_per_setup(ga_data[setup_name], setup_name, start_date, end_date, show='no', save_path=save_path,
                                  add_stats=add_stats)
