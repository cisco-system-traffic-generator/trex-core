#!/scratch/Anaconda2.4.0/bin/python
import pandas as pd
import numpy as np
import matplotlib
from enum import IntEnum

matplotlib.use('Agg')
from matplotlib import pyplot as plt
import os
import time

"""
This Module is structured to work with a raw data at the following JSON format:

 {'setup_name': {'test1_name':[QUERY1,QUERY2,QUERY3],
                'test2_name':[QUERY1,QUERY2,QUERY3]
                }
  'setup_name2': {'test1_name':[QUERY1,QUERY2,QUERY3],
                'test2_name':[QUERY1,QUERY2,QUERY3]
                }
 }

 The Query structure is set (currently) to this:
 (test_name,state, date,hour,minute,mpps_result,mpps_min,mpps_max,build_id)

 it can be changed to support other formats of queries, simply change the enum class to support your desired structure
 the enums specify the indexes of the data within the query tuple

"""


class TestQuery(IntEnum):
    QUERY_DATE = 2  # date format is yyyymmdd
    QUERY_HOUR = 3
    QUERY_MINUTE = 4
    QUERY_MPPS_RESULT = 5
    QUERY_TEST_MIN = 6
    QUERY_TEST_MAX = 7
    QUERY_BUILD_ID = 8


class Test:
    def __init__(self, name, setup_name, end_date):
        self.name = name
        self.setup_name = setup_name
        self.end_date = end_date
        self.stats = []  # tuple
        self.results_df = []  # dataFrame
        self.latest_result = []  # float
        self.latest_result_date = ''  # string

    def analyze_all_test_data(self, raw_test_data):
        test_results = []
        test_dates = []
        test_build_ids = []
        test_mins = set()
        test_maxs = set()
        for query in raw_test_data:
            date_formatted = time.strftime("%d-%m-%Y", time.strptime(query[int(TestQuery.QUERY_DATE)], "%Y%m%d"))
            time_of_res = date_formatted + '-' + query[int(TestQuery.QUERY_HOUR)] + ':' + query[
                int(TestQuery.QUERY_MINUTE)]
            test_dates.append(time_of_res)
            test_results.append(float(query[int(TestQuery.QUERY_MPPS_RESULT)]))
            test_build_ids.append(query[int(TestQuery.QUERY_BUILD_ID)])
            test_mins.add(float(query[int(TestQuery.QUERY_TEST_MIN)]))
            test_maxs.add(float(query[int(TestQuery.QUERY_TEST_MAX)]))
        test_results_df = pd.DataFrame({self.name: test_results, self.name + ' Date': test_dates,
                                        "Setup": ([self.setup_name] * len(test_results)), "Build Id": test_build_ids},
                                       dtype='str')
        stats = tuple(
            [float(test_results_df[self.name].mean()), min(test_mins), max(test_maxs)])  # stats = (avg_mpps,min,max)
        self.latest_result = float(test_results_df[self.name].iloc[-1])
        self.latest_result_date = str(test_results_df[test_results_df.columns[3]].iloc[-1])
        self.results_df = test_results_df
        self.stats = stats


class Setup:
    def __init__(self, name, start_date, end_date, raw_setup_data):
        self.name = name
        self.start_date = start_date  # string of date
        self.end_date = end_date  # string of date
        self.tests = []  # list of test objects
        self.all_tests_data_table = pd.DataFrame()  # dataframe
        self.setup_trend_stats = pd.DataFrame()  # dataframe
        self.latest_test_results = pd.DataFrame()  # dataframe
        self.raw_setup_data = raw_setup_data  # dictionary
        self.test_names = raw_setup_data.keys()  # list of names

    def analyze_all_tests(self):
        for test_name in self.test_names:
            t = Test(test_name, self.name, self.end_date)
            t.analyze_all_test_data(self.raw_setup_data[test_name])
            self.tests.append(t)

    def analyze_latest_test_results(self):
        test_names = []
        test_dates = []
        test_latest_results = []
        for test in self.tests:
            test_names.append(test.name)
            test_dates.append(test.latest_result_date)
            test_latest_results.append(test.latest_result)
        self.latest_test_results = pd.DataFrame(
            {'Date': test_dates, 'Test Name': test_names, 'MPPS\Core (Norm)': test_latest_results},
            index=range(1, len(test_latest_results) + 1))
        self.latest_test_results = self.latest_test_results[[2, 1, 0]]  # re-order columns to name|MPPS|date

    def analyze_all_tests_stats(self):
        test_names = []
        all_test_stats = []
        for test in self.tests:
            test_names.append(test.name)
            all_test_stats.append(test.stats)
        self.setup_trend_stats = pd.DataFrame(all_test_stats, index=test_names,
                                              columns=['Avg MPPS/Core (Norm)', 'Golden Min', 'Golden Max'])
        self.setup_trend_stats.index.name = 'Test Name'

    def analyze_all_tests_trend(self):
        all_tests_trend_data = []
        for test in self.tests:
            all_tests_trend_data.append(test.results_df)
        self.all_tests_data_table = reduce(lambda x, y: pd.merge(x, y, how='outer'), all_tests_trend_data)

    def plot_trend_graph_all_tests(self, save_path='', file_name='_trend_graph.png'):
        for test_name in self.test_names:
            self.all_tests_data_table[test_name].plot(style=['.-'])
            plt.legend(fontsize='small', loc='best')
        plt.ylabel('MPPS/Core (Norm)')
        plt.title('Setup: ' + self.name)
        plt.tick_params(
            axis='x',
            which='both',
            bottom='off',
            top='off',
            labelbottom='off')
        plt.xlabel('Time Period: ' + self.start_date + ' - ' + self.end_date)
        if save_path:
            plt.savefig(os.path.join(save_path, self.name + file_name))
            if not self.setup_trend_stats.empty:
                (self.setup_trend_stats.round(2)).to_csv(os.path.join(save_path, self.name +
                                                                      '_trend_stats.csv'))
            plt.close('all')

    def plot_latest_test_results_bar_chart(self, save_path='', img_file_name='_latest_test_runs.png',
                                           stats_file_name='_latest_test_runs_stats.csv'):
        plt.figure()
        colors_for_bars = ['b', 'g', 'r', 'c', 'm', 'y']
        self.latest_test_results[[1]].plot(kind='bar', legend=False,
                                           color=colors_for_bars)  # plot only mpps data, which is in column 1
        plt.xticks(rotation='horizontal')
        plt.xlabel('Index of Tests')
        plt.ylabel('MPPS/Core (Norm)')
        plt.title("Test Runs for Setup: " + self.name)
        if save_path:
            plt.savefig(os.path.join(save_path, self.name + img_file_name))
            (self.latest_test_results.round(2)).to_csv(
                os.path.join(save_path, self.name + stats_file_name))
        plt.close('all')

    def analyze_all_setup_data(self):
        self.analyze_all_tests()
        self.analyze_latest_test_results()
        self.analyze_all_tests_stats()
        self.analyze_all_tests_trend()

    def plot_all(self, save_path=''):
        self.plot_latest_test_results_bar_chart(save_path)
        self.plot_trend_graph_all_tests(save_path)


def latest_runs_comparison_bar_chart(setup_name1, setup_name2, setup1_latest_result, setup2_latest_result,
                                     save_path=''
                                     ):
    s1_res = setup1_latest_result[[0, 1]]  # column0 is test name, column1 is MPPS\Core
    s2_res = setup2_latest_result[[0, 1, 2]]  # column0 is test name, column1 is MPPS\Core, column2 is Date
    s1_res.columns = ['Test Name', setup_name1]
    s2_res.columns = ['Test Name', setup_name2, 'Date']
    compare_dframe = pd.merge(s1_res, s2_res, on='Test Name')
    compare_dframe.plot(kind='bar')
    plt.legend(fontsize='small', loc='best')
    plt.xticks(rotation='horizontal')
    plt.xlabel('Index of Tests')
    plt.ylabel('MPPS/Core (Norm)')
    plt.title("Comparison between " + setup_name1 + " and " + setup_name2)
    if save_path:
        plt.savefig(os.path.join(save_path, "_comparison.png"))
        compare_dframe = compare_dframe.round(2)
        compare_dframe.to_csv(os.path.join(save_path, '_comparison_stats_table.csv'))

        # WARNING: if the file _all_stats.csv already exists, this script deletes it, to prevent overflowing of data


def create_all_data(ga_data, start_date, end_date, save_path='', detailed_test_stats=''):
    all_setups = {}
    all_setups_data = []
    setup_names = ga_data.keys()
    for setup_name in setup_names:
        s = Setup(setup_name, start_date, end_date, ga_data[setup_name])
        s.analyze_all_setup_data()
        s.plot_all(save_path)
        all_setups_data.append(s.all_tests_data_table)
        all_setups[setup_name] = s

    if detailed_test_stats:
        if os.path.exists(os.path.join(save_path, '_detailed_table.csv')):
            os.remove(os.path.join(save_path, '_detailed_table.csv'))
        all_setups_data_dframe = pd.DataFrame().append(all_setups_data)
        all_setups_data_dframe.to_csv(os.path.join(save_path, '_detailed_table.csv'))

    trex07setup = all_setups['trex07']
    trex08setup = all_setups['trex08']
    latest_runs_comparison_bar_chart('Mellanox ConnectX-4',
                                     'Intel XL710', trex07setup.latest_test_results,
                                     trex08setup.latest_test_results,
                                     save_path=save_path)
