"""Hello Analytics Reporting API V4."""

import argparse

from apiclient.discovery import build
from oauth2client.service_account import ServiceAccountCredentials

import httplib2
from oauth2client import client
from oauth2client import file
from oauth2client import tools

from pprint import pprint
import time

SCOPES = ['https://www.googleapis.com/auth/analytics.readonly']
DISCOVERY_URI = ('https://analyticsreporting.googleapis.com/$discovery/rest')
KEY_FILE_LOCATION = '/auto/srg-sce-swinfra-usr/emb/users/itraviv/GoogleAnalytics/GA_ReportingAPI/My Project-da37fc42de8f.p12'
SERVICE_ACCOUNT_EMAIL = 'trex-cisco@i-jet-145907.iam.gserviceaccount.com'
VIEW_ID = '120207451'


def initialize_analyticsreporting():
  """Initializes an analyticsreporting service object.

  Returns:
    analytics an authorized analyticsreporting service object.
  """

  credentials = ServiceAccountCredentials.from_p12_keyfile(
    SERVICE_ACCOUNT_EMAIL, KEY_FILE_LOCATION, scopes=SCOPES)

  http = credentials.authorize(httplib2.Http())

  # Build the service object.
  analytics = build('analytics', 'v4', http=http, discoveryServiceUrl=DISCOVERY_URI)

  return analytics


def get_report(analytics,start_date='2016-11-06',end_date='2016-11-13'):
  # Use the Analytics Service Object to query the Analytics Reporting API V4.
  return analytics.reports().batchGet(
      body={
        'reportRequests': [
        {
          'viewId': VIEW_ID,
          'dateRanges': [{'startDate': start_date, 'endDate': end_date}],
          'metrics': [{'expression': 'ga:metric1','formattingType':'CURRENCY'},
					  {'expression': 'ga:metric2','formattingType':'CURRENCY'},
					  {'expression': 'ga:metric3','formattingType':'CURRENCY'},
					  {'expression': 'ga:totalEvents'}],
          'dimensions': [{"name":"ga:eventAction"},{"name": "ga:dimension1"},{"name": "ga:dimension2"},{"name": "ga:dimension3"},{"name": "ga:dimension4"}]
        }
        ]
      }
  ).execute()


def print_response(response):
  """Parses and prints the Analytics Reporting API V4 response"""

  for report in response.get('reports', []):
    columnHeader = report.get('columnHeader', {})
    dimensionHeaders = columnHeader.get('dimensions', [])
    metricHeaders = columnHeader.get('metricHeader', {}).get('metricHeaderEntries', [])
    rows = report.get('data', {}).get('rows', [])

    for row in rows:
      dimensions = row.get('dimensions', [])
      dateRangeValues = row.get('metrics', [])

      for header, dimension in zip(dimensionHeaders, dimensions):
        print header + ': ' + dimension

      for i, values in enumerate(dateRangeValues):
        print 'Date range (' + str(i) + ')'
        for metricHeader, value in zip(metricHeaders, values.get('values')):
          print metricHeader.get('name') + ': ' + value


def export_to_dict(response):
	df = {'Test_name':[],'State':[],'Setup':[],'Test_type':[],'MPPS':[],'MPPS-Golden min':[],'MPPS-Golden max':[]}
	for report in response.get('reports', []):
		rows = report.get('data', {}).get('rows', [])
		for row in rows:
			dimensions = row.get('dimensions', [])
			# print 'this is dimensions'
			# print dimensions
			df['Test_name'].append(dimensions[1])
			df['State'].append(dimensions[2])
			df['Setup'].append(dimensions[3])
			df['Test_type'].append(dimensions[4])
			dateRangeValues = row.get('metrics', [])
		 	value = dateRangeValues[0].get('values',[])[0]
			golden_min = dateRangeValues[0].get('values',[])[1]
			golden_max = dateRangeValues[0].get('values',[])[2]
			# print value
			df['MPPS'].append(value)
			df['MPPS-Golden min'].append(golden_min)
			df['MPPS-Golden max'].append(golden_max)
	return df


def export_to_tuples(response):
	setups = set()
	df = {}
	for report in response.get('reports', []):
		rows = report.get('data', {}).get('rows', [])
		for row in rows:
			data = []
			dimensions = row.get('dimensions', [])
			# print 'this is dimensions'
			# print dimensions
			data.append(dimensions[1]) #test name
			data.append(dimensions[2]) # state
			# data.append(dimensions[3]) # setup
			data.append(dimensions[4]) # test_type
			dateRangeValues = row.get('metrics', [])
		 	value = dateRangeValues[0].get('values',[])[0] #MPPS
			golden_min = dateRangeValues[0].get('values',[])[1] #golden min
			golden_max = dateRangeValues[0].get('values',[])[2] #golden max
			data.append(value)
			data.append(golden_min)
			data.append(golden_max)
			if dimensions[3] in setups:
				if dimensions[1] in df[dimensions[3]]:
					df[dimensions[3]][dimensions[1]].append(tuple(data))
				else:
					df[dimensions[3]][dimensions[1]] = [tuple(data)]
			else:
				df[dimensions[3]] = {}
				df[dimensions[3]][dimensions[1]] = [tuple(data)]
				setups.add(dimensions[3])
	return df, setups


def main():
	analytics = initialize_analyticsreporting()
	response = get_report(analytics)
	print_response(response)
	g_dict = export_to_dict(response)
	print g_dict
	pprint(g_dict)

    #pprint(response)
if __name__ == '__main__':
  main()

"""
    response = {u'reports': [{u'columnHeader': {u'dimensions': [u'ga:dimension1',
                                                 u'ga:dimension2',
                                                 u'ga:dimension3',
                                                 u'ga:dimension4'],
                                 u'metricHeader': {u'metricHeaderEntries': [{u'name': u'ga:metric1',
                                                                             u'type': u'CURRENCY'}]}},
               u'data': {u'isDataGolden': True,
                         u'maximums': [{u'values': [u'8532.0']}],
                         u'minimums': [{u'values': [u'2133.0']}],
                         u'rowCount': 4,
                         u'rows': [{u'dimensions': [u'test_name_to_date_9-10-161',
                                                    u'State_Less',
                                                    u'Setup_Name1',
                                                    u'Test_Type'],
                                    u'metrics': [{u'values': [u'2133.0']}]},
                                   {u'dimensions': [u'test_name_to_date_9-10-162',
                                                    u'State_Less',
                                                    u'Setup_Name2',
                                                    u'Test_Type'],
                                    u'metrics': [{u'values': [u'4266.0']}]},
                                   {u'dimensions': [u'test_name_to_date_9-10-163',
                                                    u'State_Less',
                                                    u'Setup_Name3',
                                                    u'Test_Type'],
                                    u'metrics': [{u'values': [u'6399.0']}]},
                                   {u'dimensions': [u'test_name_to_date_9-10-164',
                                                    u'State_Less',
                                                    u'Setup_Name4',
                                                    u'Test_Type'],
                                    u'metrics': [{u'values': [u'8532.0']}]}],
                         u'totals': [{u'values': [u'21330.0']}]}}]}
	"""