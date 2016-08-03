#!/router/bin/python-2.7.4
import h_avc


import ConfigParser
import threading
import time,signal
import argparse
import sys
import os
sys.path.append(os.path.join('trex_control_plane', 'stf', 'trex_stf_lib'))
from trex_client import CTRexClient
import subprocess
from time import sleep
import signal
import textwrap
import getpass
import random
import datetime
from datetime import timedelta
import traceback
import math
import re
import termios
import errno
import smtplib
from email.MIMEMultipart import MIMEMultipart
from email.MIMEBase import MIMEBase
from email.MIMEText import MIMEText
from email.Utils import COMMASPACE, formatdate
from email import Encoders
from email.mime.image import MIMEImage

from distutils.version import StrictVersion

class TrexRunException(Exception):
    def __init__ (self, reason, cmd = None, std_log = None, err_log = None):
        self.reason = reason
        self.std_log = std_log
        self.err_log = err_log
        # generate the error message
        self.message = "\nSummary of error:\n\n %s\n" % (reason)

        if std_log:
            self.message += "\nConsole Log:\n\n %s\n" % (self.std_log)

        if err_log:
            self.message += "\nStd Error Log:\n\n %s\n" % (self.err_log)

    def __str__(self):
        return self.message


############################# utility functions start #################################

def verify_glibc_version ():
    x = subprocess.check_output("/usr/bin/ldd --version", shell=True)
    m = re.match("^ldd \([^\)]+\) (.*)", x)
    if not m:
        raise Exception("Cannot determine LDD version")
    current_version = m.group(1)

    if StrictVersion(current_version) < StrictVersion("2.5"):
        raise Exception("GNU ldd version required for graph plotting is at least 2.5, system is %s - please run simple 'find'" % current_version)

def get_median(numericValues):
    theValues = sorted(numericValues)
    if len(theValues) % 2 == 1:
        return theValues[(len(theValues)+1)/2-1]
    else:
        lower = theValues[len(theValues)/2-1]
        upper = theValues[len(theValues)/2]
    return (float(lower + upper)) / 2  

def list_to_clusters(l, n):
    for i in xrange(0, len(l), n):
        yield l[i:i+n]

def cpu_histo_to_str (cpu_histo):
    s = "\nCPU Samplings:\n\n"
    period = 0

    clusters = list(list_to_clusters(cpu_histo, 10))

    for cluster in clusters:
        period += 10
        line = "%3s Seconds: [" % period

        cluster += (10 - len(cluster)) * [None]

        for x in cluster:
            if (x != None):
                line += "%5.1f%%, " % x
            else:
                line += "        "

        line = line[:-2] # trim the comma and space
        line += " "      # return the space 

        line += "]\n"

        s += line

    return s

# Terminal Manager Class
class TermMng:
    def __enter__(self):
        self.fd = sys.stdin.fileno()
        self.old = termios.tcgetattr(self.fd)

        # copy new and remove echo
        new = self.old[:]
        new[3] &= ~termios.ECHO

        self.tcsetattr_flags = termios.TCSAFLUSH
        if hasattr(termios, 'TCSASOFT'):
            self.tcsetattr_flags |= termios.TCSASOFT

        termios.tcsetattr(self.fd, self.tcsetattr_flags, new)

    def __exit__ (self ,type, value, traceback):
        termios.tcsetattr(self.fd, self.tcsetattr_flags, self.old)

############################# utility functions stop #################################

def send_mail(send_from, send_to, subject, html_text, txt_attachments=[], images=[], server="localhost"):
    assert isinstance(send_to, list)
    assert isinstance(txt_attachments, list)
    assert isinstance(images, list)

    # create a multi part message
    msg = MIMEMultipart()
    msg['From'] = send_from
    msg['To'] = COMMASPACE.join(send_to)
    msg['Date'] = formatdate(localtime=True)
    msg['Subject'] = subject
    msg['Cc'] = "imarom@cisco.com"

    # add all images to the text as embbeded images
    for image in images:
        html_text += '<br><img src="cid:{0}"><br>'.format(image)
        fp = open(image, 'rb')
        image_object = MIMEImage(fp.read())
        fp.close()
        image_object.add_header('Content-ID', image)
        msg.attach(image_object)

    # attach the main report as embedded HTML
    msg.attach( MIMEText(html_text, 'html') )

    # attach regualr txt files
    for f in txt_attachments:
        part = MIMEBase('application', "octet-stream")
        part.set_payload( open(f,"rb").read() )
        Encoders.encode_base64(part)
        part.add_header('Content-Disposition', 'attachment; filename="%s"' % os.path.basename(f))
        msg.attach(part)

    smtp = smtplib.SMTP(server)
    smtp.sendmail(send_from, send_to, msg.as_string())
    smtp.close()

# convert HTML to image - returning a image file as a string
def html2image (html_filename, image_filename):
    cmd = "./phantom/phantomjs ./phantom/rasterize.js {0} {1}".format(html_filename, image_filename)
    subprocess.call(cmd, shell=True)

    assert os.path.exists(image_filename)

    return (image_filename)

# convert results of run to a string
def run_results_to_str (results, cond_type):
    output = ""

    output +=  "M:                  {0:<12.6f}\n".format(results['m'])
    output +=  "BW:                 {0:<12,.2f} [Mbps]\n".format(results['tx'])
    output +=  "PPS:                {0:<12,} [pkts]\n".format(int(results['total-pps']))
    output +=  "CPU:                {0:.4f} %\n".format(results['cpu_util'])
    output +=  "Maximum Latency:    {0:<12,} [usec]\n".format(int(results['maximum-latency']))
    output +=  "Average Latency:    {0:<12,} [usec]\n".format(int(results['average-latency']))
    output +=  "Pkt Drop:           {0:<12,} [pkts]\n".format(int(results['total-pkt-drop']))
    output +=  "Condition:          {0:<12} ({1})\n".format("Passed" if check_condition(cond_type, results) else "Failed", cond_type_to_str(cond_type))

    return (output)

############################# classes #################################
class ErrorHandler(object):
    def __init__ (self, exception, traceback):

        if isinstance(exception, TrexRunException):
            logger.log("\n*** Script Terminated Due To Trex Failure")
            logger.log("\n********************** TRex Error - Report **************************\n")
            logger.log(str(exception))
            logger.flush()

        elif isinstance(exception, IOError):
            logger.log("\n*** Script Terminated Due To IO Error")
            logger.log("\nEither Router address or the Trex config is bad or some file is missing - check traceback below")
            logger.log("\n********************** IO Error - Report **************************\n")
            logger.log(str(exception))
            logger.log(str(traceback))
            logger.flush()


        else:
            logger.log("\n*** Script Terminated Due To Fatal Error")
            logger.log("\n********************** Internal Error - Report **************************\n")
            logger.log(str(exception) + "\n")
            logger.log(str(traceback))
            logger.flush()


        # call the handler
        g_kill_cause = "error"
        os.kill(os.getpid(), signal.SIGUSR1)


# simple HTML table
class HTMLTable:
    def __init__ (self):
        self.table_rows = []

    def add_row (self, param, value):
        self.table_rows.append([param, value])

    def generate_table(self):
        txt = '<table class="myWideTable" style="width:50%">'
        txt += "<tr><th>Parameter</th><th>Results</th></tr>"

        for row in self.table_rows:
            txt += "<tr><td>{0}</td><td>{1}</td></tr>".format(row[0], row[1])

        txt += "</table>"

        return txt

# process results and dispatch it
class JobReporter:
    def __init__ (self, job_summary):
        self.job_summary = job_summary
        pass

    def __plot_results_to_str (self, plot_results):
        output = "\nPlotted Points: \n\n"
        for p in plot_results:
            output += "BW  : {0:8.2f},  ".format(p['tx'])
            output += "PPS : {0:8,}  ".format(int(p['total-pps']))
            output += "CPU : {0:8.2f} %,  ".format(p['cpu_util'])
            output += "Max Latency : {0:10,},  ".format(int(p['maximum-latency']))
            output += "Avg Latency : {0:10,},  ".format(int(p['average-latency']))
            output += "Pkt Drop    : {0:12,},  \n".format(int(p['total-pkt-drop']))

        return (output + "\n")

    def __summary_to_string (self):
        output = ""

        output += "\n-== Job Completed Successfully ==-\n\n"
        output += "Job Report:\n\n"
        output +=  "Job Name:          {0}\n".format(self.job_summary['job_name'])
        output +=  "YAML file:         {0}\n".format(self.job_summary['yaml'])
        output +=  "Job Type:          {0}\n".format(self.job_summary['job_type_str'])
        output +=  "Condition:         {0}\n".format(self.job_summary['cond_name'])
        output +=  "Job Dir:           {0}\n".format(self.job_summary['job_dir'])
        output +=  "Job Log:           {0}\n".format(self.job_summary['log_filename'])
        output +=  "Email Report:      {0}\n".format(self.job_summary['email'])
        output +=  "Job Total Time:    {0}\n\n".format(self.job_summary['total_run_time'])

        if (self.job_summary.get('find_results') != None):
            find_results = self.job_summary['find_results']
            output += ("Maximum BW Point Details:\n\n")
            output += run_results_to_str(find_results, self.job_summary['cond_type'])
            
        if (self.job_summary.get('plot_results') != None):
            plot_results = self.job_summary['plot_results']
            output += self.__plot_results_to_str(plot_results)

        return output


    # simple print to screen of the job summary
    def print_summary (self):
        summary = self.__summary_to_string()
        logger.log(summary)

    def __generate_graph_report (self, plot_results):
        graph_data = str( [ [x['tx'], x['cpu_util']/100, x['maximum-latency'], x['average-latency']] for x in plot_results ] )
        table_data = str( [ [x['tx'], x['total-pps'], x['cpu_util']/100, x['norm_cpu'], x['maximum-latency'], x['average-latency'], x['total-pkt-drop']] for x in plot_results ] )

        with open ("graph_template.html", "r") as myfile:
            data = myfile.read()
            data = data.replace("!@#$template_fill_head!@#$", self.job_summary['yaml'])
            data = data.replace("!@#$template_fill_graph!@#$", graph_data[1:(len(graph_data) - 1)])
            data = data.replace("!@#$template_fill_table!@#$", table_data[1:(len(table_data) - 1)])

        # generate HTML report
        graph_filename = self.job_summary['graph_filename']
        text_file = open(graph_filename, "w")
        text_file.write(str(data))
        text_file.close()

        return graph_filename

    def __generate_body_report (self):
        job_setup_table = HTMLTable()

        job_setup_table.add_row("User Name", self.job_summary['user'])
        job_setup_table.add_row("Job Name", self.job_summary['job_name'])
        job_setup_table.add_row("Job Type", self.job_summary['job_type_str'])
        job_setup_table.add_row("Test Condition", self.job_summary['cond_name'])
        job_setup_table.add_row("YAML File", self.job_summary['yaml'])
        job_setup_table.add_row("Job Total Time", "{0}".format(self.job_summary['total_run_time']))

        job_summary_table = HTMLTable()

        find_results = self.job_summary['find_results']

        if find_results != None:
            job_summary_table.add_row("Maximum Bandwidth", "{0:,.2f} [Mbps]".format(find_results['tx']))
            job_summary_table.add_row("Maximum PPS", "{0:,} [pkts]".format(int(find_results['total-pps'])))
            job_summary_table.add_row("CPU Util.", "{0:.2f}%".format(find_results['cpu_util']))
            job_summary_table.add_row("Maximum Latency", "{0:,} [usec]".format(int(find_results['maximum-latency'])))
            job_summary_table.add_row("Average Latency", "{0:,} [usec]".format(int(find_results['average-latency'])))
            job_summary_table.add_row("Total Pkt Drop", "{0:,}  [pkts]".format(int(find_results['total-pkt-drop'])))

        with open ("report_template.html", "r") as myfile:
            data = myfile.read()
            data = data.replace("!@#$template_fill_job_setup_table!@#$", job_setup_table.generate_table())
            data = data.replace("!@#$template_fill_job_summary_table!@#$", job_summary_table.generate_table())

        return data

    # create an email report and send to the user
    def send_email_report (self):
        images = []

        logger.log("\nCreating E-Mail Report...\n")
        
        # generate main report
        report_str = self.__generate_body_report()

        # generate graph report (if exists)
        plot_results = self.job_summary['plot_results']
        if plot_results:
            logger.log("Generating Plot Results HTML ...\n")
            graph_filename  = self.__generate_graph_report(plot_results)
            logger.log("Converting HTML to image ...\n")
            images.append(html2image(graph_filename, graph_filename + ".png"))
            
        else:
            graph_filename = None

        # create email
        from_addr = 'TrexReporter@cisco.com'
        to_addr = []
        to_addr.append(self.job_summary['email'])
        to_addr.append('imarom@cisco.com')

        attachments = []
        attachments.append(self.job_summary['log_filename'])
        logger.log("Attaching log {0}...".format(self.job_summary['log_filename']))

        if graph_filename:
            attachments.append(graph_filename)
            logger.log("Attaching plotting report {0}...".format(graph_filename))

        logger.flush()

        send_mail(from_addr, to_addr, "TRex Performance Report", report_str, attachments, images)
        logger.log("\nE-mail sent successfully to: " + self.job_summary['email'])

# dummy logger in case logger creation failed
class DummyLogger(object):
    def __init__(self):
        pass

    def log(self, text, force = False, newline = True):
        text_out = (text + "\n") if newline else text
        sys.stdout.write(text_out)

    def console(self, text, force = False, newline = True):
        self.log(text, force, newline)

    def flush (self):
        pass

# logger object
class MyLogger(object):
  
    def __init__(self, log_filename):
        # Store the original stdout and stderr
        sys.stdout.flush()
        sys.stderr.flush()

        self.stdout_fd = os.dup(sys.stdout.fileno())
        self.devnull = os.open('/dev/null', os.O_WRONLY)
        self.log_file = open(log_filename, 'w')
        self.silenced = False
        self.pending_log_file_prints = 0
        self.active = True

    def shutdown (self):
        self.active = False

    def reactive (self):
        self.active = True

    # silence all prints from stdout
    def silence(self):
        os.dup2(self.devnull, sys.stdout.fileno())
        self.silenced = True

    # restore stdout status
    def restore(self):
        sys.stdout.flush()
        sys.stderr.flush()
        # Restore normal stdout
        os.dup2(self.stdout_fd, sys.stdout.fileno())
        self.silenced = False

    #print a message to the log (both stdout / log file)
    def log(self, text, force = False, newline = True):
        if not self.active:
            return

        self.log_file.write((text + "\n") if newline else text)
        self.pending_log_file_prints += 1

        if (self.pending_log_file_prints >= 10):
             self.log_file.flush()
             self.pending_log_file_prints = 0

        self.console(text, force, newline)

    # print a message to the console alone
    def console(self, text, force = False, newline = True):
        if not self.active:
            return

        _text = (text + "\n") if newline else text

        # if we are silenced and not forced - go home
        if self.silenced and not force:
            return

        if self.silenced:
            os.write(self.stdout_fd, _text)
        else:
            sys.stdout.write(_text)

        sys.stdout.flush()

    # flush
    def flush(self):
        sys.stdout.flush()
        self.log_file.flush()

    def __del__(self):
        os.close(self.devnull)
        if self.log_file:
            self.log_file.flush()
            self.log_file.close()
        

# simple progress bar
class ProgressBar(threading.Thread):
    def __init__(self, time_sec, router):
        super(ProgressBar, self).__init__()
        self.active = True
        self.time_sec = time_sec + 15
        self.router = router

    def run (self):
        global g_stop

        col = 40
        delta_for_sec = float(col) / self.time_sec

        accu = 0.0

        for i in range(self.time_sec):
            if (self.active == False):
                # print 100% - done
                bar = "\r[" + ('#' * col) + "] {0:.2f} %".format(100)
                logger.console(bar, force = True, newline = False)
                break

            if (g_stop == True):
                break

            sleep(1)
            accu += delta_for_sec
            bar = "\r[" + ('#' * int(accu)) + (' ' * (col - int(accu))) + "] {0:.2f} %".format( (accu/col) * 100 )
            bar += " / Router CPU: {0:.2f} %".format(self.router.get_last_cpu_util())
            logger.console(bar, force = True, newline = False)

        logger.console("\r\n", force = True, newline = False)
        logger.flush()

    def stop (self):
        self.active = False
        self.join()

# global vars

g_stop = False
logger = DummyLogger()

# cleanup list is a list of callables to be run when cntrl+c is caught
cleanup_list = []

################ threads ########################

# sampler
class Sample_Thread (threading.Thread):
    def __init__(self, threadID, router):

        threading.Thread.__init__(self)
        self.threadID = threadID
        self.router = router
        self.stop = False

    def run(self):
        self.router.clear_sampling_stats()

        try:
            while (self.stop==False) and (g_stop==False):
                self.router.sample_stats()
                time.sleep(1);
        except Exception as e:
            ErrorHandler(e, traceback.format_exc())

    def do_stop(self):
        self.stop = True


def general_cleanup_on_error ():
    global g_stop
    global cleanup_list

    # mark all the threads to finish
    g_stop = True;

    # shutdown and flush the logger
    logger.shutdown()
    if logger:
        logger.flush()

    # execute the registered callables
    for c in cleanup_list:
        c()
      
    # dummy wait for threads to finish (TODO: make this more smart)
    time.sleep(2)
    exit(-1)

# just a dummy for preventing chain calls
def signal_handler_dummy (sig_id, frame):
    pass

def error_signal_handler (sig_id, frame):
    # make sure no chain of calls
    signal.signal(signal.SIGUSR1, signal_handler_dummy)
    signal.signal(signal.SIGINT, signal_handler_dummy)

    general_cleanup_on_error()

def int_signal_handler(sig_id, frame):
    # make sure no chain of calls
    signal.signal(signal.SIGINT, signal_handler_dummy)
    signal.signal(signal.SIGUSR1, signal_handler_dummy)

    logger.log("\n\nCaught Cntrl+C... Cleaning up!\n\n")

    general_cleanup_on_error()


# Trex with sampling
class CTRexWithRouter:
    def __init__(self, trex, trex_params):
        self.trex = trex;
        self.trex_params = trex_params

        if self.trex_params['router_type'] == "ASR":
            self.router = h_avc.ASR1k(self.trex_params['router_interface'], self.trex_params['router_port'], self.trex_params['router_password'])
        elif self.trex_params['router_type'] == "ISR":
            self.router = h_avc.ISR(self.trex_params['router_interface'], self.trex_params['router_port'], self.trex_params['router_password'])
        else:
            raise Exception("unknown router type in config file")

        self.router.connect()

    def get_router (self):
        return self.router

    def run(self, m, duration):

        self.sample_thread = Sample_Thread(1, self.router)
        self.sample_thread.start();

        # launch trex
        try:
#           trex_res = self.trex.run(m, duration);
            self.trex.start_trex(c = self.trex_params['trex_cores'], 
                m = m,
                d = duration,
                f = self.trex_params['trex_yaml_file'],
                nc = True,
                l = self.trex_params['trex_latency'],
                limit_ports = self.trex_params['trex_limit_ports'])
            self.trex.sample_to_run_finish(20)      # collect trex-sample every 20 seconds. 
        except Exception:
            self.sample_thread.do_stop()  # signal to stop
            self.sample_thread.join()     # wait for it to realy stop 
            raise
            
        self.sample_thread.do_stop()  # signal to stop
        self.sample_thread.join()     # wait for it to realy stop 

        self.res = self.trex.get_result_obj()

        results = {}
        results['status'] = True
        results['trex_results'] = self.res
        results['avc_results']  = self.router.get_stats()

        return (results)
        #return(trex_res.get_status() == STATUS_OK);

# sanity checks to see run really went well
def sanity_test_run (trex_r, avc_r):
    pass
    #if (sum(avc_r['cpu_histo']) == 0):
        #raise h_trex.TrexRunException("CPU utilization from router is zero, check connectivity")

def _trex_run (job_summary, m, duration):
    
    trex_thread = job_summary['trex_thread']

    p = ProgressBar(duration, trex_thread.get_router())
    p.start()

    try:
        results = trex_thread.run(m, duration)
    except Exception as e:
        p.stop()
        raise

    p.stop()

    if (results == None):
        raise Exception("Failed to run Trex")

    # fetch values
    trex_r = results['trex_results']
    avc_r  = results['avc_results']

    sanity_test_run(trex_r, avc_r)

    res_dict = {}

    res_dict['m']  = m
    total_tx_bps = trex_r.get_last_value("trex-global.data.m_tx_bps")
    res_dict['tx'] = total_tx_bps / (1000 * 1000)  # EVENTUALLY CONTAINS IN MBPS (EXTRACTED IN BPS)

    res_dict['cpu_util'] = avc_r['cpu_util']

    if int(res_dict['cpu_util']) == 0:
        res_dict['norm_cpu']=1;
    else:
        res_dict['norm_cpu'] = (res_dict['tx'] / res_dict['cpu_util']) * 100

    res_dict['maximum-latency']  = max ( trex_r.get_max_latency().values() ) #trex_r.res['maximum-latency']
    res_dict['average-latency']  = trex_r.get_avg_latency()['all'] #trex_r.res['average-latency']
    
    logger.log(cpu_histo_to_str(avc_r['cpu_histo']))

    res_dict['total-pkt-drop']  = trex_r.get_total_drops()
    res_dict['expected-bps']    = trex_r.get_expected_tx_rate()['m_tx_expected_bps']
    res_dict['total-pps']       = get_median( trex_r.get_value_list("trex-global.data.m_tx_pps") )#trex_r.res['total-pps']
    res_dict['m_total_pkt']     = trex_r.get_last_value("trex-global.data.m_total_tx_pkts")

    res_dict['latency_condition'] = job_summary['trex_params']['trex_latency_condition']

    return res_dict

def trex_run (job_summary, m, duration):
    res = _trex_run (job_summary, m, duration)
    return res


def m_to_mbps (job_summary, m):
    return (m * job_summary['base_m_unit'])

# find the correct range of M
def find_m_range (job_summary):

    trex = job_summary['trex']
    trex_config = job_summary['trex_params']

    # if not provided - guess the correct range of bandwidth
    if not job_summary['m_range']:
        m_range = [0.0, 0.0]
        # 1 Mbps -> 1 Gbps
        LOW_TX = 1.0 * 1000 * 1000
        MAX_TX = 1.0 * 1000 * 1000 * 1000

        # for 10g go to 10g
        if trex_config['trex_machine_type'] == "10G":
            MAX_TX *= 10
   
        # dual injection can potentially reach X2 speed
        if trex_config['trex_is_dual'] == True:
            MAX_TX *= 2

    else:
        m_range = job_summary['m_range']
        LOW_TX = m_range[0] * 1000 * 1000
        MAX_TX = m_range[1] * 1000 * 1000
   
   
    logger.log("\nSystem Settings - Min: {0:,} Mbps / Max: {1:,} Mbps".format(LOW_TX / (1000 * 1000), MAX_TX / (1000 * 1000)))
    logger.log("\nTrying to get system minimum M and maximum M...")
   
    res_dict = trex_run(job_summary, 1, 30)
   
    # figure out low / high M
    m_range[0] = (LOW_TX / res_dict['expected-bps']) * 1
    m_range[1] = (MAX_TX / res_dict['expected-bps']) * 1


    # return both the m_range and the base m unit for future calculation
    results = {}
    results['m_range'] = m_range
    results['base_m_unit'] = res_dict['expected-bps'] /(1000 * 1000)

    return (results)

# calculate points between m_range[0] and m_range[1]
def calculate_plot_points (job_summary, m_range, plot_count):
    
    cond_type = job_summary['cond_type']
    delta_m = (m_range[1] - m_range[0]) / plot_count
    
    m_current = m_range[0]
    m_end = m_range[1]

    logger.log("\nStarting Plot Graph Task ...\n")
    logger.log("Plotting Range Is From: {0:.2f} [Mbps] To: {1:.2f} [Mbps] Over {2} Points".format(m_to_mbps(job_summary, m_range[0]), 
                                                                                                  m_to_mbps(job_summary, m_range[1]), 
                                                                                                  plot_count))
    logger.log("Delta Between Points is {0:.2f} [Mbps]".format(m_to_mbps(job_summary, delta_m)))
    plot_points = []

    duration = 180
    
    iter = 1

    trex = job_summary['trex']
    while (iter <= plot_count):
        logger.log("\nPlotting Point [{0}/{1}]:\n".format(iter, plot_count))
        logger.log("Estimated BW   ~= {0:,.2f} [Mbps]\n".format(m_to_mbps(job_summary, m_current)))
        logger.log("M               = {0:.6f}".format(m_current))
        logger.log("Duration        = {0} seconds\n".format(duration))

        res_dict = trex_run(job_summary, m_current, duration)
        print_trex_results(res_dict, cond_type)

        plot_points.append(dict(res_dict))

        m_current += delta_m
        iter = iter + 1

        # last point - make sure its the maximum point
        if (iter == plot_count):
            m_current = m_range[1]

        #print "waiting for system to stabilize ..."
        #time.sleep(30);
        
    return plot_points


def cond_type_to_str (cond_type):
    return "Max Latency" if cond_type=='latency' else "Pkt Drop"

# success condition (latency or drop)
def check_condition (cond_type, res_dict):
    if cond_type == 'latency':
        if res_dict['maximum-latency'] < res_dict['latency_condition']:
            return True
        else:
            return False

    # drop condition is a bit more complex - it should create high latency in addition to 0.2% drop
    elif cond_type == 'drop':
        if (res_dict['maximum-latency'] > (res_dict['latency_condition']+2000) ) and (res_dict['total-pkt-drop'] > (0.002 * res_dict['m_total_pkt'])):
            return False
        else:
            return True

    assert(0)

def print_trex_results (res_dict, cond_type):
    logger.log("\nRun Results:\n")
    output = run_results_to_str(res_dict, cond_type)
    logger.log(output)


######################## describe a find job ########################
class FindJob:
    # init a job object with min / max
    def __init__ (self, min, max, job_summary):
        self.min = float(min)
        self.max = float(max)
        self.job_summary = job_summary
        self.cond_type = job_summary['cond_type']
        self.success_points = []
        self.iter_num = 1
        self.found = False
        self.iter_duration = 300

    def _distance (self):
        return ( (self.max - self.min) / min(self.max, self.min) )

    def time_to_end (self):
        time_in_sec = (self.iters_to_end() * self.iter_duration)
        return timedelta(seconds = time_in_sec)

    def iters_to_end (self):
        # find 2% point
        ma = self.max
        mi = self.min
        iter = 0

        while True:
            dist = (ma - mi) / min(ma , mi)
            if dist < 0.02:
                break
            if random.choice(["up", "down"]) == "down":
                ma = (ma + mi) / 2
            else:
                mi = (ma + mi) / 2

            iter += 1

        return (iter)

    def _cur (self):
        return ( (self.min + self.max) / 2 )

    def _add_success_point (self, res_dict):
        self.success_points.append(res_dict.copy())

    def _is_found (self):
        return (self.found)

    def _next_iter_duration (self):
        return (self.iter_duration)

    # execute iteration
    def _execute (self):
        # reset the found var before running
        self.found = False

        # run and print results
        res_dict = trex_run(self.job_summary, self._cur(), self.iter_duration)

        self.iter_num += 1
        cur = self._cur()

        if (self._distance() < 0.02):
            if (check_condition(self.cond_type, res_dict)):
                # distance < 2% and success - we are done
                self.found = True
            else:
                # lower to 90% of current and retry
                self.min = cur * 0.9
                self.max = cur
        else:
            # success
            if (check_condition(self.cond_type, res_dict)):
                self.min = cur
            else:
                self.max = cur

        if (check_condition(self.cond_type, res_dict)):
            self._add_success_point(res_dict)

        return res_dict

    # find the max M before 
    def find_max_m (self):

        res_dict = {}
        while not self._is_found():

            logger.log("\n-> Starting Find Iteration #{0}\n".format(self.iter_num))
            logger.log("Estimated BW         ~=  {0:,.2f} [Mbps]".format(m_to_mbps(self.job_summary, self._cur())))
            logger.log("M                     =  {0:.6f}".format(self._cur()))
            logger.log("Duration              =  {0} seconds".format(self._next_iter_duration()))
            logger.log("Current BW Range      =  {0:,.2f} [Mbps] / {1:,.2f} [Mbps]".format(m_to_mbps(self.job_summary, self.min), m_to_mbps(self.job_summary, self.max)))
            logger.log("Est. Iterations Left  =  {0} Iterations".format(self.iters_to_end()))
            logger.log("Est. Time Left        =  {0}\n".format(self.time_to_end()))

            res_dict = self._execute()

            print_trex_results(res_dict, self.cond_type)

        find_results = res_dict.copy()
        find_results['max_m'] = self._cur()
        return (find_results)

######################## describe a plot job ########################
class PlotJob:
    def __init__(self, findjob):
        self.job_summary = findjob.job_summary

        self.plot_points = list(findjob.success_points)
        self.plot_points.sort(key = lambda item:item['tx'])

    def plot (self, duration = 300):
        return self.plot_points

        # add points if needed
        #iter = 0
        #for point in self.success_points:
            #iter += 1
            #logger.log("\nPlotting Point [{0}/{1}]:\n".format(iter, self.plot_count))
            #logger.log("Estimated BW   ~= {0:,.2f} [Mbps]\n".format(m_to_mbps(self.job_summary, point['m'])))
            #logger.log("M               = {0:.6f}".format(point['m']))
            #logger.log("Duration        = {0} seconds\n".format(duration))

            #res_dict = trex_run(self.job_summary, point['m'], duration)
            #print_trex_results(res_dict, self.job_summary['cond_type'])

            #self.plot_points.append(dict(res_dict))

        #self.plot_points = list(self.success_points)

        #print self.plot_points
        #self.plot_points.sort(key = lambda item:item['m'])
        #print self.plot_points

        #return self.plot_points


def generate_job_id ():
    return (str(int(random.getrandbits(32))))

def print_header ():
    logger.log("--== TRex Performance Tool v1.0 (2014) ==--")

# print startup summary
def log_startup_summary (job_summary):

    trex = job_summary['trex']
    trex_config = job_summary['trex_params']

    logger.log("\nWork Request Details:\n")
    logger.log("Setup Details:\n")
    logger.log("TRex Config File:   {0}".format(job_summary['config_file']))
    logger.log("Machine Name:        {0}".format(trex_config['trex_name']))
    logger.log("TRex Type:          {0}".format(trex_config['trex_machine_type']))
    logger.log("TRex Dual Int. Tx:  {0}".format(trex_config['trex_is_dual']))
    logger.log("Router Interface:    {0}".format(trex_config['router_interface']))

    logger.log("\nJob Details:\n")
    logger.log("Job Name:            {0}".format(job_summary['job_name']))
    logger.log("YAML file:           {0}".format(job_summary['yaml']))
    logger.log("Job Type:            {0}".format(job_summary['job_type_str']))
    logger.log("Condition Type:      {0}".format(job_summary['cond_name']))
    logger.log("Job Log:             {0}".format(job_summary['log_filename']))
    logger.log("Email Report:        {0}".format(job_summary['email']))

#   logger.log("\nTrex Command Used:\n{0}".format(trex.build_cmd(1, 10)))

def load_trex_config_params (filename, yaml_file):
    config = {}

    parser = ConfigParser.ConfigParser()

    try:
        parser.read(filename)

        config['trex_name'] = parser.get("trex", "machine_name")
        config['trex_port'] = parser.get("trex", "machine_port")
        config['trex_hisory_size'] = parser.getint("trex", "history_size")

        config['trex_latency_condition'] = parser.getint("trex", "latency_condition")
        config['trex_yaml_file'] = yaml_file

        # support legacy data
        config['trex_latency'] = parser.getint("trex", "latency")
        config['trex_limit_ports'] = parser.getint("trex", "limit_ports")
        config['trex_cores'] = parser.getint("trex", "cores")
        config['trex_machine_type'] = parser.get("trex", "machine_type")
        config['trex_is_dual'] = parser.getboolean("trex", "is_dual")

        # optional Trex parameters
        if parser.has_option("trex", "config_file"):
            config['trex_config_file'] = parser.get("trex", "config_file")
        else:
            config['trex_config_file'] = None

        if parser.has_option("trex", "misc_params"):
            config['trex_misc_params'] = parser.get("trex", "misc_params")
        else:
            config['trex_misc_params'] = None

        # router section
        
        if parser.has_option("router", "port"):
            config['router_port'] = parser.get("router", "port")
        else:
            # simple telnet port
            config['router_port'] = 23

        config['router_interface'] = parser.get("router", "interface")
        config['router_password'] = parser.get("router", "password")
        config['router_type'] = parser.get("router", "type")

    except Exception as inst:
        raise TrexRunException("\nBad configuration file: '{0}'\n\n{1}".format(filename, inst))

    return config

def prepare_for_run (job_summary):
    global logger
    
    # generate unique id
    job_summary['job_id'] = generate_job_id()
    job_summary['job_dir'] = "trex_job_{0}".format(job_summary['job_id'])
    
    job_summary['start_time'] = datetime.datetime.now()

    if not job_summary['email']:
        job_summary['user'] = getpass.getuser() 
        job_summary['email'] = "{0}@cisco.com".format(job_summary['user'])

    # create dir for reports
    try:
        job_summary['job_dir'] = os.path.abspath( os.path.join(os.getcwd(), 'logs', job_summary['job_dir']) )
        print(job_summary['job_dir'])
        os.makedirs( job_summary['job_dir'] )
        
    except OSError as err:
        if err.errno == errno.EACCES:
            # fall back. try creating the dir name at /tmp path
            job_summary['job_dir'] = os.path.join("/tmp/", "trex_job_{0}".format(job_summary['job_id']) )
            os.makedirs(job_summary['job_dir'])

    job_summary['log_filename'] = os.path.join(job_summary['job_dir'], "trex_log_{0}.txt".format(job_summary['job_id']))
    job_summary['graph_filename'] = os.path.join(job_summary['job_dir'], "trex_graph_{0}.html".format(job_summary['job_id']))

    # init logger
    logger = MyLogger(job_summary['log_filename'])

    # mark those as not populated yet
    job_summary['find_results'] = None
    job_summary['plot_results'] = None

    # create trex client instance
    trex_params = load_trex_config_params(job_summary['config_file'],job_summary['yaml'])
    trex = CTRexClient(trex_host = trex_params['trex_name'], 
        max_history_size = trex_params['trex_hisory_size'], 
        trex_daemon_port = trex_params['trex_port'])

    job_summary['trex'] = trex
    job_summary['trex_params'] = trex_params

    # create trex task thread
    job_summary['trex_thread'] = CTRexWithRouter(trex, trex_params);

    # in case of an error we need to call the remote cleanup
    cleanup_list.append(trex.stop_trex)
    
    # signal handler
    signal.signal(signal.SIGINT, int_signal_handler)
    signal.signal(signal.SIGUSR1, error_signal_handler)
    

def after_run (job_summary):

    job_summary['total_run_time'] = datetime.datetime.now() - job_summary['start_time']
    reporter = JobReporter(job_summary)
    reporter.print_summary()
    reporter.send_email_report()

def launch (job_summary):

    prepare_for_run(job_summary)

    print_header()
  
    log_startup_summary(job_summary)

    # find the correct M range if not provided
    range_results = find_m_range(job_summary)
    
    job_summary['base_m_unit'] = range_results['base_m_unit']

    if job_summary['m_range']:
        m_range = job_summary['m_range']
    else:
        m_range = range_results['m_range']

    logger.log("\nJob Bandwidth Working Range:\n")
    logger.log("Min M = {0:.6f} / {1:,.2f} [Mbps] \nMax M = {2:.6f} / {3:,.2f} [Mbps]".format(m_range[0], m_to_mbps(job_summary, m_range[0]), m_range[1], m_to_mbps(job_summary, m_range[1])))

    # job time
    findjob = FindJob(m_range[0], m_range[1], job_summary)
    job_summary['find_results'] = findjob.find_max_m()

    if job_summary['job_type'] == "all":
        # plot points to graph
        plotjob = PlotJob(findjob)
        job_summary['plot_results'] = plotjob.plot()

    after_run(job_summary)


# populate the fields for run
def populate_fields (job_summary, args):
    job_summary['config_file']  = args.config_file
    job_summary['job_type']     = args.job
    job_summary['cond_type']    = args.cond_type
    job_summary['yaml']         = args.yaml

    if args.n:
        job_summary['job_name']     = args.n
    else:
        job_summary['job_name']     = "Nameless"

    # did the user provided an email
    if args.e:
        job_summary['email']        = args.e
    else:
        job_summary['email']        = None

    # did the user provide a range ?
    if args.m:
        job_summary['m_range'] = args.m
    else:
        job_summary['m_range'] = None

    # some pretty shows
    job_summary['cond_name'] = 'Drop Pkt' if (args.cond_type == 'drop') else 'High Latency'

    if args.job == "find":
        job_summary['job_type_str'] = "Find Max BW"
    elif args.job == "plot":
        job_summary['job_type_str'] = "Plot Graph"
    else:
        job_summary['job_type_str'] = "Find Max BW & Plot Graph"

    if args.job != "find":
        verify_glibc_version()
        


# verify file exists for argparse
def is_valid_file (parser, err_msg, filename):
    if not os.path.exists(filename):
        parser.error("{0}: '{1}'".format(err_msg, filename))
    else:
        return (filename)  # return an open file handle

def entry ():

    job_summary = {}

    parser = argparse.ArgumentParser()

    parser.add_argument("-n", help="Job Name",
                        type = str)

    parser.add_argument("-m", help="M Range [default: auto calcuation]",
                        nargs = 2,
                        type = float)

    parser.add_argument("-e", help="E-Mail for report [default: whoami@cisco.com]",
                        type = str)

    parser.add_argument("-c", "--cfg", dest = "config_file", required = True, 
                        help = "Configuration File For Trex/Router Pair",
                        type = lambda x: is_valid_file(parser, "config file does not exists",x))

    parser.add_argument("job", help = "Job type",
                        type = str,
                        choices = ['find', 'plot', 'all'])

    parser.add_argument("cond_type", help="type of failure condition",
                        type = str,
                        choices = ['latency','drop'])

    parser.add_argument("-f", "--yaml", dest = "yaml", required = True,
                        help="YAML file to use", type = str)

    args = parser.parse_args()

    with TermMng():
        try:
            populate_fields(job_summary, args)
            launch(job_summary)

        except Exception as e:
            ErrorHandler(e, traceback.format_exc())

    logger.log("\nReport bugs to imarom@cisco.com\n")
    g_stop = True

def dummy_test ():
    job_summary = {}
    find_results = {}

    job_summary['config_file'] = 'config/trex01-1g.cfg'
    job_summary['yaml'] = 'dummy.yaml'
    job_summary['email']        = 'imarom@cisco.com'
    job_summary['job_name'] = 'test'
    job_summary['job_type_str'] = 'test'

    prepare_for_run(job_summary)

    time.sleep(2)
    job_summary['yaml'] = 'dummy.yaml'
    job_summary['job_type']  = 'find'
    job_summary['cond_name'] = 'Drop'
    job_summary['cond_type'] = 'drop'
    job_summary['job_id']= 94817231
    

    find_results['tx'] = 210.23
    find_results['m'] = 1.292812
    find_results['total-pps'] = 1000
    find_results['cpu_util'] = 74.0
    find_results['maximum-latency'] = 4892
    find_results['average-latency'] = 201
    find_results['total-pkt-drop'] = 0


    findjob = FindJob(1,1,job_summary)
    plotjob = PlotJob(findjob)
    job_summary['plot_results'] = plotjob.plot()

    job_summary['find_results'] = find_results
    job_summary['plot_results'] = [{'cpu_util': 2.0,'norm_cpu': 1.0,  'total-pps': 1000, 'expected-bps': 999980.0, 'average-latency': 85.0, 'tx': 0.00207*1000, 'total-pkt-drop': 0.0, 'maximum-latency': 221.0},
                                   {'cpu_util': 8.0,'norm_cpu': 1.0,  'total-pps': 1000,'expected-bps': 48500000.0, 'average-latency': 87.0, 'tx': 0.05005*1000, 'total-pkt-drop': 0.0, 'maximum-latency': 279.0},
                                   {'cpu_util': 14.0,'norm_cpu': 1.0, 'total-pps': 1000,'expected-bps': 95990000.0, 'average-latency': 92.0, 'tx': 0.09806*1000, 'total-pkt-drop': 0.0, 'maximum-latency': 273.0},
                                   {'cpu_util': 20.0,'norm_cpu': 1.0, 'total-pps': 1000,'expected-bps': 143490000.0, 'average-latency': 95.0, 'tx': 0.14613*1000, 'total-pkt-drop': 0.0, 'maximum-latency': 271.0},
                                   {'cpu_util': 25.0,'norm_cpu': 1.0, 'total-pps': 1000,'expected-bps': 190980000.0, 'average-latency': 97.0, 'tx': 0.1933*1000, 'total-pkt-drop': 0.0, 'maximum-latency': 302.0},
                                   {'cpu_util': 31.0,'norm_cpu': 1.0, 'total-pps': 1000,'expected-bps': 238480000.0, 'average-latency': 98.0, 'tx': 0.24213*1000, 'total-pkt-drop': 1.0, 'maximum-latency': 292.0},
                                   {'cpu_util': 37.0,'norm_cpu': 1.0, 'total-pps': 1000, 'expected-bps': 285970000.0, 'average-latency': 99.0, 'tx': 0.29011*1000, 'total-pkt-drop': 0.0, 'maximum-latency': 344.0},
                                   {'cpu_util': 43.0,'norm_cpu': 1.0, 'total-pps': 1000, 'expected-bps': 333470000.0, 'average-latency': 100.0, 'tx': 0.3382*1000, 'total-pkt-drop': 0.0, 'maximum-latency': 351.0},
                                   {'cpu_util': 48.0,'norm_cpu': 1.0, 'total-pps': 1000, 'expected-bps': 380970000.0, 'average-latency': 100.0, 'tx': 0.38595*1000, 'total-pkt-drop': 0.0, 'maximum-latency': 342.0},
                                   {'cpu_util': 54.0,'norm_cpu': 1.0, 'total-pps': 1000, 'expected-bps': 428460000.0, 'average-latency': 19852.0, 'tx': 0.43438*1000, 'total-pkt-drop': 1826229.0, 'maximum-latency': 25344.0}]

    

    after_run(job_summary)

if __name__ == "__main__":
    entry ()

