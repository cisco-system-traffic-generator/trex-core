#include <map>
#include "ibis_types.h"

class IbisMadNames {
private:
    typedef std::map < u_int32_t, std::map <u_int32_t, const char* > > mad_names_map;
    IbisMadNames();
    static IbisMadNames& getInstance() {
        static IbisMadNames instance;
        return instance;
    }

    IbisMadNames(IbisMadNames const&);
    IbisMadNames& operator=(IbisMadNames const&);

    mad_names_map m_mad_names;
    u_int8_t m_maxLenMadName;
public:
    static const char* getMadName(u_int32_t class_id, u_int32_t attribute_id);
    static u_int8_t getMaxMadNameLen();
};
