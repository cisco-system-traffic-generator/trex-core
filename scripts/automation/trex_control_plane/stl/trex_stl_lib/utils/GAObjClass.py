try: # Python2
    import Queue
    from urllib2 import *
except: # Python3
    import queue as Queue
    from urllib.request import *
    from urllib.error import *
import threading
import sys
from time import sleep
from pprint import pprint
"""
GAObjClass is a class destined to send Google Analytics Information.

cid - unique number per user.
command - the Event Category rubric appears on site. type: TEXT
action - the Event Action rubric appears on site - type: TEXT
label - the Event Label rubric - type: TEXT
value - the event value metric - type: INTEGER

QUOTAS:
1 single payload - up to 8192Bytes
batched:
A maximum of 20 hits can be specified per request.
The total size of all hit payloads cannot be greater than 16K bytes.
No single hit payload can be greater than 8K bytes.
"""
url_single = 'https://www.google-analytics.com/collect' #sending single event
url_batched = 'https://www.google-analytics.com/batch' #sending batched events
url_debug = 'https://www.google-analytics.com/debug/collect' #verifying hit is valid
url_conn = 'http://172.217.2.196' # testing internet connection to this address (google-analytics server)

#..................................................................class GA_ObjClass................................................................
class GA_ObjClass:
    def __init__(self,cid,trackerID,appName,appVer):
        self.cid = cid
        self.trackerID = trackerID
        self.appName = appName
        self.appVer = appVer
        self.payload = ''
        self.payload = GA_ObjClass.generate_payload(self)
        self.size = sys.getsizeof(self.payload)

    def generate_payload(self):
        self.payload+='v=1&t=event&tid='+str(self.trackerID)
        self.payload+='&cid='+str(self.cid)
        self.payload+='&an='+str(self.appName)
        self.payload+='&av='+str(self.appVer)
        return self.payload


#..................................................................class GA_EVENT_ObjClass................................................................
class GA_EVENT_ObjClass(GA_ObjClass):
    def __init__(self,cid,trackerID,command,action,label,value,appName,appVer):
        GA_ObjClass.__init__(self,cid,trackerID,appName,appVer)
        self.command = command
        self.action = action
        self.label = label
        self.value = value
        self.payload = self.generate_payload()
        self.size = sys.getsizeof(self.payload)

    def generate_payload(self):
        self.payload+='&ec='+str(self.command)
        self.payload+='&ea='+str(self.action)
        self.payload+='&el='+str(self.label)
        self.payload+='&ev='+str(self.value)
        return self.payload

#..................................................................class GA_EXCEPTION_ObjClass................................................................
#ExceptionFatal - BOOLEAN
class GA_EXCEPTION_ObjClass(GA_ObjClass):
    def __init__(self,cid,trackerID,ExceptionName,ExceptionFatal,appName,appVer):
        GA_ObjClass.__init__(self,cid,trackerID,appName,appVer)
        self.ExceptionName = ExceptionName
        self.ExceptionFatal = ExceptionFatal
        self.payload = self.generate_payload()

    def generate_payload(self):
        self.payload+='&exd='+str(self.ExceptionName)
        self.payload+='&exf='+str(self.ExceptionFatal)
        return self.payload



#..................................................................class GA_TESTING_ObjClass................................................................
class GA_TESTING_ObjClass(GA_ObjClass):
    def __init__(self,cid,uuid,trackerID,TRexMode,test_name,setup_name,appName,appVer,commitID,bandwidthPerCore,goldenBPC):
        GA_ObjClass.__init__(self,cid,trackerID,appName,appVer)
        self.uid = uuid
        self.TRexMode = TRexMode
        self.test_name = test_name
        self.setup_name = setup_name
        self.commitID = commitID
        self.bandwidthPerCore = bandwidthPerCore
        self.goldenBPC = goldenBPC
        self.payload = self.generate_payload()
        self.size = sys.getsizeof(self.payload)

    def generate_payload(self):
        self.payload+='&ec='+str(self.TRexMode)
        self.payload+='&ea=RegressionReport'
        self.payload+='&cd5='+str(self.uid)
        self.payload+='&cd1='+str(self.test_name)
        self.payload+='&cd2='+str(self.setup_name)
        self.payload+='&cd3='+str(self.commitID)
        self.payload+='&cm1='+str(self.bandwidthPerCore)
        self.payload+='&cm2='+str(self.goldenBPC)
        return self.payload        
#.....................................................................class ga_Thread.................................................................
"""

Google analytics thread manager:

will report and empty queue of google analytics items to GA server, every Timeout (parameter given on initialization)
will perform connectivity check every timeout*10 seconds

"""

class ga_Thread (threading.Thread):
    def __init__(self,threadID,gManager):
        threading.Thread.__init__(self)
        self.threadID = threadID
        self.gManager = gManager
    def run(self):
        keepAliveCounter=0
        #sys.stdout.write('thread started \n')
        #sys.stdout.flush()
        while True:
            if (keepAliveCounter==10):
                keepAliveCounter=0
                if (self.gManager.internet_on()==True):
                    self.gManager.connectedToInternet=1
                else:
                     self.gManager.connectedToInternet=0
            sleep(self.gManager.Timeout)
            keepAliveCounter+=1
            if not self.gManager.GA_q.empty():
                self.gManager.threadLock.acquire(1)
#               sys.stdout.write('lock acquired: reporting to GA \n')
#               sys.stdout.flush()
                if (self.gManager.connectedToInternet==1):
                    self.gManager.emptyAndReportQ()
                self.gManager.threadLock.release()
#               sys.stdout.write('finished \n')
#               sys.stdout.flush()
#.....................................................................class GAmanager.................................................................
"""

Google ID - specify tracker property, example: UA-75220362-2 (when the suffix '2' specifies the analytics property profile)

UserID - unique userID, this will differ between users on GA

appName - s string to determine app name

appVer - a string to determine app version

QueueSize - the size of the queue that holds reported items. once the Queue is full:
            on blocking mode:
                will block program until next submission to GA server, which will make new space
            on non-blocking mode:
                will drop new requests

Timout - the timeout the queue uses between data transmissions. Timeout should be shorter than the time it takes to generate 20 events. MIN VALUE = 11 seconds

User Permission - the user must accept data transmission, use this flag as 1/0 flag, when UserPermission=1 allows data collection
 
BlockingMode - set to 1 if you wish every Google Analytic Object will be submitted and processed, with no drops allowed. 
               this will block the running of the program until every item is processed

*** Restriction - Google's restriction for amount of packages being sent per session per second is: 1 event per second, per session. session length is 30min ***
"""

class GAmanager:
    def __init__(self,GoogleID,UserID,appName,appVer,QueueSize,Timeout,UserPermission,BlockingMode):
        self.UserID = UserID
        self.GoogleID = GoogleID
        self.QueueSize = QueueSize
        self.Timeout = Timeout
        self.appName = appName
        self.appVer = appVer
        self.UserPermission = UserPermission
        self.GA_q = Queue.Queue(QueueSize)
        self.thread = ga_Thread(UserID,self)
        self.threadLock = threading.Lock()
        self.BlockingMode = BlockingMode
        self.connectedToInternet =0
        if (self.internet_on()==True):
#           sys.stdout.write('internet connection active \n')
#           sys.stdout.flush()
            self.connectedToInternet=1
        else:
            self.connectedToInternet=0

    def gaAddAction(self,Event,action,label,value):
        self.gaAddObject(GA_EVENT_ObjClass(self.UserID,self.GoogleID,Event,action,label,value,self.appName,self.appVer))

    def gaAddException(self,ExceptionName,ExceptionFatal):
        self.gaAddObject(GA_EXCEPTION_ObjClass(self.UserID,self.GoogleID,ExceptionName,ExceptionFatal,self.appName,self.appVer))

    def gaAddObject(self,Object):
        if (self.BlockingMode==1):
            while (self.GA_q.full()):
                sleep(self.Timeout)
#               sys.stdout.write('blocking mode=1 \n queue full - sleeping for timeout \n') # within Timout, the thread will empty part of the queue
#               sys.stdout.flush()
        lockState = self.threadLock.acquire(self.BlockingMode)
        if (lockState==1):
#           sys.stdout.write('got lock, adding item \n')
#           sys.stdout.flush()
            try:
                self.GA_q.put_nowait(Object)
#               sys.stdout.write('got lock, item added \n')
#               sys.stdout.flush()
            except Queue.Full:
#               sys.stdout.write('Queue full \n')
#               sys.stdout.flush()
                pass
            self.threadLock.release()

    def emptyQueueToList(self,obj_list):
        items=0
        while ((not self.GA_q.empty()) and (items<20)):
            obj_list.append(self.GA_q.get_nowait().payload)
            items+=1
#           print items
        return obj_list

    def reportBatched(self,batched):
        req = Request(url_batched, data=batched.encode('ascii'))
        urlopen(req)
#       pprint(r.json())

    def emptyAndReportQ(self):
        obj_list = []
        obj_list = self.emptyQueueToList(obj_list)
        if (len(obj_list)==0):
            return
        batched = '\n'.join(obj_list)
#       print sys.getsizeof(batched)
#       print batched # - for debug
        self.reportBatched(batched)

    def printSelf(self):
        print('remaining in queue:')
        while not self.GA_q.empty():
            obj = self.GA_q.get_nowait()
            print(obj.payload)

    def internet_on(self):
        try:
            urlopen(url_conn,timeout=10)
            return True
        except URLError as err: pass
        return False

    def activate(self):
        if (self.UserPermission==1):
            self.thread.start()


#.....................................................................class GAmanager_Regression.................................................................
"""
            *-*-*-*-Google Analytics Regression Manager-*-*-*-*
            attributes:
GoogleID - the tracker ID that Google uses in order to track the activity of a property. for regression use: 'UA-75220362-4'
AnalyticsUserID - text value  - used by Google to differ between 2 users sending data. (will not be presented on reports). use only as a way to differ between different users
UUID - text  - will be presented on analysis. put here UUID
TRexMode - text - will be presented on analysis. put here TRexMode
appName - text - will be presented on analysis. put here appName as string describing app name
appVer - text - will be presented on analysis. put here the appVer
QueueSize - integer - determines the queue size. the queue will hold pending request before submission. RECOMMENDED VALUE: 20
Timeout - integer (seconds) - the timeout in seconds between automated reports when activating reporting thread
UserPermission - boolean (1/0) - required in order to send packets, should be 1.
BlockingMode - boolean (1/0) - required when each tracked event is critical and program should halt until the event is reported
SetupName - text - will be presented on analysis. put here setup name as string.
CommitID - text - will be presented on analysis. put here CommitID 
"""
class GAmanager_Regression(GAmanager):
    def __init__(self,GoogleID,AnalyticsUserID,UUID,TRexMode,appName,appVer,
                 QueueSize,Timeout,UserPermission,BlockingMode,SetupName,CommitID):
        GAmanager.__init__(self,GoogleID,AnalyticsUserID,appName,appVer,
                           QueueSize,Timeout,UserPermission,BlockingMode)
        self.UUID = UUID
        self.TRexMode = TRexMode
        self.SetupName = SetupName
        self.CommitID = CommitID

    def gaAddTestQuery(self,TestName,BandwidthPerCore,GoldenBPC):
        self.gaAddObject(GA_TESTING_ObjClass(self.UserID,self.UUID,self.GoogleID,
                                             self.TRexMode,TestName,self.SetupName,
                                             self.appName,self.appVer,self.CommitID,
                                             BandwidthPerCore,GoldenBPC))




#***************************************------TEST--------------**************************************


if __name__ == '__main__':

#g= GAmanager_Regression(GoogleID='UA-75220362-4',AnalyticsUserID=3845,UUID='trex18UUID_GA_TEST',TRexMode='stateFull_GA_TEST',
#             appName='TRex_GA_TEST',appVer='1.1_GA_TEST',QueueSize=20,Timeout=11,UserPermission=1,BlockingMode=0,SetupName='setup1_GA_TEST',CommitID='commitID1_GA_TEST')
#for j in range(1,3,1):
#for i in range(100,118,1):
#    g.gaAddTestQuery('test_name_GA_TEST',i+0.5,150)
#   sleep(11)
#   print "finished batch"
#g.emptyAndReportQ()

#g.printSelf()
#g.emptyAndReportQ()

#g = GAmanager(GoogleID='UA-75220362-4',UserID=1,QueueSize=100,Timeout=5,UserPermission=1,BlockingMode=0,appName='TRex',appVer='1.11.232') #timeout in seconds
#for i in range(0,35,1):
#   g.gaAddAction(Event='test',action='start',label='1',value=i)
#g.gaAddException('MEMFAULT',1)
#g.emptyAndReportQ()
#g.printSelf()
#print g.payload
#print g.size




#g.activate()
#g.gaAddAction(Event='test',action='start',label='1',value='1')
#sys.stdout.write('element added \n')
#sys.stdout.flush()
#g.gaAddAction(Event='test',action='start',label='2',value='1')
#sys.stdout.write('element added \n')
#sys.stdout.flush()
#g.gaAddAction(Event='test',action='start',label='3',value='1')
#sys.stdout.write('element added \n')
#sys.stdout.flush()

#testdata = "v=1&t=event&tid=UA-75220362-4&cid=2&ec=test&ea=testing&el=testpacket&ev=2"
#r = requests.post(url_debug,data=testdata)
#print r

#thread1 = ga_Thread(1,g)
#thread1.start()
#thread1.join()
#for i in range(1,10,1):
#    sys.stdout.write('yesh %d'% (i))
#    sys.stdout.flush()




   
        





