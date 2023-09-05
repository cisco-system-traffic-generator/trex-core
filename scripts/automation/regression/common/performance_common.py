import os

# performance report object
class PerformanceReport(object):
    GOLDEN_NORMAL  = 1
    GOLDEN_FAIL    = 2
    GOLDEN_BETTER  = 3

    def __init__ (self,
                  scenario,
                  machine_name,
                  core_count,
                  avg_cpu,
                  avg_gbps,
                  avg_mpps,
                  avg_gbps_per_core,
                  avg_mpps_per_core,
                  ):

        self.scenario                = scenario
        self.machine_name            = machine_name
        self.core_count              = core_count
        self.avg_cpu                 = avg_cpu
        self.avg_gbps                = avg_gbps
        self.avg_mpps                = avg_mpps
        self.avg_gbps_per_core       = avg_gbps_per_core
        self.avg_mpps_per_core       = avg_mpps_per_core

    def show (self):

        print("\n")
        print("scenario:                                {0}".format(self.scenario))
        print("machine name:                            {0}".format(self.machine_name))
        print("DP core count:                           {0}".format(self.core_count))
        print("average CPU:                             {0}".format(self.avg_cpu))
        print("average Gbps:                            {0}".format(self.avg_gbps))
        print("average Mpps:                            {0}".format(self.avg_mpps))
        print("average pkt size (bytes):                {0}".format( (self.avg_gbps * 1000 / 8) / self.avg_mpps))
        print("average Gbps per core (at 100% CPU):     {0}".format(self.avg_gbps_per_core))
        print("average Mpps per core (at 100% CPU):     {0}".format(self.avg_mpps_per_core))


    def check_golden (self, golden_mpps):
        if self.avg_mpps_per_core < golden_mpps['min']:
            return self.GOLDEN_FAIL

        if self.avg_mpps_per_core > golden_mpps['max']:
            return self.GOLDEN_BETTER

        return self.GOLDEN_NORMAL

    def report_to_analytics(self, ga, golden_mpps, trex_mode="stl"):
        print("\n* Reporting to GA *\n")
        ga.gaAddTestQuery(TestName = self.scenario,
                          TRexMode = trex_mode,
                          SetupName = self.machine_name,
                          TestType = 'performance',
                          Mppspc = self.avg_mpps_per_core,
                          ActionNumber = os.getenv("BUILD_NUM","n/a"),
                          GoldenMin = golden_mpps['min'],
                          GoldenMax = golden_mpps['max'])

        ga.emptyAndReportQ()

    def norm_senario (self):
        s=self.scenario
        s='+'.join(s.split(' '))
        s='+'.join(s.split('-'))
        s='+'.join(s.split(','))
        l=s.split('+')
        lr=[]
        for obj in l:
            if len(obj):
                lr.append(obj)
        s='-'.join(lr)
        return(s)

    def report_to_elk(self, elk,elk_obj, test_type="stateless"):
        print("\n* Reporting to elk *\n")
        elk_obj['test']={ "name" : self.norm_senario(),
                          "type"  : test_type,
                          "cores" : self.core_count,
                          "cpu%"  : self.avg_cpu,
                          "mpps" :  self.avg_mpps,
                          "streams_count" : 1,
                          "mpps_pc" :  self.avg_mpps_per_core,
                          "gbps_pc" :  self.avg_gbps_per_core,
                          "gbps" :  self.avg_gbps,
                          "avg-pktsize" : ((1000.0*self.avg_gbps/(8.0*self.avg_mpps))),
                          "latecny" : {
                                        "min" : -1.0,
                                        "max" : -1.0,
                                        "avr" : -1.0
                                      }
                        }

        #pprint.pprint(elk_obj)
        # push to elk 
        elk.perf.push_data(elk_obj)