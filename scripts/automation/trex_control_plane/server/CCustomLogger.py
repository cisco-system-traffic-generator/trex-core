
import sys
import os
import logging

def prepare_dir(log_path):
    log_dir = os.path.dirname(log_path)
    if not os.path.exists(log_dir):
        os.makedirs(log_dir)

def setup_custom_logger(name, log_path = None):
    # first make sure path availabe
    logging.basicConfig(level   = logging.INFO, 
                        format  = '%(asctime)s %(name)-10s %(module)-20s %(levelname)-8s %(message)s',
                        datefmt = '%m-%d %H:%M')
#                       filename= log_path,
#                       filemode= 'w')
#
#   # define a Handler which writes INFO messages or higher to the sys.stderr
#   consoleLogger = logging.StreamHandler()
#   consoleLogger.setLevel(logging.ERROR)
#   # set a format which is simpler for console use
#   formatter = logging.Formatter('%(name)-12s: %(levelname)-8s %(message)s')
#   # tell the handler to use this format
#   consoleLogger.setFormatter(formatter)
#
#   # add the handler to the logger
#   logging.getLogger(name).addHandler(consoleLogger)

def setup_daemon_logger (name, log_path = None):
    # first make sure path availabe
    prepare_dir(log_path)
    try:
        os.unlink(log_path)
    except:
        pass
    logging.basicConfig(level   = logging.INFO, 
                        format  = '%(asctime)s %(name)-10s %(module)-20s %(levelname)-8s %(message)s',
                        datefmt = '%m-%d %H:%M',
                        filename= log_path,
                    filemode= 'w')

class CustomLogger(object):
  
    def __init__(self, log_filename):
        # Store the original stdout and stderr
        sys.stdout.flush()
        sys.stderr.flush()

        self.stdout_fd = os.dup(sys.stdout.fileno())
        self.devnull = os.open('/dev/null', os.O_WRONLY)
        self.log_file = open(log_filename, 'w')
        self.silenced = False
        self.pending_log_file_prints = 0

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
        self.log_file.write((text + "\n") if newline else text)
        self.pending_log_file_prints += 1

        if (self.pending_log_file_prints >= 10):
             self.log_file.flush()
             self.pending_log_file_prints = 0

        self.console(text, force, newline)

    # print a message to the console alone
    def console(self, text, force = False, newline = True):
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

    def __exit__(self, type, value, traceback):
        sys.stdout.flush()
        self.log_file.flush()
        os.close(self.devnull)
        os.close(self.log_file)
