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

#include <common/gtest.h>
#include <common/basic_utils.h>
#include "h_timer.h"
#include <common/utl_gcc_diag.h>
#include <cmath>


class gt_r_timer  : public testing::Test {

protected:
  virtual void SetUp() {
  }

  virtual void TearDown() {
  }
public:
};




TEST_F(gt_r_timer, timer1) {
    CHTimerObj timer;
    timer.reset();
    timer.Dump(stdout);
}



TEST_F(gt_r_timer, timer2) {

    CHTimerOneWheel timer;

    EXPECT_EQ( timer.Create(513),RC_HTW_ERR_NO_LOG2);

}

TEST_F(gt_r_timer, timer3) {

    CHTimerOneWheel timer;
    EXPECT_EQ( timer.Create(512),RC_HTW_OK);
    EXPECT_EQ( timer.Delete(),RC_HTW_OK);
}


void tw_on_tick_cb_test1(void *userdata,
                         CHTimerObj *tmr){
    //int tick=(int)((uintptr_t)userdata);

    printf(" action %lu \n",(ulong)tmr->m_pad[0]);
}


TEST_F(gt_r_timer, timer4) {

    CHTimerOneWheel timer;
    CHTimerObj  tmr1;
    CHTimerObj  tmr2;
    tmr1.reset();
    tmr2.reset();

    tmr1.m_ticks_left=0;
    tmr1.m_pad[0]=1;
    tmr2.m_ticks_left=0;
    tmr2.m_pad[0]=2;

    EXPECT_EQ( timer.Create(512),RC_HTW_OK);
    timer.timer_start(&tmr1,7);
    timer.timer_start(&tmr2,7);
    timer.dump_link_list(1,0,tw_on_tick_cb_test1,stdout);
    timer.dump_link_list(7,0,tw_on_tick_cb_test1,stdout);


    EXPECT_EQ( timer.Delete(),RC_HTW_OK);
}

TEST_F(gt_r_timer, timer5) {

    CHTimerOneWheel timer;
    CHTimerObj  tmr1;
    CHTimerObj  tmr2;
    tmr1.reset();
    tmr2.reset();

    tmr1.m_ticks_left=0;
    tmr1.m_pad[0]=1;
    tmr2.m_ticks_left=0;
    tmr2.m_pad[0]=2;

    EXPECT_EQ( timer.Create(512),RC_HTW_OK);
    timer.timer_start(&tmr1,7);
    timer.timer_start(&tmr2,7);
    timer.dump_link_list(1,0,tw_on_tick_cb_test1,stdout);
    timer.dump_link_list(7,0,tw_on_tick_cb_test1,stdout);

    assert( timer.pop_event()==0);
    int i;
    for (i=0; i<7; i++) {
        timer.timer_tick();
    }
    CHTimerObj * obj=timer.pop_event();
    assert(obj==&tmr1);

    timer.dump_link_list(7,0,tw_on_tick_cb_test1,stdout);


    obj=timer.pop_event();
    assert(obj==&tmr2);

    EXPECT_EQ( timer.Delete(),RC_HTW_OK);
}

class CMyTestObject {
public:
    CMyTestObject(){
        m_id=0;
        m_d_tick=0;
        m_t_tick=0;
        m_timer.reset();
    }
    uint32_t    m_id;
    uint32_t    m_d_tick;
    uint32_t    m_t_tick;
    CHTimerObj  m_timer;
};


#define MY_OFFSET_OF(cls,member) ((uintptr_t)(&(cls *0)->member) )

void my_test_on_tick_cb(void *userdata,CHTimerObj *tmr){


    CHTimerWheel * lp=(CHTimerWheel *)userdata;
    UNSAFE_CONTAINER_OF_PUSH
    CMyTestObject *lpobj=(CMyTestObject *)((uint8_t*)tmr-offsetof (CMyTestObject,m_timer));
    UNSAFE_CONTAINER_OF_POP
    printf(" [event %d ]",lpobj->m_id);
    lp->timer_start(tmr,2);
}

TEST_F(gt_r_timer, timer6) {

    CHTimerWheel timer;

    CMyTestObject tmr1;
    tmr1.m_id=12;


    EXPECT_EQ( timer.Create(8,3),RC_HTW_OK);
    timer.timer_start(&tmr1.m_timer,3);

    int i;
    for (i=0; i<20; i++) {
        printf(" tick %d :",i);
        timer.on_tick((void *)&timer,my_test_on_tick_cb);
        printf(" \n");
    }

    EXPECT_EQ( timer.Delete(),RC_HTW_OK);
}

void my_test_on_tick_cb7(void *userdata,CHTimerObj *tmr){


    CHTimerWheel * lp=(CHTimerWheel *)userdata;
    UNSAFE_CONTAINER_OF_PUSH
    CMyTestObject *lpobj=(CMyTestObject *)((uint8_t*)tmr-offsetof (CMyTestObject,m_timer));
    UNSAFE_CONTAINER_OF_POP
    printf(" [event %d ]",lpobj->m_id);
    lp->timer_start(tmr,9);
}

void my_test_on_tick_cb_free(void *userdata,CHTimerObj *tmr){


    //CHTimerWheel * lp=(CHTimerWheel *)userdata;
    UNSAFE_CONTAINER_OF_PUSH
    CMyTestObject *lpobj=(CMyTestObject *)((uint8_t*)tmr-offsetof (CMyTestObject,m_timer));
    UNSAFE_CONTAINER_OF_POP
    printf(" [event %d ]",lpobj->m_id);
    delete lpobj;
}


TEST_F(gt_r_timer, timer7) {

    CHTimerWheel timer;

    CMyTestObject tmr1;
    tmr1.m_id=12;


    EXPECT_EQ( timer.Create(4,4),RC_HTW_OK);
    timer.timer_start(&tmr1.m_timer,80);

    int i;
    for (i=0; i<150; i++) {
        //printf(" tick %d :",i);
        timer.on_tick((void *)&timer,my_test_on_tick_cb7);
        //printf(" \n");
    }

    EXPECT_EQ( timer.Delete(),RC_HTW_OK);
}



class CHTimerWheelTest1Cfg {
public:
    uint32_t m_wheel_size;
    uint32_t m_num_wheels;
    uint32_t m_start_tick;
    uint32_t m_restart_tick;
    uint32_t m_total_ticks;
    bool     m_verbose;
    bool     m_dont_assert;
};

class CHTimerWheelBase {
public:
    virtual void on_tick(CMyTestObject *lpobj)=0;
};


class CHTimerWheelTest1 : public CHTimerWheelBase {

public:
    bool Create(CHTimerWheelTest1Cfg & cfg);
    void Delete();
    void start_test();
    virtual void on_tick(CMyTestObject *lpobj);

private:
    CHTimerWheelTest1Cfg  m_cfg;
    CHTimerWheel          m_timer;
    CMyTestObject         m_event;
    uint32_t              m_ticks;
    uint32_t              m_total_ticks;
    uint32_t              m_expected_total_ticks;

    uint32_t              m_expect_tick;
};

void my_test_on_tick_cb8(void *userdata,CHTimerObj *tmr){
    CHTimerWheelBase * lp=(CHTimerWheelBase *)userdata;
    UNSAFE_CONTAINER_OF_PUSH
    CMyTestObject *lpobj=(CMyTestObject *)((uint8_t*)tmr-offsetof (CMyTestObject,m_timer));
    UNSAFE_CONTAINER_OF_POP
    lp->on_tick(lpobj);
}


void CHTimerWheelTest1::on_tick(CMyTestObject *lpobj){
    assert(lpobj->m_id==17);
    m_total_ticks++;
    if (m_cfg.m_verbose) {
        printf(" [event %d ]",lpobj->m_id);
    }
    if (!m_cfg.m_dont_assert){
        assert( m_ticks == m_expect_tick);
    }
    m_timer.timer_start(&lpobj->m_timer,m_cfg.m_restart_tick);
    m_expect_tick+=m_cfg.m_restart_tick;
}


void CHTimerWheelTest1::start_test(){

    if (m_cfg.m_verbose) {
        printf(" test start %d,restart: %d \n",m_cfg.m_start_tick,m_cfg.m_restart_tick);
    }
    int i;
    m_expected_total_ticks=0;
    uint32_t cnt=m_cfg.m_start_tick;
    for (i=0; i<m_cfg.m_total_ticks; i++) {
        if (i==cnt) {
            m_expected_total_ticks++;
            cnt+=m_cfg.m_restart_tick;
        }
    }
    
    m_total_ticks=0;
    m_event.m_id=17;
    m_timer.timer_start(&m_event.m_timer,m_cfg.m_start_tick);

    m_ticks=0;
    m_expect_tick= m_cfg.m_start_tick;

    for (i=0; i<m_cfg.m_total_ticks; i++) {
        if (m_cfg.m_verbose) {
          printf(" tick %d :",i);
        }
        m_ticks=i;
        m_timer.on_tick((void *)this,my_test_on_tick_cb8);
        if (m_cfg.m_verbose) {
          printf(" \n");
        }
    }
    if (m_cfg.m_verbose) {
       printf(" %d == %d \n",m_expected_total_ticks,m_total_ticks);
    }
    if (!m_cfg.m_dont_assert){
      assert(m_expected_total_ticks ==m_total_ticks);
    }
}


bool CHTimerWheelTest1::Create(CHTimerWheelTest1Cfg & cfg){
    m_cfg = cfg;
    assert(m_timer.Create(m_cfg.m_wheel_size,m_cfg.m_num_wheels)==RC_HTW_OK);
    m_ticks=0;
    return (true);
}

void CHTimerWheelTest1::Delete(){
    assert(m_timer.Delete()==RC_HTW_OK);
}


TEST_F(gt_r_timer, timer8) {

    CHTimerWheelTest1 test;


    CHTimerWheelTest1Cfg  cfg ={
        .m_wheel_size   = 4,
        .m_num_wheels   = 4,
        .m_start_tick   = 2,
        .m_restart_tick = 2,
        .m_total_ticks  =100,
        .m_verbose=0
    };
    test.Create(cfg);
    test.start_test();
    test.Delete();
}

TEST_F(gt_r_timer, timer9) {

    CHTimerWheelTest1 test;


    CHTimerWheelTest1Cfg  cfg ={
        .m_wheel_size   = 4,
        .m_num_wheels   = 4,
        .m_start_tick   = 4,
        .m_restart_tick = 2,
        .m_total_ticks  =20
    };
    test.Create(cfg);
    test.start_test();
    test.Delete();
}

TEST_F(gt_r_timer, timer10) {

    CHTimerWheelTest1 test;


    CHTimerWheelTest1Cfg  cfg ={
        .m_wheel_size   = 4,
        .m_num_wheels   = 4,
        .m_start_tick   = 80,
        .m_restart_tick = 9,
        .m_total_ticks  =100
    };
    test.Create(cfg);
    test.start_test();
    test.Delete();
}


TEST_F(gt_r_timer, timer11) {

    int i,j;

    for (i=0; i<100; i++) {
        for (j=1; j<100; j++) {
            CHTimerWheelTest1 test;
            CHTimerWheelTest1Cfg  cfg ={
                .m_wheel_size   = 4,
                .m_num_wheels   = 4,
                .m_start_tick   = (uint32_t)i,
                .m_restart_tick = (uint32_t)j,
                .m_total_ticks  = 0
            };
            cfg.m_total_ticks= (uint32_t)(i*2+j*10);
            test.Create(cfg);
            test.start_test();
            test.Delete();
        }
    }
}

TEST_F(gt_r_timer, timer12) {

    int i;

    for (i=0; i<100; i++) {
            CHTimerWheelTest1 test;
            CHTimerWheelTest1Cfg  cfg ={
                .m_wheel_size   = 4,
                .m_num_wheels   = 4,
                .m_start_tick   = (uint32_t)i,
                .m_restart_tick = (uint32_t)i,
                .m_total_ticks  = 0
            };
            cfg.m_total_ticks= (uint32_t)(i*10);
            test.Create(cfg);
            test.start_test();
            test.Delete();
    }
}

TEST_F(gt_r_timer, timer_verylog) {


        CHTimerWheelTest1 test;
        CHTimerWheelTest1Cfg  cfg ={
            .m_wheel_size   = 4,
            .m_num_wheels   = 2,
            .m_start_tick   = 30,
            .m_restart_tick = 40,
            .m_total_ticks  = 100,
            //.m_verbose =true,
            //.m_dont_assert=true

        };
        test.Create(cfg);
        test.start_test();
        test.Delete();
}


/////////////////////////////////////////////////////////////////////////


class CHTimerWheelTest2Cfg {
public:
    uint32_t m_wheel_size;
    uint32_t m_num_wheels;
    uint32_t m_number_of_con_event;
    uint32_t m_total_ticks;
    bool     m_random;
    bool     m_verbose;
    bool     m_dont_check;
};

class CHTimerWheelTest2 : public CHTimerWheelBase {

public:
    bool Create(CHTimerWheelTest2Cfg & cfg);
    void Delete();
    void start_test();
    virtual void on_tick(CMyTestObject *lpobj);

private:
    CHTimerWheelTest2Cfg  m_cfg;
    CHTimerWheel          m_timer;
    uint32_t              m_ticks;
};

bool CHTimerWheelTest2::Create(CHTimerWheelTest2Cfg & cfg){
    m_cfg = cfg;
    assert(m_timer.Create(m_cfg.m_wheel_size,m_cfg.m_num_wheels)==RC_HTW_OK);
    m_ticks=0;
    return (true);
}

void CHTimerWheelTest2::Delete(){
    assert(m_timer.Delete()==RC_HTW_OK);
}


void CHTimerWheelTest2::start_test(){

    CMyTestObject *  m_events = new CMyTestObject[m_cfg.m_number_of_con_event]; 
    int i;
    for (i=0; i<m_cfg.m_number_of_con_event; i++) {
        CMyTestObject * lp=&m_events[i];
        lp->m_id=i+1;
        if (m_cfg.m_random) {
            lp->m_d_tick = ((rand() % m_cfg.m_number_of_con_event)+1);
            if (m_cfg.m_verbose) {
                printf(" flow %d : %d \n",i,lp->m_d_tick);
            }
        }else{
            lp->m_d_tick=i+1;
        }
        lp->m_t_tick=lp->m_d_tick;
        m_timer.timer_start(&lp->m_timer,lp->m_d_tick);
    }

    for (i=0; i<m_cfg.m_total_ticks; i++) {
        if (m_cfg.m_verbose) {
          printf(" tick %d :",i);
        }
        m_ticks=i;
        m_timer.on_tick((void *)this,my_test_on_tick_cb8);
        if (m_cfg.m_verbose) {
          printf(" \n");
        }
    }
    delete []m_events;
}


void CHTimerWheelTest2::on_tick(CMyTestObject *lp){

    if (!m_cfg.m_random) {
        assert(lp->m_id==lp->m_d_tick);
    }
    if (m_cfg.m_verbose) {
        printf(" [event %d ]",lp->m_id);
    }
    m_timer.timer_start(&lp->m_timer,lp->m_d_tick);
    if (!m_cfg.m_dont_check){
        assert(m_ticks == lp->m_t_tick);
    }
    lp->m_t_tick+=lp->m_d_tick;
}

TEST_F(gt_r_timer, timer13) {

        CHTimerWheelTest2 test;
        CHTimerWheelTest2Cfg  cfg ={
            .m_wheel_size   = 4,
            .m_num_wheels   = 4,
            .m_number_of_con_event   = 10,
            .m_total_ticks =1000,
            .m_random=false,
            .m_verbose =false
        };
        test.Create(cfg);
        test.start_test();
        test.Delete();
}

TEST_F(gt_r_timer, timer14) {

        CHTimerWheelTest2 test;
        CHTimerWheelTest2Cfg  cfg ={
            .m_wheel_size   = 8,
            .m_num_wheels   = 3,
            .m_number_of_con_event   = 10,
            .m_total_ticks =1000,
            .m_random=false,
            .m_verbose =false
        };
        test.Create(cfg);
        test.start_test();
        test.Delete();
}

TEST_F(gt_r_timer, timer15) {

        CHTimerWheelTest2 test;
        CHTimerWheelTest2Cfg  cfg ={
            .m_wheel_size   = 1024,
            .m_num_wheels   = 3,
            .m_number_of_con_event   = 100,
            .m_total_ticks =10000,
            .m_random=false,
            .m_verbose =false
        };
        test.Create(cfg);
        test.start_test();
        test.Delete();
}

TEST_F(gt_r_timer, timer16) {

        CHTimerWheelTest2 test;
        CHTimerWheelTest2Cfg  cfg ={
            .m_wheel_size   = 32,
            .m_num_wheels   = 4,
            .m_number_of_con_event   = 111,
            .m_total_ticks =20000,
            .m_random=true,
            .m_verbose =false,
            .m_dont_check=false
        };
        test.Create(cfg);
        test.start_test();
        test.Delete();
}


/* test free of iterator */
TEST_F(gt_r_timer, timer17) {

    CHTimerWheel  timer;

    CMyTestObject * tmr1;
    tmr1 = new CMyTestObject();
    tmr1->m_id=12;

    CMyTestObject * tmr2;
    tmr2 = new CMyTestObject();
    tmr2->m_id=13;

    CMyTestObject * tmr3;
    tmr3 = new CMyTestObject();
    tmr3->m_id=14;


    EXPECT_EQ( timer.Create(4,4),RC_HTW_OK);
    timer.timer_start(&tmr1->m_timer,80);
    timer.timer_start(&tmr2->m_timer,1);
    timer.timer_start(&tmr3->m_timer,0);

    timer.detach_all((void *)&timer,my_test_on_tick_cb_free);
    EXPECT_EQ( timer.Delete(),RC_HTW_OK);
}


TEST_F(gt_r_timer, timer18) {

    RC_HTW_t res[]={
        RC_HTW_OK,
        RC_HTW_ERR_NO_RESOURCES,
        RC_HTW_ERR_TIMER_IS_ON,
        RC_HTW_ERR_NO_LOG2,
        RC_HTW_ERR_MAX_WHEELS,
        RC_HTW_ERR_NOT_ENOUGH_BITS
    };

    int i;
    for (i=0; i<sizeof(res)/sizeof(RC_HTW_t); i++) {
        CHTimerWheelErrorStr err(res[i]);
        printf(" %-30s  - %s \n",err.get_str(),err.get_help_str());
    }
}



/////////////////////////////////////////////////////////////////
/* test for NA class */

class CNATimerWheelTest1Cfg {
public:
    uint32_t m_wheel_size;
    uint32_t m_level1_div;
    uint32_t m_start_tick;
    uint32_t m_restart_tick;
    uint32_t m_total_ticks;
    int     m_verbose;
    bool     m_dont_assert;
};


class CNATimerWheelTest1 : public CHTimerWheelBase {

public:
    bool Create(CNATimerWheelTest1Cfg & cfg);
    void Delete();
    void start_test();
    virtual void on_tick(CMyTestObject *lpobj);

private:
    CNATimerWheelTest1Cfg m_cfg;
    CNATimerWheel         m_timer;
    CMyTestObject         m_event;
    uint32_t              m_ticks;
    uint32_t              m_total_ticks;
    uint32_t              m_expected_total_ticks;
    uint32_t              m_div_err;

    uint32_t              m_expect_tick;
    double                m_max_err;
};

void my_test_on_tick_cb18(void *userdata,CHTimerObj *tmr){
    CHTimerWheelBase * lp=(CHTimerWheelBase *)userdata;
    UNSAFE_CONTAINER_OF_PUSH
    CMyTestObject *lpobj=(CMyTestObject *)((uint8_t*)tmr-offsetof (CMyTestObject,m_timer));
    UNSAFE_CONTAINER_OF_POP
    lp->on_tick(lpobj);
}


void CNATimerWheelTest1::on_tick(CMyTestObject *lpobj){
    assert(lpobj->m_id==17);
    m_total_ticks++;
    if (m_cfg.m_verbose) {
        printf(" [event(%d)-%d]",lpobj->m_timer.m_wheel,lpobj->m_id);
    }
    if (!m_cfg.m_dont_assert){
        uint32_t expect_min=m_expect_tick;
        if (expect_min>m_div_err) {
            expect_min-=m_div_err*2;
        }
        double pre=std::abs(100.0-100.0*(double)m_ticks/(double)m_expect_tick);
        if (pre>m_max_err){
            m_max_err=pre;
        }
        if (pre>(200.0/(double)m_div_err)) {
            printf(" =====>tick:%d expect [%d -%d] %f \n",m_ticks,expect_min,m_expect_tick+(m_div_err*2),pre);
        }
    }
    m_timer.timer_start(&lpobj->m_timer,m_cfg.m_restart_tick);
    m_expect_tick+=m_cfg.m_restart_tick;
}


void CNATimerWheelTest1::start_test(){

    if (m_cfg.m_verbose) {
        printf(" test start %d,restart: %d \n",m_cfg.m_start_tick,m_cfg.m_restart_tick);
    }
    int i;
    m_expected_total_ticks=0;
    uint32_t cnt=m_cfg.m_start_tick;
    for (i=0; i<m_cfg.m_total_ticks; i++) {
        if (i==cnt) {
            m_expected_total_ticks++;
            cnt+=m_cfg.m_restart_tick;
        }
    }

    m_div_err =m_cfg.m_wheel_size/m_cfg.m_level1_div;
    m_total_ticks=0;
    m_event.m_id=17;
    m_timer.timer_start(&m_event.m_timer,m_cfg.m_start_tick);

    m_ticks=0;
    m_expect_tick= m_cfg.m_start_tick;

    for (i=0; i<m_cfg.m_total_ticks; i++) {
        if (m_cfg.m_verbose) {
          printf(" tick %d :",i);
        }
        m_ticks=i;
        m_timer.on_tick_level0((void *)this,my_test_on_tick_cb18);
        /* level 2 */
        if ((i>=m_div_err) && (i%m_div_err==0)) {
            int cnt_rerty=0;
            while (true){
                if (m_cfg.m_verbose>1) {
                  printf("\n level1 - try %d \n",cnt_rerty);
                }

                na_htw_state_num_t state;
                state = m_timer.on_tick_level1((void *)this,my_test_on_tick_cb18);
                if (m_cfg.m_verbose>1) {
                  printf("\n state - %lu \n",(ulong)state);
                }

                if ( state !=TW_NEXT_BATCH){
                    break;
                }
                cnt_rerty++;
            }
            if (m_cfg.m_verbose>1) {
               printf("\n level1 - stop %d \n",cnt_rerty);
            }
        }
        if (m_cfg.m_verbose) {
          printf(" \n");
        }
    }
    if (m_cfg.m_verbose) {
       printf(" %d == %d \n",m_expected_total_ticks,m_total_ticks);
    }
    if (!m_cfg.m_dont_assert){
      //assert( (m_expected_total_ticks==m_total_ticks) || ((m_expected_total_ticks+1) ==m_total_ticks) );
    } 
}


bool CNATimerWheelTest1::Create(CNATimerWheelTest1Cfg & cfg){
    m_cfg = cfg;
    m_max_err=0.0;
    assert(m_timer.Create(m_cfg.m_wheel_size,m_cfg.m_level1_div)==RC_HTW_OK);
    m_ticks=0;
    return (true);
}

void CNATimerWheelTest1::Delete(){
    //printf (" %f \n",m_max_err);
    assert(m_timer.Delete()==RC_HTW_OK);
}


TEST_F(gt_r_timer, timer20) {

    CNATimerWheelTest1 test;

    CNATimerWheelTest1Cfg  cfg ={
        .m_wheel_size    = 32,
        .m_level1_div    = 4,
        .m_start_tick    = 2,
        .m_restart_tick  = 2,
        .m_total_ticks   = 1024,
        .m_verbose=0
    };
    test.Create(cfg);
    test.start_test();
    test.Delete();
}

TEST_F(gt_r_timer, timer21) {

    CNATimerWheelTest1 test;

    CNATimerWheelTest1Cfg  cfg ={
        .m_wheel_size    = 32,
        .m_level1_div    = 4,
        .m_start_tick    = 2,
        .m_restart_tick  = 34,
        .m_total_ticks   = 100,
        .m_verbose=0
    };
    test.Create(cfg);
    test.start_test();
    test.Delete();
}


TEST_F(gt_r_timer, timer22) {

    CNATimerWheelTest1 test;

    CNATimerWheelTest1Cfg  cfg ={
        .m_wheel_size    = 32,
        .m_level1_div    = 4,
        .m_start_tick    = 2,
        .m_restart_tick  = 55,
        .m_total_ticks   = 1000,
        .m_verbose=0,
        .m_dont_assert =0
    };
    test.Create(cfg);
    test.start_test();
    test.Delete();
}

TEST_F(gt_r_timer, timer23) {

    int i,j;

    for (i=0; i<100; i++) {
        for (j=1; j<100; j++) {
            CNATimerWheelTest1 test;
            CNATimerWheelTest1Cfg  cfg ={
                .m_wheel_size    = 32,
                .m_level1_div    = 4,
                .m_start_tick    = (uint32_t)i,
                .m_restart_tick  = (uint32_t)j,
                .m_total_ticks   = 1000,
                .m_verbose=0,
                .m_dont_assert =0
            };

            cfg.m_total_ticks= (uint32_t)(i*2+j*10);
            test.Create(cfg);
            test.start_test();
            test.Delete();
        }
    }
}



#if 0
// too long, skip for now 
TEST_F(gt_r_timer, timer24) {

    int i,j;

    for (i=0; i<2048; i++) {
        printf(" %d \n",i);
        for (j=1024; j<2048; j=j+7) {
            CNATimerWheelTest1 test;
            CNATimerWheelTest1Cfg  cfg ={
                .m_wheel_size    = 1024,
                .m_level1_div    = 32,
                .m_start_tick    = (uint32_t)i,
                .m_restart_tick  = (uint32_t)j,
                .m_total_ticks   = 3000,
                .m_verbose=0,
                .m_dont_assert =0
            };

            cfg.m_total_ticks= (uint32_t)(i*2+j*10);
            test.Create(cfg);
            test.start_test();
            test.Delete();
        }
    }
}
#endif

/* very long flow, need to restart it */
TEST_F(gt_r_timer, timer25) {


        CNATimerWheelTest1 test;

        CNATimerWheelTest1Cfg  cfg ={
            .m_wheel_size    = 32,
            .m_level1_div    = 4,
            .m_start_tick    = 2,
            .m_restart_tick  = 512,
            .m_total_ticks   = 1000,
            .m_verbose=0,
            .m_dont_assert =0
        };

        test.Create(cfg);
        test.start_test();
        test.Delete();
}



////////////////////////////////////////////////////////

class CNATimerWheelTest2Cfg {
public:
    uint32_t m_wheel_size;
    uint32_t m_level1_div;
    uint32_t m_number_of_con_event;
    uint32_t m_total_ticks;
    bool     m_random;
    bool     m_burst;
    int      m_verbose;
    bool     m_dont_check;
};

class CNATimerWheelTest2 : public CHTimerWheelBase {

public:
    bool Create(CNATimerWheelTest2Cfg & cfg);
    void Delete();
    void start_test();
    virtual void on_tick(CMyTestObject *lpobj);

private:
    CNATimerWheelTest2Cfg  m_cfg;
    CNATimerWheel          m_timer;
    uint32_t              m_ticks;
    uint32_t              m_div_err;
};

bool CNATimerWheelTest2::Create(CNATimerWheelTest2Cfg & cfg){
    m_cfg = cfg;
    assert(m_timer.Create(m_cfg.m_wheel_size,m_cfg.m_level1_div)==RC_HTW_OK);
    m_ticks=0;
    return (true);
}

void CNATimerWheelTest2::Delete(){
    assert(m_timer.Delete()==RC_HTW_OK);
}


void CNATimerWheelTest2::start_test(){

    CMyTestObject *  m_events = new CMyTestObject[m_cfg.m_number_of_con_event]; 
    int i;
    for (i=0; i<m_cfg.m_number_of_con_event; i++) {
        CMyTestObject * lp=&m_events[i];
        lp->m_id=i+1;
        if (m_cfg.m_random) {
            lp->m_d_tick = ((rand() % m_cfg.m_number_of_con_event)+1);
            if (m_cfg.m_verbose) {
                printf(" flow %d : %d \n",i,lp->m_d_tick);
            }
        }else{
            if (m_cfg.m_burst){
                lp->m_d_tick = m_cfg.m_wheel_size*2; /* all in the same bucket */
            }else{
                lp->m_d_tick=i+1;
            }
        }
        lp->m_t_tick=lp->m_d_tick;
        m_timer.timer_start(&lp->m_timer,lp->m_d_tick);
    }

    m_div_err =m_cfg.m_wheel_size/m_cfg.m_level1_div;

    for (i=0; i<m_cfg.m_total_ticks; i++) {
        if (m_cfg.m_verbose) {
          printf(" tick %d :",i);
        }
        m_ticks=i;
        m_timer.on_tick_level0((void *)this,my_test_on_tick_cb18);

        if ((i>=m_div_err) && (i%m_div_err==0)) {
            int cnt_rerty=0;
            while (true){
                if (m_cfg.m_verbose>1) {
                  printf("\n level1 - try %d \n",cnt_rerty);
                }

                na_htw_state_num_t state;
                state = m_timer.on_tick_level1((void *)this,my_test_on_tick_cb18);
                if (m_cfg.m_verbose>1) {
                  printf("\n state - %lu \n",(ulong)state);
                }

                if ( state !=TW_NEXT_BATCH){
                    break;
                }

                cnt_rerty++;
            }
            if (m_cfg.m_verbose>1) {
               printf("\n level1 - stop %d \n",cnt_rerty);
            }
        }


        if (m_cfg.m_verbose) {
          printf(" \n");
        }
    }
    delete []m_events;
}


void CNATimerWheelTest2::on_tick(CMyTestObject *lp){

    if (!m_cfg.m_random && !m_cfg.m_burst) {
        assert(lp->m_id==lp->m_d_tick);
    }
    if (m_cfg.m_verbose) {
        printf(" [event %d ]",lp->m_id);
    }
    m_timer.timer_start(&lp->m_timer,lp->m_d_tick);
    if (!m_cfg.m_dont_check){
        double pre=std::abs(100.0-100.0*(double)m_ticks/(double)lp->m_t_tick);
        if (pre>(200.0/(double)m_div_err)) {
            printf(" =====>tick:%d  %f \n",m_ticks,pre);
            assert(0);
        }
    }
    lp->m_t_tick+=lp->m_d_tick;
}


TEST_F(gt_r_timer, timer30) {

        CNATimerWheelTest2 test;
        CNATimerWheelTest2Cfg  cfg ={
            .m_wheel_size    = 32,
            .m_level1_div    = 4,
            .m_number_of_con_event   = 100,
            .m_total_ticks =1000,
            .m_random=false,
            .m_burst=false,
            .m_verbose =false
        };
        test.Create(cfg);
        test.start_test();
        test.Delete();
}

TEST_F(gt_r_timer, timer31) {

        CNATimerWheelTest2 test;
        CNATimerWheelTest2Cfg  cfg ={
            .m_wheel_size    = 32,
            .m_level1_div    = 4,
            .m_number_of_con_event   = 500,
            .m_total_ticks =5000,
            .m_random=true,
            .m_burst=false,
            .m_verbose =false
        };
        test.Create(cfg);
        test.start_test();
        test.Delete();
}

TEST_F(gt_r_timer, timer32) {

        CNATimerWheelTest2 test;
        CNATimerWheelTest2Cfg  cfg ={
            .m_wheel_size    = 32,
            .m_level1_div    = 4,
            .m_number_of_con_event   = 500,
            .m_total_ticks =100,
            .m_random=false,
            .m_burst=true,
            .m_verbose =0
        };
        test.Create(cfg);
        test.start_test();
        test.Delete();
}

