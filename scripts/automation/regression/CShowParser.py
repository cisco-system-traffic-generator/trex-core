#!/router/bin/python-2.7.4

import re
import misc_methods

class PlatformResponseMissmatch(Exception): 
    def __init__(self, message):
        # Call the base class constructor with the parameters it needs
        super(PlatformResponseMissmatch, self).__init__(message + ' is not available for given platform state and data.\nPlease make sure the relevant features are turned on in the platform.')

class PlatformResponseAmbiguity(Exception): 
    def __init__(self, message):
        # Call the base class constructor with the parameters it needs
        super(PlatformResponseAmbiguity, self).__init__(message + ' found more than one file matching the provided filename.\nPlease provide more distinct filename.')


class CShowParser(object):

    @staticmethod
    def parse_drop_stats (query_response, interfaces_list):
        res          = {'total_drops' : 0}
        response_lst = query_response.split('\r\n')
        mtch_found   = 0

        for line in response_lst:
            mtch = re.match("^\s*(\w+/\d/\d)\s+(\d+)\s+(\d+)", line)
            if mtch:
                mtch_found += 1
                if (mtch.group(1) in interfaces_list):
                    res[mtch.group(1)] = (int(mtch.group(2)) + int(mtch.group(3)))
                    res['total_drops'] += (int(mtch.group(2)) + int(mtch.group(3)))
#       if mtch_found == 0: # no matches found at all
#           raise PlatformResponseMissmatch('Drop stats')
#       else:
#           return res
        return res

    @staticmethod
    def parse_nbar_stats (query_response):
        response_lst = query_response.split('\r\n')
        stats        = {}
        final_stats  = {}
        mtch_found   = 0

        for line in response_lst:
            mtch = re.match("\s*([\w-]+)\s*(\d+)\s*(\d+)\s+", line)
            if mtch:
                mtch_found += 1
                key     = mtch.group(1)
                pkt_in  = int(mtch.group(2))
                pkt_out = int(mtch.group(3))

                avg_pkt_cnt = ( pkt_in + pkt_out )/2
                if avg_pkt_cnt == 0.0:
                    # escaping zero division case
                    continue
                if key in stats:
                    stats[key] += avg_pkt_cnt
                else:
                    stats[key] = avg_pkt_cnt
        
        # Normalize the results to percents
        for protocol in stats:
            protocol_norm_stat = int(stats[protocol]*10000/stats['Total'])/100.0 # round the result to x.xx format
            if (protocol_norm_stat != 0.0):
                final_stats[protocol] = protocol_norm_stat

        if mtch_found == 0: # no matches found at all
            raise PlatformResponseMissmatch('NBAR classification stats')
        else:
            return { 'percentage' : final_stats, 'packets' : stats }

    @staticmethod
    def parse_nat_stats (query_response):
        response_lst = query_response.split('\r\n')
        res          = {}
        mtch_found   = 0

        for line in response_lst:
            mtch = re.match("Total (active translations):\s+(\d+).*(\d+)\s+static,\s+(\d+)\s+dynamic", line)
            if mtch:
                mtch_found += 1
                res['total_active_trans']   = int(mtch.group(2))
                res['static_active_trans']  = int(mtch.group(3))
                res['dynamic_active_trans'] = int(mtch.group(4))
                continue

            mtch = re.match("(Hits):\s+(\d+)\s+(Misses):\s+(\d+)", line)
            if mtch:
                mtch_found += 1
                res['num_of_hits']   = int(mtch.group(2))
                res['num_of_misses'] = int(mtch.group(4))

        if mtch_found == 0: # no matches found at all
            raise PlatformResponseMissmatch('NAT translations stats')
        else:
            return res

    @staticmethod
    def parse_cpu_util_stats (query_response):
        response_lst = query_response.split('\r\n')
        res = { 'cpu0' : 0,
                'cpu1' : 0 }
        mtch_found = 0
        for line in response_lst:
            mtch = re.match("\W*Processing: Load\D*(\d+)\D*(\d+)\D*(\d+)\D*(\d+)\D*", line)
            if mtch:
                mtch_found += 1
                res['cpu0'] += float(mtch.group(1))
                res['cpu1'] += float(mtch.group(2))

        if mtch_found == 0: # no matches found at all
            raise PlatformResponseMissmatch('CPU utilization processing')
        else:
            res['cpu0'] = res['cpu0']/mtch_found
            res['cpu1'] = res['cpu1']/mtch_found
            return res

    @staticmethod
    def parse_cft_stats (query_response):
        response_lst = query_response.split('\r\n')
        res = {}
        mtch_found = 0
        for line in response_lst:
            mtch = re.match("\W*(\w+)\W*([:]|[=])\W*(\d+)", line)
            if mtch:
                mtch_found += 1
                res[ str( mix_string(m.group(1)) )] = float(m.group(3))
        if mtch_found == 0: # no matches found at all
            raise PlatformResponseMissmatch('CFT counters stats')
        else:
            return res


    @staticmethod
    def parse_cvla_memory_usage(query_response):
        response_lst = query_response.split('\r\n')
        res      = {}
        res2     = {}
        cnt      = 0
        state    = 0
        name     = ''
        number   = 0.0

        for line in response_lst:
            if state == 0:
                mtch = re.match("\W*Entity name:\W*(\w[^\r\n]+)", line)
                if mtch:
                    name = misc_methods.mix_string(mtch.group(1))
                    state = 1
                    cnt += 1
            elif state == 1:
                mtch = re.match("\W*Handle:\W*(\d+)", line)
                if mtch:
                    state = state + 1
                else:
                    state = 0;
            elif state == 2:
                mtch = re.match("\W*Number of allocations:\W*(\d+)", line)
                if mtch:
                    state = state + 1
                    number=float(mtch.group(1))
                else:
                    state = 0;
            elif state == 3:
                mtch = re.match("\W*Memory allocated:\W*(\d+)", line)
                if mtch:
                    state = 0
                    res[name]   = float(mtch.group(1))
                    res2[name]  = number
                else:
                    state = 0
        if cnt == 0:
            raise PlatformResponseMissmatch('CVLA memory usage stats')

        return (res,res2)

        
    @staticmethod
    def parse_show_image_version(query_response):
        response_lst = query_response.split('\r\n')
        res      = {}

        for line in response_lst:
            mtch = re.match("System image file is \"(\w+):(.*/)?(.+)\"", line)
            if mtch:
                res['drive'] = mtch.group(1)
                res['image'] = mtch.group(3)
                return res

        raise PlatformResponseMissmatch('Running image info')


    @staticmethod
    def parse_image_existence(query_response, img_name):
        response_lst = query_response.split('\r\n')
        cnt      = 0

        for line in response_lst:
            regex = re.compile(".* (?!include) %s" % img_name )
            mtch = regex.match(line)
            if mtch:
                cnt += 1
        if cnt == 1:
            return True
        elif cnt > 1:
            raise PlatformResponseAmbiguity('Image existence')
        else:
            return False

    @staticmethod
    def parse_file_copy (query_response):
        rev_response_lst = reversed(query_response.split('\r\n'))
        lines_parsed     = 0

        for line in rev_response_lst:
            mtch = re.match("\[OK - (\d+) bytes\]", line)
            if mtch:
                return True
            lines_parsed += 1

            if lines_parsed > 5:
                return False
        return False


if __name__ == "__main__":
    pass
