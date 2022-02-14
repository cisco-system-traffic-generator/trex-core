#include <common/gtest.h>
#include "stt_cp.h"

class gt_dyn_sts  : public testing::Test {

protected:
  virtual void SetUp() {
  }

  virtual void TearDown() {
  }
public:
};


#define DPS_DEFAULT_VALUE 10

static const meta_data_t meta_data_1 ={"Example1", 
                                       {CMetaDataPerCounter("Example1_counter_1", "help1"),
                                        CMetaDataPerCounter("Example1_counter_2", "help2")
                                       }
                                      };

static const meta_data_t meta_data_2 ={"Example2", 
                                       {CMetaDataPerCounter("Example2_counter_1", "help1"),
                                        CMetaDataPerCounter("Example2_counter_2", "help2"),
                                        CMetaDataPerCounter("Example2_counter_3", "help3")
                                       }
                                      };

typedef struct{
    uint64_t example1_counter1;
    uint64_t example1_counter2;
} Example1_t;

typedef struct{
    uint64_t example2_counter1;
    uint64_t example2_counter2;
    uint64_t example2_counter3;
} Example2_t;

class CDps {
public:
    CDps() {
        for (int i=0;i<TCP_CS_NUM;i++) {
            memset(&sts1[i],0,sizeof(Example1_t));
            memset(&sts2[i],0,sizeof(Example2_t));
        }
    }

    void initialize_dp_dyn_sts(DpsDynSts* dps_counters, const meta_data_t* meta_data) {
        if (meta_data->group_name == "Example1") {
            dps_counters->add_dp_dyn_sts(std::make_pair((uint64_t *)&sts1[0], (uint64_t *)&sts1[1]));
        } else {
             dps_counters->add_dp_dyn_sts(std::make_pair((uint64_t *)&sts2[0], (uint64_t *)&sts2[1]));
        }
    }

    void increment_dp_counters(const meta_data_t* meta_data, uint8_t index) {
        if (meta_data->group_name == "Example1") {
            increment_example1_counters(index);
        } else {
            increment_example2_counters(index);
        }
    }

    void increment_example1_counters(uint8_t index) {
        for(int i=0;i<TCP_CS_NUM;i++) {
            sts1[i].example1_counter1+=(index + 1 + i);
            sts1[i].example1_counter2+=(index + 2 + i);
        }
    }

    void increment_example2_counters(uint8_t index) {
        for(int i=0;i<TCP_CS_NUM;i++) {
            sts2[i].example2_counter1+=(index + 1 + i);
            sts2[i].example2_counter2+=(index + 2 + i);
            sts2[i].example2_counter3+=(index + 3 + i);
        }
    }

    uint64_t get_dp_counter(const meta_data_t* meta_data, uint8_t dir, uint8_t counter_index) {
        assert(dir == TCP_CLIENT_SIDE || dir == TCP_SERVER_SIDE);
        assert(counter_index < meta_data->meta_data_per_counter.size());
        uint64_t *p = nullptr;
        if (meta_data->group_name == "Example1") {
            p = (uint64_t*)&(sts1[dir]);
        } else {
            p = (uint64_t*)&(sts2[dir]);
        }
        return p[counter_index];
    }

    bool is_dp_counters_clear(const meta_data_t* meta_data) {
        uint64_t *p = nullptr;
        int size = meta_data->meta_data_per_counter.size();
        //getting the right counters
        for (int i=0;i<TCP_CS_NUM;i++) {
            if (meta_data->group_name == "Example1") {
                p = (uint64_t*)&(sts1[i]);
            } else {
                p = (uint64_t*)&(sts2[i]);
            }
            for (int j=0;j<size;j++) {
                if ( p[j] != 0 ) {
                    std::cout<<"value is:"<<p[j]<<endl;
                    return false;
                }
            }
        }
        return true;
    }


public:
    Example1_t sts1[TCP_CS_NUM];
    Example2_t sts2[TCP_CS_NUM];
};

class CDpsMngr {
public:
    CDpsMngr(uint8_t dps_count) {
        dps_vec = std::vector<CDps>(dps_count);
    }
    void initialize_dps_dyn_sts(DpsDynSts* dps_counters, const meta_data_t* meta_data) {
        for (int i=0;i<dps_vec.size();i++) {
            dps_vec[i].initialize_dp_dyn_sts(dps_counters, meta_data);
        }
    }

    void increment_dps_counters(const meta_data_t* meta_data) {
        for (int i=0;i<dps_vec.size();i++) {
            dps_vec[i].increment_dp_counters(meta_data, i);
        }
    }

    void increment_dps_counters(std::vector<const meta_data_t*> &meta_datas) {
        for(const meta_data_t* meta_data : meta_datas) {
            increment_dps_counters(meta_data);
        }
    }

    bool compare_value_to_dps_counter(const meta_data_t* meta_data, uint8_t dir, uint8_t counter_index, uint64_t exp_value) {
        uint64_t sum = 0;
        for (int i=0;i<dps_vec.size();i++) {
            sum +=dps_vec[i].get_dp_counter(meta_data, dir, counter_index);
        }
        return (sum == exp_value);
    }

    bool is_dps_counters_clear(const meta_data_t* meta_data) {
        bool res = true;
        for (int i=0;i<dps_vec.size();i++) {
            res = res && dps_vec[i].is_dp_counters_clear(meta_data);
        }
        return res;
    }

    bool compare_group_sts_to_dps_counters(CDynStsCpGroup* group, const meta_data_t* meta_data, uint8_t dir) {
        int len = group->m_dyn_sts_group_args.size;
        for (int i=0;i<len;i++) {
            uint64_t exp_value = group->m_dyn_sts_group_args.real_counters[i];
            if (!compare_value_to_dps_counter(meta_data, dir, i, exp_value)) {
                return false;
            }
        }
        return true;
    }

    bool compare_sttcp_to_dps_counters(CSTTCp* sts, std::vector<const meta_data_t*> &meta_datas) {
        for (const meta_data_t* meta_data : meta_datas) {
            for (int i=0;i<TCP_CS_NUM;i++) {
                dyn_sts_group_vec_t & dyn_sts_vec = sts->m_sts[i].m_dyn_sts;
                for (int j=0;j<dyn_sts_vec.size();j++) {
                    CDynStsCpGroup *group_sts = dyn_sts_vec[j];
                    if (group_sts->m_dyn_sts_group_args.group_name != meta_data->group_name) {
                        continue;
                    }
                    if (!compare_group_sts_to_dps_counters(group_sts, meta_data, i)) {
                        return false;
                    }
                }
            }
        }
        return true;
    }

public:
    std::vector<CDps> dps_vec;
};

/************************************ global help functions ****************************************************/

void delete_allocate_counters(std::vector<uint64_t*> &allocated_counters) {

    for(int i=0;i<allocated_counters.size();i++) {
        delete[] allocated_counters[i];
    }
    allocated_counters.clear();
}


void initialize_dyn_sts_group_args(cp_dyn_sts_group_args_t args[TCP_CS_NUM],
                                   const meta_data_t* meta_data,
                                   DpsDynSts* dps_counters,
                                   std::vector<uint64_t*> &allocated_counters) {

    size_t num_counters = meta_data->meta_data_per_counter.size();
    for(int i=0;i<TCP_CS_NUM;i++) {
        args[i].group_name = meta_data->group_name;
        args[i].size = meta_data->meta_data_per_counter.size();
        args[i].per_side_dp_sts = dps_counters->m_dp_dyn_sts_vec[i];
        args[i].real_counters = new uint64_t[num_counters];
        allocated_counters.push_back(args[i].real_counters);
    }
}


/********************************** CSTTCp test help function *******************************************************/

CSTTCp* get_initialized_CSTTCp_test() {
    CSTTCp* sts = new CSTTCp();
    sts->Create(0, 0, true);
    sts->Init(true, true);
    sts->m_init = true;
    return sts;
}

void verifies_dyn_sts_not_exist(CSTTCp *sts,
                            std::vector<const meta_data_t*> &meta_datas) {
    for (int i=0;i<TCP_CS_NUM;i++) {
        dyn_sts_group_vec_t & dyn_sts_vec = sts->m_sts[i].m_dyn_sts;
        for (const meta_data_t* meta_data : meta_datas) {
            bool res = true;
            for (int j=0;j<dyn_sts_vec.size();j++) {
                if (dyn_sts_vec[j]->m_dyn_sts_group_args.group_name == meta_data->group_name) {
                    res = false;
                }
            }
            EXPECT_EQ(1, res?1:0);
        }
    }
}


void verifies_dyn_sts_exist(CSTTCp *sts,
                            std::vector<const meta_data_t*> &meta_datas) {
    for (int i=0;i<TCP_CS_NUM;i++) {
        dyn_sts_group_vec_t & dyn_sts_vec = sts->m_sts[i].m_dyn_sts;
        for (const meta_data_t* meta_data : meta_datas) {
            bool res = false;
            for (int j=0;j<dyn_sts_vec.size();j++) {
                if (dyn_sts_vec[j]->m_dyn_sts_group_args.group_name == meta_data->group_name) {
                    res = (dyn_sts_vec[j]->m_dyn_sts_group_args.size = meta_data->meta_data_per_counter.size());
                    EXPECT_EQ(1, res?1:0);
                }
            }
            EXPECT_EQ(1, res?1:0);
        }
    }
}


void add_counters(CSTTCp *sts,
                 CDpsMngr *mngr,
                 std::vector<const meta_data_t*> &meta_datas,
                 std::vector<uint64_t*> &allocated_counters) {

    for(int i=0;i<meta_datas.size();i++) {
        DpsDynSts dps_counters;
        mngr->initialize_dps_dyn_sts(&dps_counters, meta_datas[i]);
        cp_dyn_sts_group_args_t args[TCP_CS_NUM];
        initialize_dyn_sts_group_args(args, meta_datas[i], &dps_counters, allocated_counters);
        //adding the counters
        bool res = sts->Add_dyn_stats(meta_datas[i], args);
        EXPECT_EQ(1, res?1:0);
    }
    sts->clear_counters();

    //verifies that indeed the dynamic counters were added
    verifies_dyn_sts_exist(sts, meta_datas);
}


void create_and_remove_dyn_counters(uint8_t dps_count, 
                                    std::vector<const meta_data_t*> meta_datas,
                                    bool with_remove=true, 
                                    bool reverse_remove=false) {

    std::vector<uint64_t*> allocated_counters;
    CDpsMngr mngr = CDpsMngr(dps_count);
    CSTTCp *sts = get_initialized_CSTTCp_test();
    add_counters(sts, &mngr, meta_datas, allocated_counters);


    //removing the counters
    if (with_remove) {
        //removing them in reverse order
        if (reverse_remove) {
            std::reverse(meta_datas.begin(), meta_datas.end());
        }
        for(int i=0;i<meta_datas.size();i++) {
            bool res = sts->Delete_dyn_sts(meta_datas[i]->group_name);
            EXPECT_EQ(1, res?1:0);
        }
        verifies_dyn_sts_not_exist(sts, meta_datas);
    }

    delete_allocate_counters(allocated_counters);
    sts->Delete();
    delete sts;
}

void compare_sts_values(CSTTCp* sts,
                        CDpsMngr *mngr,
                        std::vector<const meta_data_t*> &meta_datas
                        ) {
    verifies_dyn_sts_exist(sts, meta_datas);
    bool res;
    //verifies that the dynamic allocation by cheking if the real counters are being updated.
    for (int i=0;i<100;i++) {
        mngr->increment_dps_counters(meta_datas);
        sts->Update();
        res = mngr->compare_sttcp_to_dps_counters(sts, meta_datas);
        EXPECT_EQ(1, res?1:0);
    }
}

void add_dyn_counters_with_compare(uint8_t dps_count, 
                                                std::vector<const meta_data_t*> meta_datas) {

    std::vector<uint64_t*> allocated_counters;
    std::vector<CSTTCp *> sts_vec = {get_initialized_CSTTCp_test(), get_initialized_CSTTCp_test()};
    CDpsMngr mngr = CDpsMngr(dps_count);
    for (CSTTCp * sts : sts_vec) {
        add_counters(sts, &mngr, meta_datas, allocated_counters);
        sts->clear_counters();
    }
    bool res;
    for (int i=0;i<10;i++) {
        for (const meta_data_t* meta_data : meta_datas) {
            mngr.increment_dps_counters(meta_data);
        }
        sts_vec[0]->Update();
        res = mngr.compare_sttcp_to_dps_counters(sts_vec[0], meta_datas);
        EXPECT_EQ(1, res?1:0);
    }


    sts_vec[1]->Accumulate(true, false, sts_vec[0]);
    res = mngr.compare_sttcp_to_dps_counters(sts_vec[1], meta_datas);
    EXPECT_EQ(1, res?1:0);

    //checking the dps_counters clear
    sts_vec[0]->clear_dps_dyn_counters();
    for (const meta_data_t * meta_data : meta_datas) {
        res = mngr.is_dps_counters_clear(meta_data);
        EXPECT_EQ(1, res?1:0);
    }

    for (CSTTCp * sts : sts_vec) {
        sts->Delete();
        delete sts;
    }
    delete_allocate_counters(allocated_counters);
}


void create_and_remove_dyn_counters_all_option(uint8_t dp_counter,
                                               std::vector<const meta_data_t*>& meta_datas) {

    std::vector<bool> remove_vec = {true, false};
    std::vector<bool> revese_vec = {true, false};
    for (bool remove : remove_vec) {
        for (bool reverse : revese_vec) {
            create_and_remove_dyn_counters(dp_counter, meta_datas, remove, reverse);
        }
    }
}

/********************************** CCpDynStsInfra test help function *******************************************************/

bool create_and_remove_dyn_counters_infra(CCpDynStsInfra* infra,
                                          CDpsMngr* mngr,
                                          std::string profile_id,
                                          std::vector<const meta_data_t*>& meta_datas,
                                          bool with_remove=true) {
    // adding counters for each meta data
    for(int i=0;i<meta_datas.size();i++) {
        DpsDynSts dps_counters;
        mngr->initialize_dps_dyn_sts(&dps_counters, meta_datas[i]);
        if (!(infra->add_counters(meta_datas[i], &dps_counters, profile_id))) {
            return false;
        }
    }

    if (with_remove) {
        for(int i=0;i<meta_datas.size();i++) {
            if (!(infra->remove_counters(meta_datas[i], profile_id))) {
                return false;
            }
        }
        //verifies that indeed the counters were deleted
        // In it isn't the deafult profile it might get deleted if it has no other counters
        if (infra->is_valid_profile(profile_id)) {
            verifies_dyn_sts_not_exist(infra->get_profile_sts(profile_id), meta_datas);
        }
    }
    return true;
}

/********************************** CDynStsCpGroup test help function *******************************************************/

void group_sts_test(uint8_t dps_count,
                    const meta_data_t* meta_data) {

    std::vector<uint64_t*> allocated_counters;
    cp_dyn_sts_group_args_t args[TCP_CS_NUM];
    CDpsMngr mngr = CDpsMngr(dps_count);
    DpsDynSts dps_counters;
    size_t num_counters = meta_data->meta_data_per_counter.size();
    // initiallizing the dp counters with inital value
    mngr.initialize_dps_dyn_sts(&dps_counters, meta_data);
    initialize_dyn_sts_group_args(args, meta_data, &dps_counters, allocated_counters);
    // initialize CDynStsCpGroup object with dps counters
    CDynStsCpGroup group1(&args[0]);
    bool res = (group1.m_dyn_sts_group_args.size == num_counters);
    EXPECT_EQ(1, res?1:0);

    //incremeting the dps counters and updating the group sts and verifies the values are the same
    for (int k=0;k<10;k++) {
        mngr.increment_dps_counters(meta_data);
        group1.update();
        res = mngr.compare_group_sts_to_dps_counters(&group1, meta_data, 0);
        EXPECT_EQ(1, res?1:0);
    }

    CDynStsCpGroup group2(&args[1]);
    group2.clear();
    // accumulate the counters from group 1, now they should have the same real counters
    group2.accumulate(&group1);
    for (int i=0;i<num_counters;i++) {
        uint64_t exp_value = group1.m_dyn_sts_group_args.real_counters[i];
        res = (group2.m_dyn_sts_group_args.real_counters[i] == exp_value);
        EXPECT_EQ(1, res?1:0);
    }
    //test clear_clear_dps_dyn_counters
    group1.clear_dps_dyn_counters();

    uint64_t exp_value = 0;
    for (int i=0;i<num_counters;i++) {
        res = mngr.compare_value_to_dps_counter(meta_data, 0, i, exp_value);
        EXPECT_EQ(1, res?1:0);
    }
    delete_allocate_counters(allocated_counters);
}


/*************************************************** CCpMngr *****************************************************/

class CCpMngr {
public:
    CCpMngr(uint8_t dps_count) {
        m_dp_mngr = new CDpsMngr(dps_count);
        m_infra = new CCpDynStsInfra(dps_count);
    }

    void add_counters(Json::Value& params, Json::Value& result, const meta_data_t* meta_data) {
        result.clear();
        std::string profile_id = params["profile_id"].asString();
        DpsDynSts dps_sts;
        m_dp_mngr->initialize_dps_dyn_sts(&dps_sts, meta_data);
        bool res = m_infra->add_counters(meta_data, &dps_sts, profile_id);
        result["success"] = res;
    }

    void remove_counters(Json::Value& params, Json::Value& result, const meta_data_t* meta_data) {
        result.clear();
        std::string profile_id = params["profile_id"].asString();
        bool res = m_infra->remove_counters(meta_data, profile_id);
        result["success"] = res;
    }

    void get_meta_data(Json::Value& params, Json::Value& result) {
        result.clear();
        CSTTCp *lp = nullptr;
        bool is_sum = false;
        if (params.isMember("is_sum")) {
            is_sum = params["is_sum"].asBool();
        }
        if (is_sum) {
            lp = m_infra->m_total_sts;
        } else {
            std::string profile_id = params["profile_id"].asString();
            if ( !m_infra->is_valid_profile(profile_id)) {
                result["success"] = false;
                return;
            }
            lp = m_infra->get_profile_sts(profile_id);
        }
        lp->m_dtbl.dump_meta("counter desc", result["result"]);
        result["result"]["epoch"] = lp->m_epoch;
        result["success"] = true;
    }

    void get_values(Json::Value& params, Json::Value& result) {
        result.clear();
        std::string profile_id = params["profile_id"].asString();
        if (!m_infra->is_valid_profile(profile_id)) {
            result["error"] = "Invalid profile : " + profile_id;
            result["success"] = false;
            return;
        }
        CSTTCp *lpstt = m_infra->get_profile_sts(profile_id);
        uint64_t epoch =  (uint64_t)params["epoch"].asUInt();
        if (epoch != lpstt->m_epoch) {
            result["result"]["epoch_err"] = "Try again: Epoch is not updated";
            result["success"] = true;
            return;
        }
        lpstt->m_dtbl.dump_values("counter vals", false, result["result"]);
        result["success"] = true;
    }

    void get_total_values(Json::Value& params, Json::Value& result) {
        result.clear();
        uint64_t epoch = (uint64_t)params["epoch"].asUInt();
        if (epoch != m_infra->m_total_sts->m_epoch) {
            result["result"]["epoch_err"] = "Try again: Epoch is not updated";
            result["success"] = true;
            return;
        }
        vector<CSTTCp *> sttcp_total_list = m_infra->get_dyn_sts_list();
        for (auto lpstt : sttcp_total_list) {
            bool clear = false;
            if(lpstt == sttcp_total_list.front()) {
                clear = true;
            }
            m_infra->accumulate_total_dyn_sts(clear, lpstt);
        }
        m_infra->m_total_sts->m_dtbl.dump_values("counter vals", false, result["result"]);
        result["success"] = true;
    }

    void inceremet_dps(std::vector<const meta_data_t*> meta_datas) {
        m_dp_mngr->increment_dps_counters(meta_datas);
    }

    void update_sts_objects() {
        vector<CSTTCp *> sttcp_total_list = m_infra->get_dyn_sts_list();
        for (auto lpstt : sttcp_total_list) {
            lpstt->Update();
        }
    }

    ~CCpMngr() {
        delete m_infra;
        delete m_dp_mngr;
    }

public:
    CDpsMngr*       m_dp_mngr;
    CCpDynStsInfra* m_infra;
};


/*************************** CDynStsCpGroup test ***************************/

//test for CDynStsCpGroup clear
TEST_F(gt_dyn_sts, tst1) {
    cp_dyn_sts_group_args_t args;
    size_t size = 10;
    uint64_t* counters = new uint64_t[size];
    args.real_counters = counters;
    args.size = size;
    CDynStsCpGroup group(&args);
    for (int i=0;i<size;i++) {
        group.m_dyn_sts_group_args.real_counters[i] = DPS_DEFAULT_VALUE;
    }
    group.clear();
    bool res = true;
    for (int i=0;i<size;i++) {
        res = res && (group.m_dyn_sts_group_args.real_counters[i] == 0);
    }
    EXPECT_EQ(1, res?1:0);
    delete[] counters;
}


//test for update and accumulate and clear_dps_dynamic_counters
TEST_F(gt_dyn_sts, tst2) {
    group_sts_test(5, &meta_data_1);
    group_sts_test(7, &meta_data_2);
}


/*************************** CSTTCp dynamic counters test ***************************/

TEST_F(gt_dyn_sts, tst3) {
    std::vector<const meta_data_t*> meta_datas = {&meta_data_1};
    create_and_remove_dyn_counters_all_option(1, meta_datas);
}


TEST_F(gt_dyn_sts, tst4) {
    std::vector<const meta_data_t*> meta_datas = {&meta_data_1, &meta_data_2};
    create_and_remove_dyn_counters_all_option(5, meta_datas);
}


TEST_F(gt_dyn_sts, tst5) {
    std::vector<const meta_data_t*> meta_datas = {&meta_data_1, &meta_data_2};
    add_dyn_counters_with_compare(5, meta_datas);
}

/*************************** CCpDynStsInfra test ***************************/

TEST_F(gt_dyn_sts, tst6) {
    CCpDynStsInfra* infra = new CCpDynStsInfra(5);
    //should fail because we didn't added counters
    bool res = (infra->remove_counters(&meta_data_1) == false);
    EXPECT_EQ(1, res?1:0);
    delete infra;
}

TEST_F(gt_dyn_sts, tst7) {
    std::vector<const meta_data_t*> meta_datas = {&meta_data_1};
    uint8_t dps_count = 5;
    CCpDynStsInfra* infra = new CCpDynStsInfra(dps_count);
    CDpsMngr mngr = CDpsMngr(dps_count);
    std::string profile_id = "profile_1";
    bool res;
    //adding dynamic counters and verifies success, without removing the counters
    res = create_and_remove_dyn_counters_infra(infra, &mngr, profile_id, meta_datas, false);
    EXPECT_EQ(1, res?1:0);
    //should return true because we added dynamic counters to this profile
    res = (infra->is_valid_profile(profile_id));
    EXPECT_EQ(1, res?1:0);
    CSTTCp* sts =  infra->get_profile_sts(profile_id);
    //verifies that we indeed have dynamic counters inside the CSTTCp objects
    compare_sts_values(sts, &mngr, meta_datas);
    //each dynamic counter should added to the total sts
    compare_sts_values(infra->m_total_sts, &mngr, meta_datas);
    sts = nullptr;
    delete infra;
}


TEST_F(gt_dyn_sts, tst8) {
    std::vector<const meta_data_t*> meta_datas = {&meta_data_1};
    uint8_t dps_count = 5;
    CDpsMngr mngr = CDpsMngr(dps_count);
    CCpDynStsInfra* infra = new CCpDynStsInfra(dps_count);
    bool res;
    //adding dynamic counters and verifies success, without removing the counters
    res = create_and_remove_dyn_counters_infra(infra, &mngr, DEFAULT_ASTF_PROFILE_ID, meta_datas, false);
    EXPECT_EQ(1, res?1:0);
    //should fail because we already added dynamic counter with the same meta data
    res = create_and_remove_dyn_counters_infra(infra, &mngr, "profile1", meta_datas, false);
    EXPECT_EQ(1, res?0:1);
    delete infra;
}


TEST_F(gt_dyn_sts, tst9) {
    std::vector<const meta_data_t*> meta_datas = {&meta_data_1};
    uint8_t dps_count = 5;
    CDpsMngr mngr = CDpsMngr(dps_count);
    CCpDynStsInfra* infra = new CCpDynStsInfra(dps_count);
    bool res;
    //adding dynamic counters and verifies success, with removing the counters
    res = create_and_remove_dyn_counters_infra(infra, &mngr, "profile1", meta_datas, true);
    EXPECT_EQ(1, res?1:0);
    //should fail because "profile_1" has no dynamic counters because we deleted them
    res = infra->get_profile_sts("profile1");
    EXPECT_EQ(1, res?0:1);
    delete infra;
}


TEST_F(gt_dyn_sts, tst10) {
    std::vector<const meta_data_t*> meta_datas = {&meta_data_1, &meta_data_2};
    std::vector<uint64_t*> allocated_counters;
    uint8_t dps_count = 5;
    CDpsMngr mngr = CDpsMngr(dps_count);
    CCpDynStsInfra* infra = new CCpDynStsInfra(dps_count);
    bool res;
    //adding multiple dynamic counters to the deafult profile and removing them, verifies success
    res = create_and_remove_dyn_counters_infra(infra, &mngr, DEFAULT_ASTF_PROFILE_ID, meta_datas, true);
    EXPECT_EQ(1, res?1:0);
    //adding multiple dynamic counters to the profile "1" without removing them, verifies success
    res = create_and_remove_dyn_counters_infra(infra, &mngr, "1", meta_datas, false);
    EXPECT_EQ(1, res?1:0);
    //should delete also the dynamic counters of profile "1"
    delete infra;
}

TEST_F(gt_dyn_sts, tst11) {
    std::vector<const meta_data_t*> meta_datas = {&meta_data_1, &meta_data_2};
    uint8_t dps_count = 5;
    CCpDynStsInfra* infra = new CCpDynStsInfra(dps_count);
    CDpsMngr mngr = CDpsMngr(dps_count);

    std::vector<std::string> profiles = {"profile_1", "profile_2"};
    bool res;
    for (int i=0;i<profiles.size();i++) {
        //adding dynamic counters and verifies success, without removing the counters
        std::vector<const meta_data_t*> meta_datas_per_profile = {meta_datas[i]};
        res = create_and_remove_dyn_counters_infra(infra, &mngr, profiles[i], meta_datas_per_profile, false);
        EXPECT_EQ(1, res?1:0);
    }

    for (int i=0;i<profiles.size();i++) {
        //should return true because we added dynamic counters to this profile
        res = (infra->is_valid_profile(profiles[i]));
        EXPECT_EQ(1, res?1:0);
        CSTTCp* sts =  infra->get_profile_sts(profiles[i]);
        std::vector<const meta_data_t*> meta_datas_per_profile = {meta_datas[i]};
        //verifies that we indeed have dynamic counters inside the CSTTCp object
        compare_sts_values(sts, &mngr, meta_datas_per_profile);
        //clears the dps counter of this profile and verifies success
        infra->clear_dps_dyn_counters(profiles[i]);
        res = mngr.is_dps_counters_clear(meta_datas[i]);
        EXPECT_EQ(1, res?1:0);
    }
    //the total_sts should have counters of both meta datas 1 and 2.
    compare_sts_values(infra->m_total_sts, &mngr, meta_datas);
    delete infra;
}

TEST_F(gt_dyn_sts, tst12) {
    std::vector<const meta_data_t*> meta_datas = {&meta_data_1};
    uint8_t dps_count = 5;
    CCpDynStsInfra* infra = new CCpDynStsInfra(dps_count);
    CDpsMngr mngr = CDpsMngr(dps_count);
    CSTTCp* deafult_profile = infra->get_profile_sts(DEFAULT_ASTF_PROFILE_ID);
    std::vector<CSTTCp*> sts_vec = {infra->m_total_sts, deafult_profile};
    std::vector<uint64_t> prev_epoch_vec = {infra->m_total_sts->m_epoch, deafult_profile->m_epoch};
    //adding counters - should change the epoch of of both CSTTCp object, total and deafult
    create_and_remove_dyn_counters_infra(infra, &mngr, DEFAULT_ASTF_PROFILE_ID, meta_datas, false);
    bool res;
    // comparing the epoch after adding counters - should not be equal
    for(int i=0;i<sts_vec.size();i++) {
        res = (sts_vec[i]->m_epoch != prev_epoch_vec[i]);
        EXPECT_EQ(1, res?1:0);
        //updating the epoch inside prev_epoch_vec
        prev_epoch_vec[i] = sts_vec[i]->m_epoch;
    }
    //removing meta data 1 counters from deafult profile - should change the epoch of both CSTTCp object, total and deafult.
    infra->remove_counters(&meta_data_1);
    for(int i=0;i<sts_vec.size();i++) {
        res = (sts_vec[i]->m_epoch != prev_epoch_vec[i]);
        EXPECT_EQ(1, res?1:0);
    }
    delete infra;
}

TEST_F(gt_dyn_sts, tst13) {
    CCpMngr cp = CCpMngr(7);
    Json::Value params;
    Json::Value result1;
    Json::Value result2;
    params["profile_id"] = DEFAULT_ASTF_PROFILE_ID;
    cp.add_counters(params, result1, &meta_data_1);
    bool res = result1["success"].asBool();
    EXPECT_EQ(1, res?1:0);
    result1.clear();
    //result1 holds the meta data of the deafult profile
    cp.get_meta_data(params, result1);
    params.clear();
    params["is_sum"] = true;
    //result2 holds the total meta data
    cp.get_meta_data(params, result2);
    std::cout<<endl<<endl<<result1<<endl;
    std::cout<<result2<<endl<<endl;
    //the meta datas should be the same because we only have 1 profile
    res = (result1 == result2);
    EXPECT_EQ(1, res?1:0);
}

TEST_F(gt_dyn_sts, tst14) {
    std::vector<const meta_data_t*> meta_datas = {&meta_data_1, &meta_data_2};
    CCpMngr cp = CCpMngr(7);
    Json::Value params;
    Json::Value result1;
    Json::Value result2;
    params["profile_id"] = DEFAULT_ASTF_PROFILE_ID;
    cp.add_counters(params, result1, meta_datas[0]);
    cp.add_counters(params, result1, meta_datas[1]);
    bool res = result1["success"].asBool();
    EXPECT_EQ(1, res?1:0);
    for (int i=0;i<10;i++) {
        cp.inceremet_dps(meta_datas);
        cp.update_sts_objects();
    }

    //bad epoch
    params["epoch"] = 0;
    cp.get_values(params, result1);
    res = result1["result"].isMember("epoch_err");
    EXPECT_EQ(1, res?1:0);

    params.clear();
    params["profile_id"] = DEFAULT_ASTF_PROFILE_ID;

    //updateing the epoch
    cp.get_meta_data(params, result1);
    uint64_t new_epoch = (uint64_t)result1["result"]["epoch"].asUInt();
    params["epoch"] = new_epoch;
    //result1 holds the values of the default profile
    cp.get_values(params, result1);
    res = !(result1["result"].isMember("epoch_err"));
    EXPECT_EQ(1, res?1:0);

    //result2 holds the total values
    cp.get_total_values(params, result2);

    std::cout<<endl<<endl<<result1<<endl;
    std::cout<<result2<<endl<<endl;
    //the values should be the same because we only have 1 profile
    res = (result1 == result2);
    EXPECT_EQ(1, res?1:0);
}

TEST_F(gt_dyn_sts, tst15) {
    std::vector<const meta_data_t*> meta_datas = {&meta_data_1, &meta_data_2};
    CCpMngr cp = CCpMngr(7);
    Json::Value params;
    Json::Value result1;
    Json::Value result2;
    Json::Value result3;
    params["profile_id"] = DEFAULT_ASTF_PROFILE_ID;
    cp.add_counters(params, result1, meta_datas[0]);
    params["profile_id"] = "profile_1";
    cp.add_counters(params, result1, meta_datas[1]);
    for (int i=0;i<10;i++) {
        cp.inceremet_dps(meta_datas);
        cp.update_sts_objects();
    }

    cp.get_meta_data(params, result1);
    params["profile_id"] = DEFAULT_ASTF_PROFILE_ID;
    cp.get_meta_data(params, result2);
    params["is_sum"] = true;
    cp.get_meta_data(params, result3);
    bool res = false;
    //none of them should be the same
    res = res || (result1 == result2);
    res = res || (result1 == result3);
    res = res || (result2 == result3);
    //res should be false
    EXPECT_EQ(0, res?1:0);

    std::cout<<endl<<endl<<"profile_1:"<<endl;
    std::cout<<result1<<endl;
    std::cout<<"default profile:"<<endl;
    std::cout<<result2<<endl;
    std::cout<<"total"<<endl;
    std::cout<<result3<<endl<<endl;

    //the total sts epoch is 2 since we added two counters
    params["epoch"] = 2;
    cp.get_total_values(params, result3);
    //the epoch of the default and profile_1 profiles is 1 since we added one counter to each of them
    params.clear();
    params["epoch"] = 1;
    params["profile_id"] = DEFAULT_ASTF_PROFILE_ID;
    cp.get_values(params, result2);
    params["profile_id"] = "profile_1";
    cp.get_values(params, result1);

    //none of them should be the same
    res = res || (result1 == result2);
    res = res || (result1 == result3);
    res = res || (result2 == result3);

    //res should be false
    EXPECT_EQ(0, res?1:0);
}