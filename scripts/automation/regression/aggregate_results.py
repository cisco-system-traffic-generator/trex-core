# -*- coding: utf-8 -*-
import xml.etree.ElementTree as ET
import argparse
import glob
from pprint import pprint
import sys, os
from collections import OrderedDict
import copy
import datetime, time
import cPickle as pickle
import subprocess, shlex

FUNCTIONAL_CATEGORY = 'Functional' # how to display those categories
ERROR_CATEGORY = 'Error'


def pad_tag(text, tag):
    return '<%s>%s</%s>' % (tag, text, tag)

def is_functional_test_name(testname):
    if testname.startswith('platform_') or testname.startswith('misc_methods_'):
        return True
    return False

def is_good_status(text):
    return text in ('Successful', 'Fixed', 'Passed', 'True', 'Pass')

# input: xml element with test result
# output string: 'error', 'failure', 'skipped', 'passed'
def get_test_result(test):
    for child in test.getchildren():
        if child.tag in ('error', 'failure', 'skipped'):
            return child.tag
    return 'passed'

# returns row of table with <th> and <td> columns - key: value
def add_th_td(key, value):
    return '<tr><th>%s</th><td>%s</td></tr>\n' % (key, value)

# returns row of table with <td> and <td> columns - key: value
def add_td_td(key, value):
    return '<tr><td>%s</td><td>%s</td></tr>\n' % (key, value)

# returns row of table with <th> and <th> columns - key: value
def add_th_th(key, value):
    return '<tr><th>%s</th><th>%s</th></tr>\n' % (key, value)

# returns <div> with table of tests under given category.
# category - string with name of category
# hidden - bool, true = <div> is hidden by CSS
# tests - list of tests, derived from aggregated xml report, changed a little to get easily stdout etc.
# category_info_dir - folder to search for category info file
# expanded - bool, false = outputs (stdout etc.) of tests are hidden by CSS
# brief - bool, true = cut some part of tests outputs (useful for errors section with expanded flag)
def add_category_of_tests(category, tests, hidden = False, category_info_dir = None, expanded = False, brief = False):
    is_actual_category = category not in (FUNCTIONAL_CATEGORY, ERROR_CATEGORY)
    html_output = '<div style="display:%s;" id="cat_tglr_%s">\n' % ('none' if hidden else 'block', category)
    
    if is_actual_category:
        html_output += '<br><table class="reference">\n'
        
        if category_info_dir:
            category_info_file = '%s/report_%s.info' % (category_info_dir, category)
            if os.path.exists(category_info_file):
                with open(category_info_file) as f:
                    for info_line in f.readlines():
                        key_value = info_line.split(':', 1)
                        if key_value[0].startswith('User'): # always 'hhaim', no need to show
                            continue
                        html_output += add_th_td('%s:' % key_value[0], key_value[1])
            else:
                html_output += add_th_td('Info:', 'No info')
                print 'add_category_of_tests: no category info %s' % category_info_file
        if len(tests):
            total_duration = 0.0
            for test in tests:
                total_duration += float(test.attrib['time'])
            html_output += add_th_td('Tests duration:', datetime.timedelta(seconds = int(total_duration)))
        html_output += '</table>\n'

    if not len(tests):
        return html_output + pad_tag('<br><font color=red>No tests!</font>', 'b') + '</div>'
    html_output += '<br>\n<table class="reference">\n<tr><th align="left">'

    if category == ERROR_CATEGORY:
        html_output += 'Setup</th><th align="left">Failed tests:'
    else:
        html_output += '%s tests:' % category
    html_output += '</th><th align="center">Final Result</th>\n<th align="center">Time (s)</th>\n</tr>\n'
    for test in tests:
        functional_test = is_functional_test_name(test.attrib['name'])
        if functional_test and is_actual_category:
            continue
        if category == ERROR_CATEGORY:
            test_id = ('err_' + test.attrib['classname'] + test.attrib['name']).replace('.', '_')
        else:
            test_id = (category + test.attrib['name']).replace('.', '_')
        if expanded:
            html_output += '<tr>\n<th>'
        else:
            html_output += '<tr onclick=tgl_test("%s") class=linktr>\n<td class=linktext>' % test_id
        if category == ERROR_CATEGORY:
            html_output += FUNCTIONAL_CATEGORY if functional_test else test.attrib['classname']
            if expanded:
                html_output += '</th><td>'
            else:
                html_output += '</td><td class=linktext>'
        html_output += '%s</td>\n<td align="center">' % test.attrib['name']
        test_result = get_test_result(test)
        if test_result == 'error':
            html_output += '<font color="red"><b>ERROR</b></font></td>'
        elif test_result == 'failure':
            html_output += '<font color="red"><b>FAILED</b></font></td>'
        elif test_result == 'skipped':
            html_output += '<font color="blue"><b>SKIPPED</b></font></td>'
        else:
            html_output += '<font color="green"><b>PASSED</b></font></td>'
        html_output += '<td align="center"> '+ test.attrib['time'] + '</td></center></tr>'

        result, result_text = test.attrib.get('result', ('', ''))
        if result_text:
            result_text = '<b style="color:000080;">%s:</b><br>%s<br><br>' % (result.capitalize(), result_text.replace('\n', '<br>'))
        stderr = '' if brief and result_text else test.get('stderr', '')
        if stderr:
            stderr = '<b style="color:000080;"><text color=000080>Stderr</text>:</b><br>%s<br><br>\n' % stderr.replace('\n', '<br>')
        stdout = '' if brief and result_text else test.get('stdout', '')
        if stdout:
            if brief: # cut off server logs
                stdout = stdout.split('>>>>>>>>>>>>>>>', 1)[0]
            stdout = '<b style="color:000080;">Stdout:</b><br>%s<br><br>\n' % stdout.replace('\n', '<br>')

        html_output += '<tr style="%scolor:603000;" id="%s"><td colspan=%s>' % ('' if expanded else 'display:none;', test_id, 4 if category == ERROR_CATEGORY else 3)
        if result_text or stderr or stdout:
            html_output += '%s%s%s</td></tr>' % (result_text, stderr, stdout)
        else:
            html_output += '<b style="color:000080;">No output</b></td></tr>'

    html_output += '\n</table>\n</div>'
    return html_output

# main
if __name__ == '__main__':

    # deal with input args
    argparser = argparse.ArgumentParser(description='Aggregate test results of from ./reports dir, produces xml, html, mail report.')
    argparser.add_argument('--input_dir', default='./reports',
                   help='Directory with xmls/setups info. Filenames: report_<setup name>.xml/report_<setup name>.info')
    argparser.add_argument('--output_xml', default='./reports/aggregated_tests.xml',
                   dest = 'output_xmlfile', help='Name of output xml file with aggregated results.')
    argparser.add_argument('--output_html', default='./reports/aggregated_tests.html',
                   dest = 'output_htmlfile', help='Name of output html file with aggregated results.')
    argparser.add_argument('--output_mail', default='./reports/aggregated_tests_mail.html',
                   dest = 'output_mailfile', help='Name of output html file with aggregated results for mail.')
    argparser.add_argument('--output_title', default='./reports/aggregated_tests_title.txt',
                   dest = 'output_titlefile', help='Name of output file to contain title of mail.')
    argparser.add_argument('--build_status_file', default='./reports/build_status',
                   dest = 'build_status_file', help='Name of output file to save scenaries build results (should not be wiped).')
    args = argparser.parse_args()


##### get input variables/TRex commit info

    scenario                = os.environ.get('SCENARIO')
    build_url               = os.environ.get('BUILD_URL')
    build_id                = os.environ.get('BUILD_ID')
    trex_last_commit_hash   = os.environ.get('TREX_LAST_COMMIT_HASH') # TODO: remove it, take from setups info
    trex_repo               = os.environ.get('TREX_CORE_REPO')
    if not scenario:
        print 'Warning: no environment variable SCENARIO, using default'
        scenario = 'TRex regression'
    if not build_url:
        print 'Warning: no environment variable BUILD_URL'
    if not build_id:
        print 'Warning: no environment variable BUILD_ID'
    trex_last_commit_info = ''
    if scenario == 'trex_build' and trex_last_commit_hash and trex_repo:
        try:
            print 'Getting TRex commit with hash %s' % trex_last_commit_hash
            command = 'timeout 10 git --git-dir %s show %s --quiet' % (trex_repo, trex_last_commit_hash)
            print 'Executing: %s' % command
            proc = subprocess.Popen(shlex.split(command), stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            (trex_last_commit_info, stderr) = proc.communicate()
            print 'Stdout:\n\t' + trex_last_commit_info.replace('\n', '\n\t')
            print 'Stderr:', stderr
            print 'Return code:', proc.returncode
            trex_last_commit_info = trex_last_commit_info.replace('\n', '<br>')
        except Exception as e:
            print 'Error getting last commit: %s' % e

##### get xmls: report_<setup name>.xml

    err = []
    jobs_list = []
    jobs_file = '%s/jobs_list.info' % args.input_dir
    if os.path.exists(jobs_file):
        with open('%s/jobs_list.info' % args.input_dir) as f:
            for line in f.readlines():
                line = line.strip()
                if line:
                    jobs_list.append(line)
    else:
        message = '%s does not exist!' % jobs_file
        print message
        err.append(message)

##### aggregate results to 1 single tree
    aggregated_root = ET.Element('testsuite')
    setups = {}
    for job in jobs_list:
        xml_file = '%s/report_%s.xml' % (args.input_dir, job)
        if not os.path.exists(xml_file):
            message = '%s referenced in jobs_list.info does not exist!' % xml_file
            print message
            err.append(message)
            continue
        if os.path.basename(xml_file) == os.path.basename(args.output_xmlfile):
            continue
        setups[job] = []
        print('Processing setup: %s' % job)
        tree = ET.parse(xml_file)
        root = tree.getroot()
        for key, value in root.attrib.items():
            if key in aggregated_root.attrib and value.isdigit(): # sum total number of failed tests etc.
                aggregated_root.attrib[key] = str(int(value) + int(aggregated_root.attrib[key]))
            else:
                aggregated_root.attrib[key] = value
        tests = root.getchildren()
        if not len(tests): # there should be tests:
            message = 'No tests in xml %s' % xml_file
            print message
            err.append(message)
        for test in tests:
            setups[job].append(test)
            test.attrib['name'] = test.attrib['classname'] + '.' + test.attrib['name']
            test.attrib['classname'] = job
            aggregated_root.append(test)

##### save output xml

    print('Writing output file: %s' % args.output_xmlfile)
    ET.ElementTree(aggregated_root).write(args.output_xmlfile)


##### build output html
    error_tests = []
    functional_tests = OrderedDict()
    # categorize and get output of each test
    for test in aggregated_root.getchildren(): # each test in xml
        if is_functional_test_name(test.attrib['name']):
            functional_tests[test.attrib['name']] = test
        result_tuple = None
        for child in test.getchildren():        # <system-out>, <system-err>  (<failure>, <error>, <skipped> other: passed)
#            if child.tag in ('failure', 'error'):
                #temp = copy.deepcopy(test)
                #print temp._children
                #print test._children
#                error_tests.append(test)
            if child.tag == 'failure':
                error_tests.append(test)
                result_tuple = ('failure', child.text)
            elif child.tag == 'error':
                error_tests.append(test)
                result_tuple = ('error', child.text)
            elif child.tag == 'skipped':
                result_tuple = ('skipped', child.text)
            elif child.tag == 'system-out':
                test.attrib['stdout'] = child.text
            elif child.tag == 'system-err':
                test.attrib['stderr'] = child.text
        if result_tuple:
            test.attrib['result'] = result_tuple

    html_output = '''\
<html>
<head>
<meta http-equiv="Content-Type" content="text/html; charset=UTF-8">
<style type="text/css">
'''
    with open('style.css') as f:
        style_css = f.read()
    html_output += style_css
    html_output +='''
</style>
</head>

<body>
<table class="reference">
'''
    html_output += add_th_td('Scenario:', scenario.capitalize())
    start_time_file = '%s/start_time.info' % args.input_dir
    if os.path.exists(start_time_file):
        with open(start_time_file) as f:
            start_time = int(f.read())
        total_time = int(time.time()) - start_time
        html_output += add_th_td('Started:', datetime.datetime.fromtimestamp(start_time).strftime('%d/%m/%Y %H:%M:%S'))
        html_output += add_th_td('Total duration:', datetime.timedelta(seconds = total_time))
    if trex_last_commit_info:
        html_output += add_th_td('Last commit:', trex_last_commit_info)
    html_output += '</table><br>\n'
    if err:
        html_output += '<font color=red>%s<font><br><br>\n' % '\n<br>'.join(err)

#<table style="width:100%;">
#    <tr>
#        <td>Summary:</td>\
#'''
    #passed_quantity = len(result_types['passed'])
    #failed_quantity = len(result_types['failed'])
    #error_quantity = len(result_types['error'])
    #skipped_quantity = len(result_types['skipped'])

    #html_output += '<td>Passed: %s</td>' % passed_quantity
    #html_output += '<td>Failed: %s</td>' % (pad_tag(failed_quantity, 'b') if failed_quantity else '0')
    #html_output += '<td>Error: %s</td>' % (pad_tag(error_quantity, 'b') if error_quantity else '0')
    #html_output += '<td>Skipped: %s</td>' % (pad_tag(skipped_quantity, 'b') if skipped_quantity else '0')
#    html_output += '''
#    </tr>
#</table>'''

    category_arr = [FUNCTIONAL_CATEGORY, ERROR_CATEGORY]

# Adding buttons
    # Error button
    if len(error_tests):
        html_output += '\n<button onclick=tgl_cat("cat_tglr_{error}")>{error}</button>'.format(error = ERROR_CATEGORY)
    # Setups buttons
    for category, tests in setups.items():
        category_arr.append(category)
        html_output += '\n<button onclick=tgl_cat("cat_tglr_%s")>%s</button>' % (category_arr[-1], category)
    # Functional buttons
    if len(functional_tests):
        html_output += '\n<button onclick=tgl_cat("cat_tglr_%s")>%s</button>' % (FUNCTIONAL_CATEGORY, FUNCTIONAL_CATEGORY)

# Adding tests
    # Error tests
    if len(error_tests):
        html_output += add_category_of_tests(ERROR_CATEGORY, error_tests, hidden=False)
    # Setups tests
    for category, tests in setups.items():
        html_output += add_category_of_tests(category, tests, hidden=True, category_info_dir=args.input_dir)
    # Functional tests
    if len(functional_tests):
        html_output += add_category_of_tests(FUNCTIONAL_CATEGORY, functional_tests.values(), hidden=True)

    html_output += '\n\n<script type="text/javascript">\n    var category_arr = %s\n' % ['cat_tglr_%s' % x for x in category_arr]
    html_output += '''
    function tgl_cat(id)
        {
        for(var i=0; i<category_arr.length; i++)
            {
            var e = document.getElementById(category_arr[i]);
            if (id == category_arr[i])
                {
                if(e.style.display == 'block')
                    e.style.display = 'none';
                else
                    e.style.display = 'block';
                }
            else
                {
                if (e) e.style.display = 'none';
                }
            }
        }
    function tgl_test(id)
        {
        var e = document.getElementById(id);
        if(e.style.display == 'table-row')
            e.style.display = 'none';
        else
            e.style.display = 'table-row';
        }
</script>
</body>
</html>\
'''

# mail report (only error tests, expanded)

    mail_output = '''\
<html>
<head>
<meta http-equiv="Content-Type" content="text/html; charset=UTF-8">
<style type="text/css">
'''
    mail_output += style_css
    mail_output +='''
</style>
</head>

<body>
<table class="reference">
'''
    mail_output += add_th_td('Scenario:', scenario.capitalize())
    if build_url:
        mail_output += add_th_td('Full HTML report:', '<a class="example" href="%s/HTML_Report">link</a>' % build_url)
    start_time_file = '%s/start_time.info' % args.input_dir
    if os.path.exists(start_time_file):
        with open(start_time_file) as f:
            start_time = int(f.read())
        total_time = int(time.time()) - start_time
        mail_output += add_th_td('Started:', datetime.datetime.fromtimestamp(start_time).strftime('%d/%m/%Y %H:%M:%S'))
        mail_output += add_th_td('Total duration:', datetime.timedelta(seconds = total_time))
    if trex_last_commit_info:
        mail_output += add_th_td('Last commit:', trex_last_commit_info)
    mail_output += '</table><br>\n<table width=100%><tr><td>\n'

    for category in setups.keys():
        failing_category = False
        for test in error_tests:
            if test.attrib['classname'] == category:
                failing_category = True
        if failing_category or not len(setups[category]):
            mail_output += '<table class="reference_fail" align=left style="Margin-bottom:10;Margin-right:10;">\n'
        else:
            mail_output += '<table class="reference" align=left style="Margin-bottom:10;Margin-right:10;">\n'
        mail_output += add_th_th('Setup:', pad_tag(category.replace('.', '/'), 'b'))
        category_info_file = '%s/report_%s.info' % (args.input_dir, category.replace('.', '_'))
        if os.path.exists(category_info_file):
            with open(category_info_file) as f:
                for info_line in f.readlines():
                    key_value = info_line.split(':', 1)
                    if key_value[0].startswith('User'): # always 'hhaim', no need to show
                        continue
                    mail_output += add_th_td('%s:' % key_value[0], key_value[1])
        else:
            mail_output += add_th_td('Info:', 'No info')
        mail_output += '</table>\n'
    mail_output += '</td></tr></table>\n'

    # Error tests
    if len(error_tests) or err:
        if err:
            mail_output += '<font color=red>%s<font>' % '\n<br>'.join(err)
        if len(error_tests) > 5:
            mail_output += '\n<br><font color=red>More than 5 failed tests, showing brief output.<font>\n<br>'
            # show only brief version (cut some info)
            mail_output += add_category_of_tests(ERROR_CATEGORY, error_tests, hidden=False, expanded=True, brief=True)
        else:
            mail_output += add_category_of_tests(ERROR_CATEGORY, error_tests, hidden=False, expanded=True)
    else:
        mail_output += '<table><tr style="font-size:120;color:green;font-family:arial"><td>☺</td><td style="font-size:20">All passed.</td></tr></table>\n'
    mail_output += '\n</body>\n</html>'

##### save outputs

# html
    with open(args.output_htmlfile, 'w') as f:
        print('Writing output file: %s' % args.output_htmlfile)
        f.write(html_output)

# mail content
    with open(args.output_mailfile, 'w') as f:
        print('Writing output file: %s' % args.output_mailfile)
        f.write(mail_output)

# build status
    category_dict_status = {}
    if os.path.exists(args.build_status_file):
        with open(args.build_status_file) as f:
            print('Reading: %s' % args.build_status_file)
            category_dict_status = pickle.load(f)
            if type(category_dict_status) is not dict:
                print '%s is corrupt, truncating' % args.build_status_file
                category_dict_status = {}

    last_status = category_dict_status.get(scenario, 'Successful') # assume last is passed if no history
    if err or len(error_tests): # has fails
        if is_good_status(last_status):
            current_status = 'Failure'
        else:
            current_status = 'Still Failing'
    else:
        if is_good_status(last_status):
            current_status = 'Successful'
        else:
            current_status = 'Fixed'
    category_dict_status[scenario] = current_status

    with open(args.build_status_file, 'w') as f:
        print('Writing output file: %s' % args.build_status_file)
        pickle.dump(category_dict_status, f)

# mail title
    mailtitle_output = scenario.capitalize()
    if build_id:
        mailtitle_output += ' - Build #%s' % build_id
    mailtitle_output += ' - %s!' % current_status
    
    with open(args.output_titlefile, 'w') as f:
        print('Writing output file: %s' % args.output_titlefile)
        f.write(mailtitle_output)
