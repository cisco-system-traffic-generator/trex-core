
import sys
import os
import logging


# def setup_custom_logger(name, log_path = None):
#     logging.basicConfig(level   = logging.INFO, 
#                         format  = '%(asctime)s %(name)-10s %(module)-20s %(levelname)-8s %(message)s',
#                         datefmt = '%m-%d %H:%M')


def setup_custom_logger(name, log_path = None):
    # first make sure path availabe
    if log_path is None:
        log_path = os.getcwd()+'/trex_log.log'
    else:
        directory = os.path.dirname(log_path)
        if not os.path.exists(directory):
            os.makedirs(directory)
    logging.basicConfig(level   = logging.DEBUG, 
                        format  = '%(asctime)s %(name)-10s %(module)-20s %(levelname)-8s %(message)s',
                        datefmt = '%m-%d %H:%M',
                        filename= log_path, 
                        filemode= 'w')

    # define a Handler which writes INFO messages or higher to the sys.stderr
    consoleLogger = logging.StreamHandler()
    consoleLogger.setLevel(logging.ERROR)
    # set a format which is simpler for console use
    formatter = logging.Formatter('%(name)-12s: %(levelname)-8s %(message)s')
    # tell the handler to use this format
    consoleLogger.setFormatter(formatter)

    # add the handler to the logger
    logging.getLogger(name).addHandler(consoleLogger) 