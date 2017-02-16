# -*- coding: utf-8 -*-
import xml.etree.ElementTree as ET
import outer_packages
import argparse
import glob
from pprint import pprint
import sys, os
from collections import OrderedDict
import copy
import datetime, time
import traceback
import yaml
import subprocess, shlex
from ansi2html import Ansi2HTMLConverter

converter = Ansi2HTMLConverter(inline = True)
convert = converter.convert

def ansi2html(text):
    return convert(text, full = False)

FUNCTIONAL_CATEGORY = 'Functional' # how to display those categories
ERROR_CATEGORY = 'Error'


def try_write(file, text):
    try:
        file.write(text)
    except:
        try:
            file.write(text.encode('utf-8'))
        except:
            file.write(text.decode('utf-8'))

def pad_tag(text, tag):
    return '<%s>%s</%s>' % (tag, text, tag)

def mark_string(text, color, condition):
    if condition:
        return '<font color=%s><b>%s</b></font>' % (color, text)
    return text


def is_functional_test_name(testname):
    #if testname.startswith(('platform_', 'misc_methods_', 'vm_', 'payload_gen_', 'pkt_builder_')):
    #    return True
    #return False
    if testname.startswith('functional_tests.'):
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
# tests - list of tests, derived from aggregated xml report, changed a little to get easily stdout etc.
# tests_type - stateful or stateless
# category_info_dir - folder to search for category info file
# expanded - bool, false = outputs (stdout etc.) of tests are hidden by CSS
# brief - bool, true = cut some part of tests outputs (useful for errors section with expanded flag)
def add_category_of_tests(category, tests, tests_type = None, category_info_dir = None, expanded = False, brief = False):
    is_actual_category = category not in (FUNCTIONAL_CATEGORY, ERROR_CATEGORY)
    category_id = '_'.join([category, tests_type]) if tests_type else category
    category_name = ' '.join([category, tests_type.capitalize()]) if tests_type else category
    html_output = ''
    if is_actual_category:
        html_output += '<br><table class="reference">\n'
        
        if category_info_dir:
            category_info_file = '%s/report_%s.info' % (category_info_dir, category)
            if os.path.exists(category_info_file):
                with open(category_info_file) as f:
                    for info_line in f.readlines():
                        key_value = info_line.split(':', 1)
                        if key_value[0].strip() in list(trex_info_dict.keys()) + ['User']: # always 'hhaim', no need to show
                            continue
                        html_output += add_th_td('%s:' % key_value[0], key_value[1])
            else:
                html_output += add_th_td('Info:', 'No info')
                print('add_category_of_tests: no category info %s' % category_info_file)
        if tests_type:
            html_output += add_th_td('Tests type:', tests_type.capitalize())
        if len(tests):
            total_duration = 0.0
            for test in tests:
                total_duration += float(test.attrib['time'])
            html_output += add_th_td('Tests duration:', datetime.timedelta(seconds = int(total_duration)))
        html_output += '</table>\n'

    if not len(tests):
        return html_output + pad_tag('<br><font color=red>No tests!</font>', 'b')
    html_output += '<br>\n<table class="reference" width="100%">\n<tr><th align="left">'

    if category == ERROR_CATEGORY:
        html_output += 'Setup</th><th align="left">Failed tests:'
    else:
        html_output += '%s tests:' % category_name
    html_output += '</th><th align="center">Final Result</th>\n<th align="center">Time (s)</th>\n</tr>\n'
    for test in tests:
        functional_test = is_functional_test_name(test.attrib['name'])
        if functional_test and is_actual_category:
            continue
        if category == ERROR_CATEGORY:
            test_id = ('err_' + test.attrib['classname'] + test.attrib['name']).replace('.', '_')
        else:
            test_id = (category_id + test.attrib['name']).replace('.', '_')
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
        html_output += '<td align="center"> '+ test.attrib['time'] + '</td></tr>'

        result, result_text = test.attrib.get('result', ('', ''))
        if result_text:
            start_index_errors_stl = result_text.find('STLError: \n******')
            if start_index_errors_stl > 0:
                result_text = result_text[start_index_errors_stl:].strip() # cut traceback
            start_index_errors = result_text.find('Exception: The test is failed, reasons:')
            if start_index_errors > 0:
                result_text = result_text[start_index_errors + 10:].strip() # cut traceback
            result_text = ansi2html(result_text)
            result_text = '<b style="color:000080;">%s:</b><br>%s<br><br>' % (result.capitalize(), result_text.replace('\n', '<br>'))
        stderr = '' if brief and result_text else test.get('stderr', '')
        if stderr:
            stderr = ansi2html(stderr)
            stderr = '<b style="color:000080;"><text color=000080>Stderr</text>:</b><br>%s<br><br>\n' % stderr.replace('\n', '<br>')
        stdout = '' if brief and result_text else test.get('stdout', '')
        if stdout:
            stdout = ansi2html(stdout)
            if brief: # cut off server logs
                stdout = stdout.split('>>>>>>>>>>>>>>>', 1)[0]
            stdout = '<b style="color:000080;">Stdout:</b><br>%s<br><br>\n' % stdout.replace('\n', '<br>')

        html_output += '<tr style="%scolor:603000;" id="%s"><td colspan=%s>' % ('' if expanded else 'display:none;', test_id, 4 if category == ERROR_CATEGORY else 3)
        if result_text or stderr or stdout:
            html_output += '%s%s%s</td></tr>' % (result_text, stderr, stdout)
        else:
            html_output += '<b style="color:000080;">No output</b></td></tr>'

    html_output += '\n</table>'
    return html_output

style_css = """
html {overflow-y:scroll;}

body {
    font-size:12px;
    color:#000000;
    background-color:#ffffff;
    margin:0px;
    font-family:verdana,helvetica,arial,sans-serif;
}

div {width:100%;}

table,th,td,input,textarea {
    font-size:100%;
}

table.reference, table.reference_fail {
    background-color:#ffffff;
    border:1px solid #c3c3c3;
    border-collapse:collapse;
    vertical-align:middle;
}

table.reference th {
    background-color:#e5eecc;
    border:1px solid #c3c3c3;
    padding:3px;
}

table.reference_fail th {
    background-color:#ffcccc;
    border:1px solid #c3c3c3;
    padding:3px;
}


table.reference td, table.reference_fail td {
    border:1px solid #c3c3c3;
    padding:3px;
}

a.example {font-weight:bold}

#a:link,a:visited {color:#900B09; background-color:transparent}
#a:hover,a:active {color:#FF0000; background-color:transparent}

.linktr {
    cursor: pointer;
}

.linktext {
    color:#0000FF;
    text-decoration: underline;
}
"""


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
    argparser.add_argument('--last_passed_commit', default='./reports/last_passed_commit',
                   dest = 'last_passed_commit', help='Name of output file to save last passed commit (should not be wiped).')
    args = argparser.parse_args()


##### get input variables/TRex commit info

    scenario                = os.environ.get('SCENARIO')
    build_url               = os.environ.get('BUILD_URL')
    build_id                = os.environ.get('BUILD_ID')
    trex_repo               = os.environ.get('TREX_CORE_REPO')
    last_commit_info_file   = os.environ.get('LAST_COMMIT_INFO')
    last_commit_branch_file = os.environ.get('LAST_COMMIT_BRANCH')
    python_ver              = os.environ.get('PYTHON_VER')
    if not scenario:
        print('Warning: no environment variable SCENARIO, using default')
        scenario = 'TRex regression'
    if not build_url:
        print('Warning: no environment variable BUILD_URL')
    if not build_id:
        print('Warning: no environment variable BUILD_ID')
    if not python_ver:
        print('Warning: no environment variable PYTHON_VER')

    trex_info_dict = OrderedDict()
    for file in glob.glob('%s/report_*.info' % args.input_dir):
        with open(file) as f:
            file_lines = f.readlines()
            if not len(file_lines):
                continue # to next file
            for info_line in file_lines:
                key_value = info_line.split(':', 1)
                not_trex_keys = ['Server', 'Router', 'User']
                if key_value[0].strip() in not_trex_keys:
                    continue # to next parameters
                trex_info_dict[key_value[0].strip()] = key_value[1].strip()
            break

    branch_name = ''
    if last_commit_branch_file and os.path.exists(last_commit_branch_file):
        with open(last_commit_branch_file) as f:
            branch_name = f.read().strip()

    trex_last_commit_info = ''
    trex_last_commit_hash = trex_info_dict.get('Git SHA')
    if last_commit_info_file and os.path.exists(last_commit_info_file):
        with open(last_commit_info_file) as f:
            trex_last_commit_info = f.read().strip().replace('\n', '<br>\n')
    elif trex_last_commit_hash and trex_repo:
        try:
            command = 'git show %s -s' % trex_last_commit_hash
            print('Executing: %s' % command)
            proc = subprocess.Popen(shlex.split(command), stdout = subprocess.PIPE, stderr = subprocess.STDOUT, cwd = trex_repo)
            (stdout, stderr) = proc.communicate()
            stdout = stdout.decode('utf-8', errors = 'replace')
            print('Stdout:\n\t' + stdout.replace('\n', '\n\t'))
            if stderr or proc.returncode:
                print('Return code: %s' % proc.returncode)
            trex_last_commit_info = stdout.replace('\n', '<br>\n')
        except Exception as e:
            traceback.print_exc()
            print('Error getting last commit: %s' % e)
    else:
        print('Could not find info about commit!')

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
        print(message)
        err.append(message)

##### aggregate results to 1 single tree
    aggregated_root = ET.Element('testsuite')
    test_types = ('functional', 'stateful', 'stateless')
    setups = {}
    for job in jobs_list:
        setups[job] = {}
        for test_type in test_types:
            xml_file = '%s/report_%s_%s.xml' % (args.input_dir, job, test_type)
            if not os.path.exists(xml_file):
                continue
            if os.path.basename(xml_file) == os.path.basename(args.output_xmlfile):
                continue
            setups[job][test_type] = []
            print('Processing report: %s.%s' % (job, test_type))
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
                print(message)
                #err.append(message)
            for test in tests:
                setups[job][test_type].append(test)
                test.attrib['name'] = test.attrib['classname'] + '.' + test.attrib['name']
                test.attrib['classname'] = job
                aggregated_root.append(test)
        if not sum([len(x) for x in setups[job].values()]):
            message = 'No reports from setup %s!' % job
            print(message)
            err.append(message)
            continue

    total_tests_count   = int(aggregated_root.attrib.get('tests', 0))
    error_tests_count   = int(aggregated_root.attrib.get('errors', 0))
    failure_tests_count = int(aggregated_root.attrib.get('failures', 0))
    skipped_tests_count = int(aggregated_root.attrib.get('skip', 0))
    passed_tests_count  = total_tests_count - error_tests_count - failure_tests_count - skipped_tests_count

    tests_count_string = mark_string('Total: %s' % total_tests_count, 'red', total_tests_count == 0) + ', '
    tests_count_string += mark_string('Passed: %s' % passed_tests_count, 'red', error_tests_count + failure_tests_count > 0) + ', '
    tests_count_string += mark_string('Error: %s' % error_tests_count, 'red', error_tests_count > 0) + ', '
    tests_count_string += mark_string('Failure: %s' % failure_tests_count, 'red', failure_tests_count > 0) + ', '
    tests_count_string += 'Skipped: %s' % skipped_tests_count

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
    html_output += style_css
    html_output +='''
</style>
</head>

<body>
<table class="reference">
'''
    if scenario:
        html_output += add_th_td('Scenario:', scenario.capitalize())
    if python_ver:
        html_output += add_th_td('Python:', python_ver)
    start_time_file = '%s/start_time.info' % args.input_dir
    if os.path.exists(start_time_file):
        with open(start_time_file) as f:
            start_time = int(f.read())
        total_time = int(time.time()) - start_time
        html_output += add_th_td('Regression start:', datetime.datetime.fromtimestamp(start_time).strftime('%d/%m/%Y %H:%M'))
        html_output += add_th_td('Regression duration:', datetime.timedelta(seconds = total_time))
    html_output += add_th_td('Tests count:', tests_count_string)
    for key in trex_info_dict:
        if key == 'Git SHA':
            continue
        html_output += add_th_td(key, trex_info_dict[key])
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
    for category in sorted(setups.keys()):
        category_arr.append(category)
        html_output += '\n<button onclick=tgl_cat("cat_tglr_%s")>%s</button>' % (category_arr[-1], category)
    # Functional buttons
    if len(functional_tests):
        html_output += '\n<button onclick=tgl_cat("cat_tglr_%s")>%s</button>' % (FUNCTIONAL_CATEGORY, FUNCTIONAL_CATEGORY)

# Adding tests
    # Error tests
    if len(error_tests):
        html_output += '<div style="display:block;" id="cat_tglr_%s">' % ERROR_CATEGORY
        html_output += add_category_of_tests(ERROR_CATEGORY, error_tests)
        html_output += '</div>'
    # Setups tests
    for category, tests in setups.items():
        html_output += '<div style="display:none;" id="cat_tglr_%s">' % category
        if 'stateful' in tests:
            html_output += add_category_of_tests(category, tests['stateful'], 'stateful', category_info_dir=args.input_dir)
        if 'stateless' in tests:
            html_output += add_category_of_tests(category, tests['stateless'], 'stateless', category_info_dir=(None if 'stateful' in tests else args.input_dir))
        html_output += '</div>'
    # Functional tests
    if len(functional_tests):
        html_output += '<div style="display:none;" id="cat_tglr_%s">' % FUNCTIONAL_CATEGORY
        html_output += add_category_of_tests(FUNCTIONAL_CATEGORY, functional_tests.values())
        html_output += '</div>'

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

# save html
    with open(args.output_htmlfile, 'w') as f:
        print('Writing output file: %s' % args.output_htmlfile)
        try_write(f, html_output)
    html_output = None

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
    if scenario:
        mail_output += add_th_td('Scenario:', scenario.capitalize())
    if python_ver:
        mail_output += add_th_td('Python:', python_ver)
    if build_url:
        mail_output += add_th_td('Full HTML report:', '<a class="example" href="%s/HTML_Report">link</a>' % build_url)
    start_time_file = '%s/start_time.info' % args.input_dir
    if os.path.exists(start_time_file):
        with open(start_time_file) as f:
            start_time = int(f.read())
        total_time = int(time.time()) - start_time
        mail_output += add_th_td('Regression start:', datetime.datetime.fromtimestamp(start_time).strftime('%d/%m/%Y %H:%M'))
        mail_output += add_th_td('Regression duration:', datetime.timedelta(seconds = total_time))
    mail_output += add_th_td('Tests count:', tests_count_string)
    for key in trex_info_dict:
        if key == 'Git SHA':
            continue
        mail_output += add_th_td(key, trex_info_dict[key])

    if trex_last_commit_info:
        mail_output += add_th_td('Last commit:', trex_last_commit_info)
    mail_output += '</table><br>\n<table width=100%><tr><td>\n'

    for category in setups.keys():
        failing_category = False
        for test in error_tests:
            if test.attrib['classname'] == category:
                failing_category = True
        if failing_category or not len(setups[category]) or not sum([len(x) for x in setups[category]]):
            mail_output += '<table class="reference_fail" align=left style="Margin-bottom:10;Margin-right:10;">\n'
        else:
            mail_output += '<table class="reference" align=left style="Margin-bottom:10;Margin-right:10;">\n'
        mail_output += add_th_th('Setup:', pad_tag(category.replace('.', '/'), 'b'))
        category_info_file = '%s/report_%s.info' % (args.input_dir, category.replace('.', '_'))
        if os.path.exists(category_info_file):
            with open(category_info_file) as f:
                for info_line in f.readlines():
                    key_value = info_line.split(':', 1)
                    if key_value[0].strip() in list(trex_info_dict.keys()) + ['User']: # always 'hhaim', no need to show
                        continue
                    mail_output += add_th_td('%s:' % key_value[0].strip(), key_value[1].strip())
        else:
            mail_output += add_th_td('Info:', 'No info')
        mail_output += '</table>\n'
    mail_output += '</td></tr></table>\n'

    # Error tests
    if len(error_tests) or err:
        if err:
            mail_output += '<font color=red>%s<font>' % '\n<br>'.join(err)
        if len(error_tests) > 5:
            mail_output += '\n<font color=red>More than 5 failed tests, showing brief output.<font>\n<br>'
            # show only brief version (cut some info)
            mail_output += add_category_of_tests(ERROR_CATEGORY, error_tests, expanded=True, brief=True)
        else:
            mail_output += add_category_of_tests(ERROR_CATEGORY, error_tests, expanded=True)
    else:
        mail_output += u'<table><tr style="font-size:120;color:green;font-family:arial"><td>☺</td><td style="font-size:20">All passed.</td></tr></table>\n'
    mail_output += '\n</body>\n</html>'

##### save outputs


# mail content
    with open(args.output_mailfile, 'w') as f:
        print('Writing output file: %s' % args.output_mailfile)
        try_write(f, mail_output)

# build status
    category_dict_status = {}
    if os.path.exists(args.build_status_file):
        print('Reading: %s' % args.build_status_file)
        with open(args.build_status_file, 'r') as f:
            try:
                category_dict_status = yaml.safe_load(f.read())
            except Exception as e:
                print('Error during YAML load: %s' % e)
        if type(category_dict_status) is not dict:
            print('%s is corrupt, truncating' % args.build_status_file)
            category_dict_status = {}

    last_status = category_dict_status.get(scenario, 'Successful') # assume last is passed if no history
    if err or len(error_tests): # has fails
        exit_status = 1
        if is_good_status(last_status):
            current_status = 'Failure'
        else:
            current_status = 'Still Failing'
    else:
        exit_status = 0
        if is_good_status(last_status):
            current_status = 'Successful'
        else:
            current_status = 'Fixed'
    category_dict_status[scenario] = current_status

    with open(args.build_status_file, 'w') as f:
        print('Writing output file: %s' % args.build_status_file)
        yaml.dump(category_dict_status, f)

# last successful commit
    if (current_status in ('Successful', 'Fixed')) and trex_last_commit_hash and len(jobs_list) > 0 and scenario == 'nightly':
        with open(args.last_passed_commit, 'w') as f:
            print('Writing output file: %s' % args.last_passed_commit)
            try_write(f, trex_last_commit_hash)

# mail title
    mailtitle_output = scenario.capitalize()
    if branch_name:
        mailtitle_output += ' (%s)' % branch_name
    if build_id:
        mailtitle_output += ' - Build #%s' % build_id
    mailtitle_output += ' - %s!' % current_status

    with open(args.output_titlefile, 'w') as f:
        print('Writing output file: %s' % args.output_titlefile)
        try_write(f, mailtitle_output)

# exit
    print('Status: %s' % current_status)
    sys.exit(exit_status)
