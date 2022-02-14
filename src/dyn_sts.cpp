#include "dyn_sts.h"
#include "string.h"
#include "stt_cp.h"

CSTTCp* get_initialized_CSTTCp() {
    CSTTCp* sts = new CSTTCp();
    sts->Create(0, 0, true);
    sts->Init(true, true);
    sts->m_init = true;
    return sts;
}

CCpDynStsInfra::CCpDynStsInfra(uint16_t num_cores) {
    m_num_cores = num_cores;
    m_total_sts = get_initialized_CSTTCp();
    CSTTCp *deafult_profile_sts = get_initialized_CSTTCp();
    m_profiles_map[DEFAULT_ASTF_PROFILE_ID] = deafult_profile_sts;
}


std::vector<CSTTCp *> CCpDynStsInfra::get_dyn_sts_list(){
    std::vector<CSTTCp *> sts_vec;
    for (auto pair : m_profiles_map) {
        sts_vec.push_back(pair.second);
    }
    return sts_vec;
}


bool CCpDynStsInfra::add_counters(const meta_data_t* meta_data, DpsDynSts* dps_counters, std::string profile_id) {
    //making sure that we get dynamic counters of all dps
    assert(dps_counters->m_dp_dyn_sts_vec[TCP_CLIENT_SIDE].size() == m_num_cores);
    assert(dps_counters->m_dp_dyn_sts_vec[TCP_SERVER_SIDE].size() == m_num_cores);
    //checking we don't have group sts with the same name
    if (m_group_sts_set.find(meta_data->group_name) != m_group_sts_set.end()) {
        return false;
    }
    // saving the group sts name
    m_group_sts_set.insert(meta_data->group_name);

    //initialize the args from the meta and dps_counters
    cp_dyn_sts_group_args_t dyn_sts_group_args[TCP_CS_NUM];
    for(int j=0;j<TCP_CS_NUM;j++) {
        dyn_sts_group_args[j].group_name = meta_data->group_name;
        dyn_sts_group_args[j].size =meta_data->meta_data_per_counter.size();
        dyn_sts_group_args[j].per_side_dp_sts = dps_counters->m_dp_dyn_sts_vec[j];
    }
    // getting the right CSTTCp pointer according to the profile id
    if (m_profiles_map.find(profile_id) == m_profiles_map.end()) {
        CSTTCp* sts = get_initialized_CSTTCp();
        m_profiles_map[profile_id] = sts;
    }
    // The CSTTCp instances that we are going to add the dynamic counters to
    // We always add the dynamic counters to the m_total_sts instance since we need to collect all the counters from it.
    std::vector<CSTTCp *> sts_vec = {m_profiles_map[profile_id], m_total_sts};

    // looping over the right CSTTCp instances and adding them the dynamic counters
    for (auto lpstt : sts_vec) {
        for (int i=0;i<TCP_CS_NUM;i++) {
            //allocating real counters per side for each lpstt instance
            size_t size =  dyn_sts_group_args[i].size;
            uint64_t *real_counters = new uint64_t[size];
            memset((void *)real_counters, 0, size*sizeof(uint64_t));
            //saving the counters so we can't delete them later
            m_dyn_real_counters[i].push_back(make_pair(meta_data->group_name, real_counters));
            dyn_sts_group_args[i].real_counters = real_counters;
        }
        lpstt->Add_dyn_stats(meta_data, dyn_sts_group_args);
    }
    return true;
}


bool CCpDynStsInfra::remove_counters(const meta_data_t* meta_data, std::string profile_id) {
    std::string group_sts_name = meta_data->group_name;
    // checking that we added the counters
    if (m_group_sts_set.find(group_sts_name) == m_group_sts_set.end()) {
        return false;
    }
    // checking that we added the counters to this profile id
    if (m_profiles_map.find(profile_id) == m_profiles_map.end()) {
        return false;
    }
    //deleting the name from the group sts set
    m_group_sts_set.erase(group_sts_name);
    //deleting the allocated records from the CSTTcp objects
    std::vector<CSTTCp *> sts_vec = {m_profiles_map[profile_id], m_total_sts};
    for (auto lpstt : sts_vec) {
        lpstt->Delete_dyn_sts(group_sts_name);
    }

    //should delete the profile sts if has no dynamic counters
    auto lp = m_profiles_map[profile_id];
    if ((lp->m_sts[0].m_dyn_sts.size() == 0) && (profile_id != DEFAULT_ASTF_PROFILE_ID)) {
        lp->Delete();
        delete lp;
        m_profiles_map.erase(profile_id);
    }

    //deleting the allocated counters from m_dyn_real_counters
    for (int i=0;i<TCP_CS_NUM;i++) {
        for (int j=0;j<m_dyn_real_counters[i].size();j++) {
            if (m_dyn_real_counters[i][j].first == group_sts_name){
                delete[] m_dyn_real_counters[i][j].second;
                m_dyn_real_counters[i].erase(m_dyn_real_counters[i].begin() + j);
            }
        }
    }
    return true;
}

void CCpDynStsInfra::accumulate_total_dyn_sts(bool clear, CSTTCp* lp) {
    m_total_sts->Accumulate(clear, false, lp);
}


bool CCpDynStsInfra::is_valid_profile(std::string profile_id) {
    if (m_profiles_map.find(profile_id) == m_profiles_map.end()) {
        return false;
    }
    return true;
}


void CCpDynStsInfra::clear_dps_dyn_counters(std::string profile_id) {
    if (!is_valid_profile(profile_id)) {
        return;
    }
    CSTTCp* p = get_profile_sts(profile_id);
    p->clear_dps_dyn_counters();
    m_total_sts->m_epoch++;
}


CSTTCp* CCpDynStsInfra::get_profile_sts(std::string profile_id) {
    if (!is_valid_profile(profile_id)) {
        return nullptr;
    }
    return m_profiles_map[profile_id];
}


CCpDynStsInfra::~CCpDynStsInfra() {
    m_total_sts->Delete();
    delete m_total_sts;
    for (auto p : m_profiles_map) {
        p.second->Delete();
        delete p.second;
    }

    for (int i=0;i<TCP_CS_NUM;i++) {
        for (int j=0;j<m_dyn_real_counters[i].size();j++) {
            delete[] m_dyn_real_counters[i][j].second;
        }
        m_dyn_real_counters[i].clear();
    }
}


void CDynStsCpGroup::clear() {
    memset((void *)m_dyn_sts_group_args.real_counters, 0, m_dyn_sts_group_args.size*sizeof(uint64_t));
}


void CDynStsCpGroup::accumulate(CDynStsCpGroup* sts) {
    if (sts->m_dyn_sts_group_args.group_name != m_dyn_sts_group_args.group_name) {
        return;
    }
    CGCountersUtl64 dyn_sts(m_dyn_sts_group_args.real_counters, m_dyn_sts_group_args.size);
    CGCountersUtl64 dyn_sts_add(sts->m_dyn_sts_group_args.real_counters, m_dyn_sts_group_args.size);
    dyn_sts+=dyn_sts_add;
}


void CDynStsCpGroup::update() {
    clear();
    CGCountersUtl64 dyn_sts(m_dyn_sts_group_args.real_counters, m_dyn_sts_group_args.size);
    for(int i=0;i<m_dyn_sts_group_args.per_side_dp_sts.size();i++){
        CGCountersUtl64 dyn_sts_add(m_dyn_sts_group_args.per_side_dp_sts[i], m_dyn_sts_group_args.size);
        dyn_sts+=dyn_sts_add;
    }
}

void CDynStsCpGroup::clear_dps_dyn_counters() {
    for(int i=0;i<m_dyn_sts_group_args.per_side_dp_sts.size();i++){
        memset((void*)m_dyn_sts_group_args.per_side_dp_sts[i], 0, m_dyn_sts_group_args.size * sizeof(uint64_t));
    }
}