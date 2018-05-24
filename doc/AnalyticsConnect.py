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


def get_report(analytics, start_date='2016-11-27', end_date='2016-11-27'):
    # Use the Analytics Service Object to query the Analytics Reporting API V4.
    return analytics.reports().batchGet(
        body={
            'reportRequests': [
                {
                    'viewId': VIEW_ID,
                    'dateRanges': [{'startDate': start_date, 'endDate': end_date}],
                    'metrics': [{'expression': 'ga:metric1', 'formattingType': 'CURRENCY'},
                                {'expression': 'ga:metric2', 'formattingType': 'CURRENCY'},
                                {'expression': 'ga:metric3', 'formattingType': 'CURRENCY'},
                                {'expression': 'ga:totalEvents'}],
                    'dimensions': [{"name": "ga:eventAction"}, {"name": "ga:dimension1"}, {"name": "ga:dimension2"},
                                   {"name": "ga:dimension3"},
                                   {"name": "ga:date"}, {"name": "ga:hour"}, {"name": "ga:minute"}],
                    'pageSize': 10000
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


def export_to_tuples(response):
    # counter = 0
    setups = set()
    df = {}
    for report in response.get('reports', []):
        rows = report.get('data', {}).get('rows', [])
        for row in rows:
            data = []
            dimensions = row.get('dimensions', [])
            # print 'this is dimensions'
            # print dimensions
            data.append(dimensions[1])  # test name
            data.append(dimensions[2])  # state
            # data.append(dimensions[3]) # setup
            data.append(dimensions[4])  # date in YYYYMMDD format
            data.append(dimensions[5])  # hour
            data.append(dimensions[6])  # minute
            dateRangeValues = row.get('metrics', [])
            value = dateRangeValues[0].get('values', [])[0]  # MPPS
            golden_min = dateRangeValues[0].get('values', [])[1]  # golden min
            golden_max = dateRangeValues[0].get('values', [])[2]  # golden max
            data.append(value)
            # counter += 1
            data.append(golden_min)
            data.append(golden_max)
            data.append(dimensions[0])  # build id
            if dimensions[3] in setups:
                if dimensions[1] in df[dimensions[3]]:
                    df[dimensions[3]][dimensions[1]].append(tuple(data))
                else:
                    df[dimensions[3]][dimensions[1]] = [tuple(data)]
            else:
                df[dimensions[3]] = {}
                df[dimensions[3]][dimensions[1]] = [tuple(data)]
                setups.add(dimensions[3])
    # print 'counter is: %d' % counter
    return df, setups


def main():
    analytics = initialize_analyticsreporting()
    response = get_report(analytics)
    df, setups = export_to_tuples(response)
    # pprint(df)
    return df, setups


if __name__ == '__main__':
    main()

"""
response structure (when fetched with "export to tuples"):

{ 'setup1': {'test_name1': [(test_res1),(test_res2),...], 'test_name2': [(test_res1),(test_res2),...]},
  'setup2': {'test_name1': [(test_res1),(test_res2),...], 'test_name2': [(test_res1),(test_res2),...]},
   .
   .
   .
   .
}

{u'kiwi02': {u'VM - 64 bytes, multi CPU, cache size 1024': [(u'VM - 64 bytes, multi CPU, cache size 1024',
                                                             u'stl',
                                                             u'performance',
                                                             u'19.711146',
                                                             u'19.0',
                                                             u'22.0'),
                                                            (u'VM - 64 bytes, multi CPU, cache size 1024',
                                                             u'stl',
                                                             u'performance',
                                                             u'19.581567',
                                                             u'19.0',
                                                             u'22.0')],
             u'VM - 64 bytes, multi CPUs': [(u'VM - 64 bytes, multi CPUs',
                                             u'stl',
                                             u'performance',
                                             u'10.398847',
                                             u'9.7',
                                             u'12.5'),
                                            (u'VM - 64 bytes, multi CPUs',
                                             u'stl',
                                             u'performance',
                                             u'10.925308',
                                             u'9.7',
                                             u'12.5')
                                            ]
            }
 u'trex07': {u'VM - 64 bytes, multi CPU, cache size 1024': [(u'VM - 64 bytes, multi CPU, cache size 1024',
                                                             u'stl',
                                                             u'performance',
                                                             u'25.078212',
                                                             u'9.0',
                                                             u'15.0')
                                                            ]
             u'VM - 64 bytes, multi CPUs': [(u'VM - 64 bytes, multi CPUs',
                                             u'stl',
                                             u'performance',
                                             u'9.469138',
                                             u'8.5',
                                             u'12.0')
                                            ]
            }
}



"""
