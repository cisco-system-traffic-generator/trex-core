#!/router/bin/python
from __future__ import print_function
import threading
import sys
import time
import outer_packages
import termstyle
import progressbar


class ProgressThread(threading.Thread):
    def __init__(self, notifyMessage = None):
        super(ProgressThread, self).__init__()
        self.stoprequest = threading.Event()
        self.notifyMessage = notifyMessage

    def run(self):
        if self.notifyMessage is not None:
            print(self.notifyMessage, end=' ')

        while not self.stoprequest.is_set():
            print("\b.", end=' ')
            sys.stdout.flush()
            time.sleep(5)

    def join(self, timeout=None):
        if self.notifyMessage is not None:
            print(termstyle.green("Done!\n"), end=' ')
        self.stoprequest.set()
        super(ProgressThread, self).join(timeout)


class TimedProgressBar(threading.Thread):
    def __init__(self, time_in_secs):
        super(TimedProgressBar, self).__init__()
        self.stoprequest    = threading.Event()
        self.stopFlag       = False
        self.time_in_secs   = time_in_secs + 15 # 80 # taking 15 seconds extra
        widgets             = ['Running TRex: ', progressbar.Percentage(), ' ',
                   progressbar.Bar(marker='>',left='[',right=']'),
                   ' ', progressbar.ETA()]
        self.pbar           = progressbar.ProgressBar(widgets=widgets, maxval=self.time_in_secs*2)
        

    def run (self):
        # global g_stop
        print()
        self.pbar.start()

        try:
            for i in range(0, self.time_in_secs*2 + 1):
                if (self.stopFlag == True):
                    break
                time.sleep(0.5)
                self.pbar.update(i)
            # self.pbar.finish()

        except KeyboardInterrupt:
            # self.pbar.finish()
            print("\nInterrupted by user!!")
            self.join()
        finally:
            print()

    def join(self, isPlannedStop = True, timeout=None):
        if isPlannedStop:
            self.pbar.update(self.time_in_secs*2)
            self.stopFlag = True
        else:
            self.stopFlag = True # Stop the progress bar in its current location
        self.stoprequest.set()
        super(TimedProgressBar, self).join(timeout)


def timedProgressBar(time_in_secs):
    widgets = ['Running TRex: ', progressbar.Percentage(), ' ',
                   Bar(marker='>',left='[',right=']'),
                   ' ', progressbar.ETA()]
    pbar = progressbar.ProgressBar(widgets=widgets, maxval=time_in_secs*2)
    pbar.start()
    for i in range(0, time_in_secs*2 + 1):
        time.sleep(0.5)
        pbar.update(i)
    pbar.finish()
    print()
        
        
