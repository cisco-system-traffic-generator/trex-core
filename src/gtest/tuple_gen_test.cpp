/*
 Wenxian Li 
 
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
#include <cstdlib>
#include <ctime>
#include "../bp_sim.h"
#include <common/gtest.h>
#include <common/basic_utils.h>

/* TEST case for CClientInfo*/

class CClientInfo : public CSimpleClientInfo<CIpInfo> {
public:
    CClientInfo() :  CSimpleClientInfo<CIpInfo>(0) {

    }
};

class CClientInfoL : public CSimpleClientInfo<CIpInfoL> {
public:
    CClientInfoL() :  CSimpleClientInfo<CIpInfoL>(0) {

    }
};

static ClientCfgDB g_dummy;

class CClientInfoUT {
public:
    CClientInfoUT(CClientInfo *a) {
        obj = a;
    }

    uint16_t m_head_port_get() {
        return obj->m_head_port;
    }

    void m_head_port_set(uint16_t head) {
        obj->m_head_port = head;
    }

    bool is_port_legal(uint16_t port) {
        return obj->is_port_legal(port);
    }
    bool is_port_free(uint16_t port) {
        return obj->is_port_available(port);
    }
    void m_bitmap_port_set_bit(uint16_t port) {
        obj->m_bitmap_port[port] = 1;
    }
    void m_bitmap_port_reset_bit(uint16_t port) {
        obj->m_bitmap_port[port] = 0;
    }
    uint8_t m_bitmap_port_get_bit(uint16_t port) {
        return obj->m_bitmap_port[port];
    }
    void get_next_free_port_by_bit() {
        obj->get_next_free_port_by_bit();
    }

    CClientInfo *obj;
};

TEST(CClientInfoTest, Constructors) {
    CClientInfo c;
    CClientInfoUT client(&c);
    EXPECT_EQ(MIN_PORT, client.m_head_port_get());

    CClientInfo c2;
    CClientInfoUT client2(&c2);
    EXPECT_EQ(MIN_PORT, client2.m_head_port_get());
}

TEST(CClientInfoTest, is_port_legal) {
    CClientInfo c;
    CClientInfoUT client(&c);
    EXPECT_TRUE(client.is_port_legal(MIN_PORT));
    EXPECT_FALSE(client.is_port_legal(MIN_PORT-1));
    EXPECT_TRUE(client.is_port_legal(MAX_PORT-1));
    EXPECT_FALSE(client.is_port_legal(MAX_PORT));
    EXPECT_FALSE(client.is_port_legal(MAX_PORT+1));
}

TEST(CClientInfoTest, is_port_free) {
    CClientInfo c;
    CClientInfoUT client(&c);
    client.m_bitmap_port_set_bit(2000);
    EXPECT_FALSE(client.is_port_free(2000));
    client.m_bitmap_port_reset_bit(2000);
    EXPECT_TRUE(client.is_port_free(2000));
}



TEST(CClientInfoTest,get_next_free_port_by_bit) {
    CClientInfo c;
    CClientInfoUT client(&c);
    client.m_head_port_set(200);
    client.get_next_free_port_by_bit();
    EXPECT_EQ(MIN_PORT, client.m_head_port_get());
    for(int idx=1024;idx<2000;idx++) {
        client.m_bitmap_port_set_bit(idx);
    }
    client.get_next_free_port_by_bit();
    EXPECT_EQ(1044, client.m_head_port_get());
}

TEST(CClientInfoTest, get_new_free_port) {
    CClientInfo c;
    CClientInfoUT client(&c);

    EXPECT_EQ(1024, client.obj->get_new_free_port());
    EXPECT_EQ(PORT_IN_USE, client.m_bitmap_port_get_bit(1024));

    client.m_bitmap_port_reset_bit(1024);
    EXPECT_EQ(PORT_FREE, client.m_bitmap_port_get_bit(1024));
    client.m_head_port_set(MAX_PORT-1);
    client.m_bitmap_port_set_bit(MAX_PORT-1);

    EXPECT_EQ(1024, client.obj->get_new_free_port());
    EXPECT_EQ(PORT_IN_USE, client.m_bitmap_port_get_bit(1024));
    client.m_head_port_set(1024);
    EXPECT_EQ(1025, client.obj->get_new_free_port());
    EXPECT_EQ(PORT_IN_USE, client.m_bitmap_port_get_bit(1025));

    for(int i=1024;i<1200;i++) {
        client.m_bitmap_port_set_bit(i);
    }
    client.m_head_port_set(1024);
    EXPECT_EQ(ILLEGAL_PORT, client.obj->get_new_free_port());

}


TEST(CClientInfoTest, return_port) {
    CClientInfo c;
    CClientInfoUT client(&c);
    client.m_bitmap_port_set_bit(2000);
    client.obj->return_port(2000);
    EXPECT_EQ(PORT_FREE, client.m_bitmap_port_get_bit(2000));
}

TEST(CClientInfoLTest, get_new_free_port) {
    CClientInfoL c;
    for(int i=0;i<10;i++) {
        EXPECT_EQ(1024+i, c.get_new_free_port());
    }
    c.return_all_ports();
    for(int i=0;i<10;i++) {
        EXPECT_EQ(1024+i, c.get_new_free_port());
    }
}



/* UIT of CClientPool, using CClientInfoL */
TEST(tuple_gen,clientPoolL) {
    CClientPool gen;
    gen.Create(cdSEQ_DIST, 
               0x10000001,  0x10000f01, 64000, g_dummy, 
               0,0);
    CTupleBase result;
    uint32_t result_src;
    uint16_t result_port;

    for(int i=0;i<10;i++) {
        gen.GenerateTuple(result);

        result_src = result.getClient();
        result_port = result.getClientPort();
        printf(" C:%x P:%d %d\n",result.getClient(),result.getClientPort(),result_port);

        EXPECT_EQ(result_src, (uint32_t)(0x10000001+i));
    }

    gen.Delete();
//    EXPECT_EQ((size_t)0, gen.m_clients.size());
}

/* UIT of CClientPool, using CClientInfo */
TEST(tuple_gen,clientPool) {
    CClientPool gen;
    gen.Create(cdSEQ_DIST, 
               0x10000001,  0x10000021, 64000000, g_dummy,
               0,0);
    CTupleBase result;
    uint32_t result_src;
    uint16_t result_port;

    for(int i=0;i<10;i++) {
        gen.GenerateTuple(result);
        result_src = result.getClient();
        result_port = result.getClientPort();
        printf(" C:%x P:%d (%d) \n",result.getClient(),result.getClientPort(),result_port);
        EXPECT_EQ(result_src, (uint32_t)(0x10000001+i));
    }

    gen.Delete();
//    EXPECT_EQ((size_t)0, gen.m_clients.size());
}

/* UIT of CServerPool */
TEST(tuple_gen,serverPool) {
    CServerPool gen;
    gen.Create(cdSEQ_DIST, 
               0x30000001,  0x30000ff1, 6400000);
    CTupleBase result;
    uint32_t result_dest;

    for(int i=0;i<10;i++) {
        gen.GenerateTuple(result);
        printf(" S:%x \n",result.getServer());

        result_dest = result.getServer();
        EXPECT_EQ(result_dest, (uint32_t) (((0x30000001+i)) ) );
    }

    gen.Delete();

    gen.Create(cdSEQ_DIST, 
               0x30000001,  0x30000003, 64000000);

    for(int i=0;i<10;i++) {
        gen.GenerateTuple(result);
        printf(" S:%x \n",result.getServer());

        result_dest = result.getServer();
        EXPECT_EQ(result_dest, (uint32_t) (((0x30000001+i%3)) ) );
    }

    gen.Delete();
    //    EXPECT_EQ((size_t)0, gen.m_clients.size());
}

TEST(tuple_gen,servePoolSim) {
    CServerPoolSimple gen;
    gen.Create(cdSEQ_DIST, 
               0x30000001,  0x40000001, 640000);
    CTupleBase result;
    uint32_t result_dest;

    for(int i=0;i<10;i++) {
        gen.GenerateTuple(result);
        printf(" S:%x \n",result.getServer());

        result_dest = result.getServer();
        EXPECT_EQ(result_dest, (uint32_t) (((0x30000001+i)) ) );
    }

    gen.Delete();

    gen.Create(cdSEQ_DIST, 
               0x30000001,  0x30000003, 64000000);

    for(int i=0;i<10;i++) {
        gen.GenerateTuple(result);
        printf(" S:%x \n",result.getServer());

        result_dest = result.getServer();
        EXPECT_EQ(result_dest, (uint32_t) (((0x30000001+i%3)) ) );
    }

    gen.Delete();
    //    EXPECT_EQ((size_t)0, gen.m_clients.size());
}




TEST(tuple_gen,GenerateTuple2) {
    CClientPool c_gen;
    CClientPool c_gen_2;
    c_gen.Create(cdSEQ_DIST, 
               0x10000001,  0x1000000f, 64000*4, g_dummy,
               0,0);
    CServerPool s_gen;
    CServerPool s_gen_2;
    s_gen.Create(cdSEQ_DIST, 
               0x30000001,  0x30000ff1, 640000);
    CTupleBase result;

    uint32_t result_src;
    uint32_t result_dest;
    //uint16_t result_port;

    for(int i=0;i<200;i++) {
        c_gen.GenerateTuple(result);
        s_gen.GenerateTuple(result);
      //  gen.Dump(stdout);
      //  fprintf(stdout, "i:%d\n",i);
        result_src = result.getClient();
        result_dest = result.getServer();
        //result_port = result.getClientPort();
        EXPECT_EQ(result_src, (uint32_t)(0x10000001+i%15));
        EXPECT_EQ(result_dest, (uint32_t)((0x30000001+i) ) );
        //EXPECT_EQ(result_port, 1024+i/15);
    }

    s_gen.Delete();
    c_gen.Delete();
//    EXPECT_EQ((size_t)0, gen.m_clients.size());
    c_gen.Create(cdSEQ_DIST, 
               0x10000001,  0x1000000f, 64000*400, g_dummy, 
               0,0);
    s_gen.Create(cdSEQ_DIST, 
               0x30000001,  0x30000001, 640000);
    for(int i=0;i<200;i++) {
        s_gen.GenerateTuple(result);
        c_gen.GenerateTuple(result);
    //    gen.Dump(stdout);
       // fprintf(stdout, "i:%d\n",i);
        result_src = result.getClient();
        result_dest = result.getServer();
        EXPECT_EQ(result_src, (uint32_t)(0x10000001+i%15));
        EXPECT_EQ(result_dest, (uint32_t) (((0x30000001)) ) );
    }

    s_gen.Delete();
    c_gen.Delete();


}


TEST(tuple_gen,split1) {
    CIpPortion  portion;

    CTupleGenPoolYaml fi;
    fi.m_ip_start =0x10000000;
    fi.m_ip_end   =0x100000ff;

    fi.m_dual_interface_mask =0x01000000;

    split_ips(0,
                  1, 
                  0,
                  fi,
                  portion);
    EXPECT_EQ(portion.m_ip_start, (uint32_t)(0x10000000));
    EXPECT_EQ(portion.m_ip_end,   (uint32_t)(0x100000ff ));
    printf(" %x %x \n",portion.m_ip_start,portion.m_ip_end);

    split_ips(2,
                  4, 
                  1,
                  fi,
                  portion);

     EXPECT_EQ(portion.m_ip_start, (uint32_t)(0x11000080));
     EXPECT_EQ(portion.m_ip_end, (uint32_t)(0x110000bf ));
     printf(" %x %x \n",portion.m_ip_start,portion.m_ip_end);
}

TEST(tuple_gen,split2) {
    CIpPortion  portion;

    CTupleGenPoolYaml fi;

    fi.m_ip_start =0x20000000;
    fi.m_ip_end   =0x200001ff;

    fi.m_dual_interface_mask =0x01000000;

    int i;
    for (i=0; i<8; i++) {
        split_ips(i,
                      8, 
                      (i&1),
                      fi,
                      portion);


        if ( (i&1) ) {
            EXPECT_EQ(portion.m_ip_start  , (uint32_t)(0x21000000)+ (0x40*i) );
            EXPECT_EQ(portion.m_ip_end    , (uint32_t)(0x21000000)+(0x40*i+0x40-1) );
        }else{
            EXPECT_EQ(portion.m_ip_start  , (uint32_t)(0x20000000) + (0x40*i) );
            EXPECT_EQ(portion.m_ip_end    , (uint32_t)(0x20000000) + (0x40*i+0x40-1) );
        }
        printf(" %x %x \n",portion.m_ip_start,portion.m_ip_end);
    }
}

TEST(tuple_gen,template1) {
    CTupleGeneratorSmart gen;
    gen.Create(1, 1); 
    gen.add_client_pool(cdSEQ_DIST,0x10000001,0x1000000f,64000, g_dummy, 0, 0);
    gen.add_server_pool(cdSEQ_DIST,0x30000001,0x40000001,64000,false);
    CTupleTemplateGeneratorSmart template_1;
    template_1.Create(&gen,0,0);
    template_1.SetSingleServer(true,0x12121212,0,0);
    CTupleBase result;


    int i;
    for (i=0; i<10; i++) {
        template_1.GenerateTuple(result);
        uint32_t result_src = result.getClient();
        uint32_t result_dest = result.getServer();
        //printf(" %x %x %x \n",result_src,result_dest,result_port);
        EXPECT_EQ(result_src, (uint32_t)(0x10000001+i));
        EXPECT_EQ(result_dest, (uint32_t)(((0x12121212)) ));
    }

    template_1.Delete();
    gen.Delete();
}

TEST(tuple_gen,template2) {
    CTupleGeneratorSmart gen;
    gen.Create(1, 1);
    gen.add_client_pool(cdSEQ_DIST,0x10000001,0x1000000f,64000,g_dummy,0,0);
    gen.add_server_pool(cdSEQ_DIST,0x30000001,0x40000001,64000,false);
    CTupleTemplateGeneratorSmart template_1;
    template_1.Create(&gen,0,0);
    template_1.SetW(10);

    CTupleBase result;


    int i;
    for (i=0; i<20; i++) {
        template_1.GenerateTuple(result);
        uint32_t result_src = result.getClient();
        uint32_t result_dest = result.getServer();
        EXPECT_EQ(result_src, (uint32_t)(0x10000001+(i/10)));
        EXPECT_EQ(result_dest, (uint32_t)(((0x30000001+ (i/10) )) ));
    }

    template_1.Delete();
    gen.Delete();
}


TEST(tuple_gen,no_free) {
    CTupleGeneratorSmart gen;
    gen.Create(1, 1);
    gen.add_client_pool(cdSEQ_DIST,0x10000001,0x10000001,64000,g_dummy,0,0);
    gen.add_server_pool(cdSEQ_DIST,0x30000001,0x400000ff,64000,false);
    CTupleTemplateGeneratorSmart template_1;
    template_1.Create(&gen,0,0);

    CTupleBase result;


    int i;
    for (i=0; i<65557; i++) {
        template_1.GenerateTuple(result);
    }
    // should have error
    EXPECT_TRUE((gen.getErrorAllocationCounter()>0)?true:false);

    template_1.Delete();
    gen.Delete();
}

TEST(tuple_gen,try_to_free) {
    CTupleGeneratorSmart gen;
    gen.Create(1, 1); 
    gen.add_client_pool(cdSEQ_DIST,0x10000001,0x10000001,64000,g_dummy,0,0);
    gen.add_server_pool(cdSEQ_DIST,0x30000001,0x400000ff,64000,false);
    CTupleTemplateGeneratorSmart template_1;
    template_1.Create(&gen,0,0);

    CTupleBase result;


    int i;
    for (i=0; i<65557; i++) {
        template_1.GenerateTuple(result);
        uint16_t result_port = result.getClientPort();
        gen.FreePort(0,result.getClientId(),result_port);
    }
    // should have error
    EXPECT_FALSE((gen.getErrorAllocationCounter()>0)?true:false);

    template_1.Delete();
    gen.Delete();
}



/* tuple generator using CClientInfoL*/
TEST(tuple_gen_2,GenerateTuple) {
    CTupleGeneratorSmart gen;
    gen.Create(1, 1); 
    gen.add_client_pool(cdSEQ_DIST,0x10000001,0x10000f01,64000,g_dummy,0,0);
    gen.add_server_pool(cdSEQ_DIST,0x30000001,0x40000001,64000,false);
    CTupleTemplateGeneratorSmart template_1;
    template_1.Create(&gen,0,0);
    CTupleBase result;
    uint32_t result_src;
    uint32_t result_dest;

    for(int i=0;i<10;i++) {
        template_1.GenerateTuple(result);
        printf(" C:%x S:%x P:%d \n",result.getClient(),result.getServer(),result.getClientPort());

        result_src = result.getClient();
        result_dest = result.getServer();
        EXPECT_EQ(result_src, (uint32_t)(0x10000001+i));
        EXPECT_EQ(result_dest, (uint32_t) (((0x30000001+i)) ) );
    }

    gen.Delete();
//    EXPECT_EQ((size_t)0, gen.m_clients.size());
}

TEST(tuple_gen_2,GenerateTuple2) {
    CTupleGeneratorSmart gen;
    gen.Create(1, 1);
    gen.add_client_pool(cdSEQ_DIST,0x10000001,0x1000000f,64000,g_dummy,0,0);
    gen.add_server_pool(cdSEQ_DIST,0x30000001,0x40000001,64000,false);
    CTupleTemplateGeneratorSmart template_1;
    template_1.Create(&gen,0,0);
    CTupleBase result;
    uint32_t result_src;
    uint32_t result_dest;

    for(int i=0;i<200;i++) {
        template_1.GenerateTuple(result);
      //  gen.Dump(stdout);
      //  fprintf(stdout, "i:%d\n",i);
        result_src = result.getClient();
        result_dest = result.getServer();
        //result_port = result.getClientPort();
        EXPECT_EQ(result_src, (uint32_t)(0x10000001+i%15));
        EXPECT_EQ(result_dest, (uint32_t)((0x30000001+i) ) );
    }

    gen.Delete();
//    EXPECT_EQ((size_t)0, gen.m_clients.size());
    gen.Create(1, 1); 
    gen.add_client_pool(cdSEQ_DIST,0x10000001,0x1000000f,64000,g_dummy,0,0);
    gen.add_server_pool(cdSEQ_DIST,0x30000001,0x40000001,64000,false);
    template_1.Create(&gen,0,0);
    for(int i=0;i<200;i++) {
        template_1.GenerateTuple(result);
    //    gen.Dump(stdout);
     //   fprintf(stdout, "i:%d\n",i);
        result_src = result.getClient();
        result_dest = result.getServer();
        //result_port = result.getClientPort();
        EXPECT_EQ(result_src, (uint32_t)(0x10000001+i%15));
        EXPECT_EQ(result_dest, (uint32_t) (((0x30000001+i)) ) );
    }
}


TEST(tuple_gen_2,template1) {
    CTupleGeneratorSmart gen;
    gen.Create(1, 1); 
    gen.add_client_pool(cdSEQ_DIST,0x10000001,0x1000000f,64000,g_dummy,0,0);
    gen.add_server_pool(cdSEQ_DIST,0x30000001,0x40000001,64000,false);
    CTupleTemplateGeneratorSmart template_1;
    template_1.Create(&gen,0,0);
    template_1.SetSingleServer(true,0x12121212,0,0);
    CTupleBase result;


    int i;
    for (i=0; i<10; i++) {
        template_1.GenerateTuple(result);
        uint32_t result_src = result.getClient();
        uint32_t result_dest = result.getServer();
        //uint16_t result_port = result.getClientPort();
        EXPECT_EQ(result_src, (uint32_t)(0x10000001+i));
        EXPECT_EQ(result_dest, (uint32_t)(((0x12121212)) ));
    }

    template_1.Delete();
    gen.Delete();
}

TEST(tuple_gen_2,template2) {
    CTupleGeneratorSmart gen;
    gen.Create(1, 1);
    gen.add_client_pool(cdSEQ_DIST,0x10000001,0x1000000f,64000,g_dummy,0,0);
    gen.add_server_pool(cdSEQ_DIST,0x30000001,0x40000001,64000,false);
    CTupleTemplateGeneratorSmart template_1;
    template_1.Create(&gen,0,0);
    template_1.SetW(10);

    CTupleBase result;


    int i;
    for (i=0; i<20; i++) {
        template_1.GenerateTuple(result);
        uint32_t result_src = result.getClient();
        uint32_t result_dest = result.getServer();
        //uint16_t result_port = result.getClientPort();
        //printf(" %x %x %x \n",result_src,result_dest,result_port);
        EXPECT_EQ(result_src, (uint32_t)(0x10000001+(i/10)));
        EXPECT_EQ(result_dest, (uint32_t)(((0x30000001+ (i/10) )) ));
    }

    template_1.Delete();
    gen.Delete();
}


TEST(tuple_gen_yaml,yam_reader1) {

    CTupleGenYamlInfo  fi;

    try {
       std::ifstream fin((char *)"cap2/tuple_gen.yaml");
       YAML::Parser parser(fin);
       YAML::Node doc;

       parser.GetNextDocument(doc);
       for(unsigned i=0;i<doc.size();i++) {
          doc[i] >> fi;
          break;
       }
    } catch ( const std::exception& e ) {
        std::cout << e.what() << "\n";
        exit(-1);
    }
}

TEST(tuple_gen_yaml,yam_is_valid) {

    CTupleGenYamlInfo  fi;
    CTupleGenPoolYaml c_pool;
    CTupleGenPoolYaml s_pool;
    fi.m_client_pool.push_back(c_pool); 
    fi.m_server_pool.push_back(s_pool); 
    
    fi.m_client_pool[0].m_ip_start = 0x10000001;
    fi.m_client_pool[0].m_ip_end   = 0x100000ff;

    fi.m_server_pool[0].m_ip_start = 0x10000001;
    fi.m_server_pool[0].m_ip_end   = 0x100001ff;

    EXPECT_EQ(fi.is_valid(8,true)?1:0, 1);


    fi.m_client_pool[0].m_ip_start = 0x10000001;
    fi.m_client_pool[0].m_ip_end   = 0x100000ff;

    fi.m_server_pool[0].m_ip_start = 0x10000001;
    fi.m_server_pool[0].m_ip_end   = 0x10000007;

    EXPECT_EQ(fi.is_valid(8,true)?1:0, 0);

    fi.m_client_pool[0].m_ip_start = 0x10000001;
    fi.m_client_pool[0].m_ip_end   = 0x100000ff;

    fi.m_server_pool[0].m_ip_start = 0x10000001;
    fi.m_server_pool[0].m_ip_end   = 0x100003ff;

    EXPECT_EQ(fi.is_valid(8,true)?1:0, 1);

}



TEST(tuple_gen,astf_mode1) {
    #if 0
    #endif
}

TEST(tuple_gen,astf_mode2) {
    uint8_t  reta_mask=0x7f;
    int cid;
    int tcores;
    for (tcores=2; tcores<7; tcores++ ) {
        for (cid=0; cid<tcores; cid++) {
            int i;
            for (i=0; i<0xffff; i++) {
                uint16_t val=rss_align_lsb(i,cid,tcores,reta_mask);
                EXPECT_EQ((val&reta_mask)%tcores,cid);
            }
        }
    }
}

int test_gen_astf_rss(uint16_t rss_thread_id,
                      uint16_t rss_thread_max,
                      uint8_t  reta_mask,
                      int cnt){
    CTupleGeneratorSmart gen;
    gen.Create(1, 1);
    gen.set_astf_rss_mode(rss_thread_id,
                          rss_thread_max,
                          reta_mask);      
    gen.add_client_pool(cdSEQ_DIST,0x10000001,0x10000001,64000,g_dummy,0,0);
    gen.add_server_pool(cdSEQ_DIST,0x30000001,0x400000ff,64000,false);
    CTupleTemplateGeneratorSmart template_1;
    template_1.Create(&gen,0,0);

    CTupleBase result;

    int i;
    for (i=0; i<cnt; i++) {
        template_1.GenerateTuple(result);
        EXPECT_EQ(result.getClient(),0x10000001);
        //EXPECT_EQ(result.getClientPort(),golden[i]);
        if (result.getClientPort()!=0){
            uint16_t calc_thread_id=((rss_reverse_bits_port(result.getClientPort())&reta_mask)%rss_thread_max);


            if (calc_thread_id != rss_thread_id) {
                printf(" port:%x reverse_port_mask: %x  thread_id: %x original_thread_id: %d \n",
                                                                     result.getClientPort(),
                                                                    (rss_reverse_bits_port(result.getClientPort()) &reta_mask),
                                                                     calc_thread_id,rss_thread_id);

                EXPECT_EQ(calc_thread_id,rss_thread_id);    
            }

            CTupleGeneratorSmart * lpgen= template_1.get_gen();
            if ( lpgen->IsFreePortRequired(0) ){
                lpgen->FreePort(0,0, result.getClientPort());
            }

        }
    }
            
    template_1.Delete();
    gen.Delete();
    return(0);
}



TEST(tuple_gen,astf_mode3) {

    uint8_t  reta_mask=0x7f;
    int cid;
    int tcores;
    for (tcores=2; tcores<7; tcores++ ) {
        for (cid=0; cid<tcores; cid++) {
            test_gen_astf_rss(cid,
                              tcores,
                              reta_mask,
                              40000);

        }
    }
}


TEST(tuple_gen,astf_mode4) {

    uint8_t  reta_mask=0x7f;
    test_gen_astf_rss(1,3,reta_mask,80000);

}

void test_rss_sport_lsb_align(uint16_t rss_thread_id,
                              uint16_t rss_thread_max,
                              uint8_t rss_reta_mask) {
    int rand_value = rand();
    uint16_t rand_port = MIN_PORT + (rand_value % (MAX_PORT - MIN_PORT)) + 1;
    uint16_t aligned_port = rss_align_lsb(rand_port, rss_thread_id, rss_thread_max, rss_reta_mask);
    uint16_t calculated_thread_id = rss_get_thread_id_aligned(false, aligned_port, rss_thread_max, rss_reta_mask);
    EXPECT_EQ(rss_thread_id, calculated_thread_id);
}


TEST(tuple_gen, astf_mode5) {
    srand(time(NULL)); // Set the seed based on time so we get a different pattern on each run.

    // case 1
    uint8_t reta_mask = 0xff;
    uint16_t thread_max = 8;
    for (uint16_t i = 0; i < thread_max; i++) {
        test_rss_sport_lsb_align(i, thread_max, reta_mask);
    }

    // case 2
    thread_max = 5;
    for (uint16_t i = 0; i < thread_max; i++) {
        test_rss_sport_lsb_align(i, thread_max, reta_mask);
    }

    // case 3
    reta_mask = 0x7f;
    for (uint16_t i = 0; i < thread_max; i++) {
        test_rss_sport_lsb_align(i, thread_max, reta_mask);
    }

    // case 4
    thread_max = 4;
    for (uint16_t i = 0; i < thread_max; i++) {
        test_rss_sport_lsb_align(i, thread_max, reta_mask);
    }
}


