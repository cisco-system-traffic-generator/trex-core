#include "timer_wheel_pq.h"
#include "utl_json.h"
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


#define DP(a) fprintf(fd," %-40s   : %d \n",#a,(int)a)
#define DP_OUT(a) fprintf(stdout," %-40s   : %d \n",#a,(int)a)
#define DP_J(f)  json+=add_json(#f,f);
#define DP_J_LAST(f)  json+=add_json(#f,f,true);


void CTimerWheel::Dump(FILE *fd){
    DP(m_st_alloc);
    DP(m_st_free);
    DP(m_st_start);
    DP(m_st_stop);
    DP(m_st_handle);
    uint64_t m_active=m_st_alloc-m_st_free;
    DP(m_active);
}


void CTimerWheel::dump_json(std::string & json ){
    json+="\"timer_w\" : {";
    DP_J(m_st_alloc);
    DP_J(m_st_free);
    DP_J(m_st_start);
    DP_J(m_st_stop);
    DP_J(m_st_handle);
    uint64_t m_active=m_st_alloc-m_st_free;
    /* MUST BE LAST */
    DP_J_LAST(m_active);
    json+="},";
}



void CTimerWheel::restart_timer(CFlowTimerHandle *  timer, 
	double new_time){


    m_st_start++;
	if (timer->m_timer == 0){
		/* first time add the new time*/
		CFlowTimer * t = new CFlowTimer();
        m_st_alloc++;
		t->m_time = new_time;
		t->m_flow = timer;
		timer->m_timer = t;
		m_pq.push(t);
	}
	else{
		CFlowTimer * t = timer->m_timer;
		if (new_time > t->m_time){
			t->m_updated_time = new_time;
		}else{
			t->m_flow = 0;/* kill old when it timout  */
			CFlowTimer * t = new CFlowTimer(); /* alloc new one */
            m_st_alloc++;
			t->m_time = new_time;
			t->m_flow = timer;
			timer->m_timer = t;
			m_pq.push(t);
		}
   }
}

void CTimerWheel::stop_timer(CFlowTimerHandle *  timer){

	CFlowTimer * t = timer->m_timer;
	if (t){
        m_st_stop++;
		t->m_flow = 0; 
		timer->m_timer = 0;
	}
};



bool  CTimerWheel::peek_top_time(double & time){

	while (!m_pq.empty()) {
		CFlowTimer * timer = m_pq.top();
		if (!timer->is_valid()){
			m_pq.pop();
            m_st_free++;
			delete timer;
		}
		else{
			if (timer->m_updated_time > 0.0 && (timer->m_updated_time > timer->m_time )) {
				timer->m_time = timer->m_updated_time;
				m_pq.pop();
				m_pq.push(timer);

			} else{
				assert(timer->m_flow);
                time=   timer->m_time;
                return (true);
			}
		}
	}
    return (false);
}

void CTimerWheel::drain_all(void){

    double tw_time;
    while (true) {
        if ( peek_top_time(tw_time) ){
                handle();
        }else{
            break;
        }
    }
}


void CTimerWheel::try_handle_events(double now){
    double min_time;
    while (true) {
        if ( peek_top_time(min_time) ){
            if (min_time < now ) {
                handle();
            }else{
                break;
            }
        }else{
            break;
        }
    }
}


bool CTimerWheel::handle(){

    while (!m_pq.empty()) {
        CFlowTimer * timer = m_pq.top();
        if (!timer->is_valid()){
            m_pq.pop();
            m_st_free++;
            delete timer;
        }
        else{
            if (timer->m_updated_time > 0.0 && (timer->m_updated_time > timer->m_time ) ) {
                timer->m_time = timer->m_updated_time;
                m_pq.pop();
                m_pq.push(timer);

            } else{
                assert(timer->m_flow);
                CFlowTimerHandle *	flow =timer->m_flow;
                m_st_handle++;
                if ( flow->m_callback ){
                    flow->m_callback(flow);
                }
                timer->m_flow=0;/* stop the timer */
                flow->m_timer=0;
                m_pq.pop();
                m_st_free++;
                delete timer;
                return(true);
            }
        }
    }
    return(false);
}


#ifdef TW_TESTS


void  flow_callback(CFlowTimerHandle * timer_handle);

class CTestFlow {
public:
	CTestFlow(){
		flow_id = 0;
        m_timer_handle.m_callback=flow_callback;
		m_timer_handle.m_object = (void *)this;
		m_timer_handle.m_id = 0x1234;
	}

	uint32_t		flow_id;
	CFlowTimerHandle m_timer_handle;
public:
	void OnTimeOut(){
        printf(" timeout %d \n",flow_id);
	}
};

void  flow_callback(CFlowTimerHandle * t){
    CTestFlow * lp=(CTestFlow *)t->m_object;
    assert(lp);
    assert(t->m_id==0x1234);
    lp->OnTimeOut();
}

CTimerWheel  my_tw;


void tw_test1(){

    CTestFlow f1;
    CTestFlow f2;
    CTestFlow f3;
    CTestFlow f4;

    f1.flow_id=1;
    f2.flow_id=2;
    f3.flow_id=3;
    f4.flow_id=4;
    double time;
    assert(my_tw.peek_top_time(time)==false);
    my_tw.restart_timer(&f1.m_timer_handle,10.0);
    my_tw.restart_timer(&f2.m_timer_handle,5.0);
    my_tw.restart_timer(&f3.m_timer_handle,1.0);
    assert(my_tw.peek_top_time(time)==true);
    printf(" time %f \n",time);
    assert(time ==1.0);

    assert(my_tw.peek_top_time(time)==true);
    printf(" time %f \n",time);
    assert(time ==1.0);

    assert(my_tw.peek_top_time(time)==true);
    printf(" time %f \n",time);
    assert(time ==1.0);

    assert(my_tw.handle());

    assert(my_tw.peek_top_time(time)==true);
    printf(" time %f \n",time);
    assert(time ==5.0);

    assert(my_tw.handle());

    assert(my_tw.peek_top_time(time)==true);
    printf(" time %f \n",time);
    assert(time ==10.0);

    assert(my_tw.handle());

}

void test2(){

    CTestFlow f1;
    CTestFlow f2;
    CTestFlow f3;
    CTestFlow f4;

    f1.flow_id=1;
    f2.flow_id=2;
    f3.flow_id=3;
    f4.flow_id=4;
    double time;
    assert(my_tw.peek_top_time(time)==false);
    my_tw.restart_timer(&f1.m_timer_handle,10.0);
    my_tw.restart_timer(&f2.m_timer_handle,5.0);
    my_tw.restart_timer(&f3.m_timer_handle,1.0);
    my_tw.stop_timer(&f1.m_timer_handle);

    assert(my_tw.peek_top_time(time)==true);
    printf(" time %f \n",time);
    assert(time ==1.0);

    assert(my_tw.peek_top_time(time)==true);
    printf(" time %f \n",time);
    assert(time ==1.0);

    assert(my_tw.peek_top_time(time)==true);
    printf(" time %f \n",time);
    assert(time ==1.0);

    assert(my_tw.handle());

    assert(my_tw.peek_top_time(time)==true);
    printf(" time %f \n",time);
    assert(time ==5.0);

    assert(my_tw.handle());

    assert(my_tw.peek_top_time(time)==false);
    my_tw.Dump(stdout);

}

void test3(){
	int i;
    for (i=0; i<100; i++) {
        CTestFlow * f= new CTestFlow();
        f->flow_id=(uint32_t)i;
        my_tw.restart_timer(&f->m_timer_handle,100.0-(double)i);
    }

	double time;
    while (true) {
        if ( my_tw.peek_top_time(time) ){
            printf(" %f \n",time);
            assert(my_tw.handle());
		}
		else{
			break;
		}
    }
    my_tw.Dump(stdout);
}

void test4(){
	int i;
	for (i = 0; i<100; i++) {
		CTestFlow * f = new CTestFlow();
		f->flow_id = (uint32_t)i;
		my_tw.restart_timer(&f->m_timer_handle, 500.0 - (double)i);
		my_tw.restart_timer(&f->m_timer_handle, 1000.0 - (double)i);
		my_tw.restart_timer(&f->m_timer_handle, 100.0 - (double)i);
		my_tw.stop_timer(&f->m_timer_handle);
	}


	double time;
	while (true) {
		if (my_tw.peek_top_time(time)){
			printf(" %f \n", time);
			assert(my_tw.handle());
		}
		else{
			break;
		}
	}
	my_tw.Dump(stdout);
}

#endif


