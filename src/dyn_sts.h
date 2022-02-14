#ifndef __DYN_STS_H__
#define __DYN_STS_H__

#include <string>
#include "stdint.h"
#include <vector>
#include <set>
#include "utl_counter.h"

const std::string DEFAULT_ASTF_PROFILE_ID = "_";

typedef enum {
    TCP_CLIENT_SIDE = 0,
    TCP_SERVER_SIDE = 1,
    TCP_CS_NUM = 2,
    TCP_CS_INVALID = 255
} tcp_dir_enum_t;

class CSTTCp;

typedef std::map<std::string, CSTTCp*>                 profiles_map_t;
typedef std::vector<uint64_t*>                         per_side_dp_sts_vec_t;
typedef std::set<std::string>                          group_sts_set_t;
typedef std::pair<uint64_t*, uint64_t*>                dp_sts_t;
typedef std::vector<std::pair<std::string, uint64_t*>> dyn_counters_t;
typedef uint8_t dir;


/************************** CMetaDataPerCounter ************************************/

/*class that holds the meta data per counter*/
class CMetaDataPerCounter {
public:
    CMetaDataPerCounter(std::string counter_name, std::string help, std::string units="", counter_info_t info=scINFO, bool dump_zero=false, bool is_abs=false) {
        m_name = counter_name;
        m_help = help;
        m_units = units;
        m_info = info;
        m_is_abs = is_abs;
        m_dump_zero = dump_zero;
    }

public:
    std::string    m_name;
    std::string    m_help;
    std::string    m_units;
    counter_info_t m_info;
    bool           m_is_abs;
    bool           m_dump_zero;
};


/************************** meta_data_t ****************************************/


/*struct that holds counters meta data */
typedef struct{
    std::string group_name;
    std::vector<CMetaDataPerCounter> meta_data_per_counter;
}meta_data_t;



/************************** DpsDynSts *****************************************/


/*class that holds all the dps counters*/
class DpsDynSts {
public:

    /**
    * Add the dp's client and server sides counters
    *
    * @param dp_counters
    *   A pair that holds two pointers.
    *   The first one is a pointer to the client side counters.
    *   The second one is a pointer to the server side counters.
    */
    void add_dp_dyn_sts(dp_sts_t dp_counters) {
        m_dp_dyn_sts_vec[TCP_CLIENT_SIDE].push_back(dp_counters.first);
        m_dp_dyn_sts_vec[TCP_SERVER_SIDE].push_back(dp_counters.second);
    }

public:
    per_side_dp_sts_vec_t m_dp_dyn_sts_vec[TCP_CS_NUM];
};


/************************** cp_dyn_sts_group_args_t *******************************/

// The arguments of CDynStsCpGroup
typedef struct {
    /*The counters that the cp allocates*/
    uint64_t             *real_counters;
    /*the number of counters per dp*/
    size_t                size;
    /*the counters group name*/
    std::string           group_name;
    /*the dps counters*/
    per_side_dp_sts_vec_t per_side_dp_sts;
}cp_dyn_sts_group_args_t;



/************************** CDynStsCpGroup *****************************************/


/* a cp group of counters per side*/
class CDynStsCpGroup {
public:
    CDynStsCpGroup(cp_dyn_sts_group_args_t* dyn_sts_group_args){
        m_dyn_sts_group_args = (*dyn_sts_group_args);
    }

    /**
    * clear the real_counters inside m_dyn_sts_group_args
    */
    void clear();

    /**
    * accumulate the real_counters from the dps counters.
    *
    * @param sts
    *   A pointer to CDynStsCpGroup object which we accumulates the counters.
    */
    void accumulate(CDynStsCpGroup* sts);

    /**
    * update the real counters from the dps counters
    *
    */
    void update();

    /**
    * Clears the valuse of the dps dynamic counters
    */
    void clear_dps_dyn_counters();

    cp_dyn_sts_group_args_t m_dyn_sts_group_args;
};



/************************** CCpDynStsInfra *****************************************/


/*infra for adding and removing dynamic counters */
class CCpDynStsInfra {
public:
    CCpDynStsInfra(uint16_t num_cores);

    /**
    * Add dynamic counters to TRex.
    * 
    * @param meta_data
    *   The meta data of the counters. 
    *
    * @param dps_counters
    *   The dps_counters holds all of the dps counters of both client and server side.
    *
    * @param profile_id
    *   The profile id to which we want to add dynamic counters to
    *
    * @return bool
    *   true in case of success otherwise false
    */
    bool add_counters(const meta_data_t* meta_data, DpsDynSts* dps_counters, std::string profile_id=DEFAULT_ASTF_PROFILE_ID);

    /**
    * Removes dynamic counters from TRex
    * Notice: Only counters that were added using add_counters function can be removed.
    *
    * @param meta_data
    *   The meta data of the counters. 
    *
    * @param profile_id
    *   The profile id to which we want to remove dynamic counters from
    *
    * @return bool
    *   true in case of success otherwise false
    */
    bool remove_counters(const meta_data_t* meta_data, std::string profile_id=DEFAULT_ASTF_PROFILE_ID);

    /**
    * Gets a vector with pointers to CSTTCp that holds the dynamic counters.
    * 
    * @return 
    *   vector of CSTTCp* that holds the dynamic counters
    */
    std::vector<CSTTCp *> get_dyn_sts_list();

    /**
    * Accumulate the dynamic counters from CSTTCp pointer
    *
    * @param clear
    *   whether to clear the sum counters before accumulate
    *
    * @param lp
    *   pointer to CSTTCp object that holds dynamic counters
    */
    void accumulate_total_dyn_sts(bool clear, CSTTCp* lp);

    /**
    *  Check if the profile id exist in the cp infra
    *
    * @param profile_id
    *   the profile id as string
    *
    * @return
    *   bool: true if the profile exist and false otherwise
    */
    bool is_valid_profile(std::string profile_id);

    /**
    *  Clear the dps_dyn_counters of the profile if exists.
    *
    * @param profile_id
    *   the profile id as string
    *
    */
    void clear_dps_dyn_counters(std::string profile_id=DEFAULT_ASTF_PROFILE_ID);

    /**
    *  Gets pointer to the profile CSTTCp object.
    *  Notice the profile must be the deafult profile or profile that dynamic counters were added
    *  to it.
    *
    * @param profile_id
    *   the profile id as string
    *
    * @return
    *   pointer to the profile CSTTCp object if the profile exists, otherwise nullptr
    */
    CSTTCp* get_profile_sts(std::string profile_id);

    ~CCpDynStsInfra();

public:
    CSTTCp *              m_total_sts;
private:
    uint16_t              m_num_cores;
    /*a set that includes the existing counters group names*/
    group_sts_set_t       m_group_sts_set;
    /*pointers to the real counters that were allocated per CSTTCp object*/
    dyn_counters_t        m_dyn_real_counters[TCP_CS_NUM];
    profiles_map_t        m_profiles_map;
};
#endif