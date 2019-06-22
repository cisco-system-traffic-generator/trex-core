#ifndef IPG_BUCKET_H
#define IPG_BUCKET_H

/*
 Hanoh Haim
 Cisco Systems, Inc.
*/

/*
Copyright (c) 2015-2015 Cisco Systems, Inc.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/


class CCalcIpgDiff {

public:
    CCalcIpgDiff(double bucket_is_sec){
        m_acc=0;
        m_bucket_is_sec=bucket_is_sec;
        m_cnt=0;
    }

    uint32_t do_calc(double ipg_sec){
        uint32_t res;
        double d_residue=(m_acc+ipg_sec);
        double d_buckets = (d_residue/m_bucket_is_sec);
        if (d_buckets< 1.0) {
            m_acc+=ipg_sec;
            m_cnt++;
            if (m_cnt>30) {
                m_cnt=0; /* move at least 1 bucket after 20 packets not to create Infinite loop in timer wheel*/
                return(1);
            }
            return(0);
        }
        m_cnt=0;
        if (d_buckets>(double)UINT32_MAX) {
            d_buckets=(double)UINT32_MAX;
        }
        res=((uint32_t)d_buckets);
        m_acc=(d_residue)-m_bucket_is_sec*(double)res;
        return (res);
    }

private:

    double m_acc;
    double m_bucket_is_sec;
    uint8_t m_cnt;

};




#endif
