#include "trex_rx_rpc_tunnel.h"
#include <assert.h>


CRpcDispatch::~CRpcDispatch(){
    m_map.clear();
}

rpc_method_cb_t CRpcDispatch::find_func_by_name(const string & json_name){
    int_rpc_map_it_t it = m_map.find(json_name);
    if (it == m_map.end()) {
        return NULL;
    } else {
        return it->second;
    }
}

void CRpcDispatch::register_func(const string & json_name,rpc_method_cb_t  cb){
    assert(find_func_by_name(json_name)==NULL);
    m_map.insert(int_rpc_map_pair_t(json_name, cb));
}



/* update how many command executed and how many error are reported */
void CRpcTunnelBatch::update_cmd_count(uint32_t total_commands,
                                       uint32_t err_commands){
}

trex_rpc_cmd_rc_e  CRpcTunnelBatch::_run(const Json::Value &params,
                                         Json::Value &result) {
    assert(0);
    return(TREX_RPC_CMD_OK);
}

void CRpcTunnelBatch::register_func(const string & json_name,
                                    rpc_method_cb_t cb){
    m_rpc_dispatch.register_func(json_name,cb);
}

trex_rpc_cmd_rc_e CRpcTunnelBatch::run_batch(const Json::Value &commands,
                                             Json::Value &results){
    assert(commands.isArray());

    uint32_t index=0;
    uint32_t index_err=0;
    for (auto command : commands) {
        update_cmd_count(index,index_err);

        std::string method_name = command["method"].asString();
        Json::Value &params = command["params"];
        rpc_method_cb_t cb = m_rpc_dispatch.find_func_by_name(method_name);
        if (cb== NULL){
            std::stringstream err;
            err << "Method " << method_name << " not registered";
            results[index]["error"]=err.str();
            index_err++;
        }else{
            try {
                Json::Value result;
                cb(params,result);
                /* save space for common case */
                if (result == Json::nullValue) {
                    results[index] = result;
                }else{
                    results[index]["result"] = result;
                }
            } catch (const TrexRpcException &e) {
                results[index]["error"]= e.what();
                index_err++;
            }
        }
        index++;
    }
    return (TREX_RPC_CMD_OK);
}




