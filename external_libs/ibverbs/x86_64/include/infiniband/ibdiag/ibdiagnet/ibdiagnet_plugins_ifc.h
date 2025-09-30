/*
 * Copyright (c) 2004-2021 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 2021-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 *
 * This software is available to you under the terms of the
 * OpenIB.org BSD license included below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */


#ifndef _IBDIAGNET_PLUGINS_INTERFACE_H_
#define _IBDIAGNET_PLUGINS_INTERFACE_H_

#include <stdlib.h>

#include <infiniband/ibdiag/ibdiag.h>
#include <infiniband/misc/tool_trace/tool_trace.h>
#include <misc/cmdparser/cmdparser.h>
#include <infiniband/ibdiag/ibdiag_log.h>

/******************************************************/
extern bool found_lids_errors;
extern bool found_links_errors;

/* general define */
#define DB_FILE                 "ibdiagnet2.db_csv"

#define MAX_SPACES_FABRIC_SUMMARY_SHORT   24
#define MAX_SPACES_FABRIC_SUMMARY_LONG    35

/******************************************************/
/* log Macros */
#ifdef DEBUG
    #define IBDIAGNET_LOG(level, fmt, ...) TT_LOG(TT_LOG_MODULE_IBDIAGNET, level, fmt, ## __VA_ARGS__);
    #define IBDIAGNET_ENTER TT_ENTER( TT_LOG_MODULE_IBDIAGNET );
    #define IBDIAGNET_RETURN(rc) { TT_EXIT( TT_LOG_MODULE_IBDIAGNET );  \
                                return (rc); }
    #define IBDIAGNET_RETURN_VOID { TT_EXIT( TT_LOG_MODULE_IBDIAGNET );   \
                                    return; }
#else           /*def DEBUG */
    #define IBDIAGNET_LOG(level, fmt, ...)
    #define IBDIAGNET_ENTER
    #define IBDIAGNET_RETURN(rc) return (rc);
    #define IBDIAGNET_RETURN_VOID return;
#endif          /*def DEBUG */


/******************************************************/
/* common functions */
inline string generate_file_name(string path, const char *base_name) {
    return(path + base_name);
}
inline bool is_non_neg_num(std::string& str) {
    std::string::const_iterator it = str.begin();
    while (it != str.end() && std::isdigit(*it)) ++it;
    return !str.empty() && it == str.end();
}

inline bool is_hex_num(string const& s)
{
    return s.compare(0, 2, "0x") == 0 &&
           s.size() > 2 &&
           s.find_first_not_of("0123456789abcdefABCDEF", 2) == string::npos;
}

int copy_file(const char SRC[], const char DST[], string& errors);
list_string get_dir_files(string dir, list_string files_types);

/******************************************************/
int check_if_can_send_mads_by_lid(IN IBDiag *p_ibdiag,
        OUT bool& result);

/******************************************************/
/*
 * Stage is one stage. The program will contain
 * an array of stages.
 * Each stage has 3 steps: prepare / run / run_check
 * Also each stage can be active / skipped / finished
 * Each stage is also holds handler to db file. There
 * he can prints errors or data results from its run.
 */
class Stage {
private:
    enum {ACTIVE, SKIPPED, FINISHED} stage_status;
protected:
    ////////////////////
    //members
    ////////////////////
    IBDiag *p_ibdiag;
    u_int32_t num_warnings;
    u_int32_t num_errors;
    string generated_files_name;
    string stage_name;

    unsigned int *p_num_of_errors_to_screen;
    string *p_base_path;
    CSVOut *p_csv_out;

    static u_int32_t max_spaces_fabric_summary;

public:
    struct {
        struct timespec real;
        struct timeval  user;
        struct timeval  sys;
    } m_perf_time_info;

protected:
    ////////////////////
    //methods
    ////////////////////
    void AddGeneratedFileName(string desc, string name);
    void ParseIBDMOutput(string& output);

public:
    ////////////////////
    //methods
    ////////////////////
    Stage(string name, IBDiag *p_ibdiag_obj);
    virtual ~Stage() {}

    const std::string & GetName() const { return stage_name; }

    virtual const char* GetLastError() { return(this->p_ibdiag->GetLastError()); }

    IBDiag * GetIBDiag() { return p_ibdiag; }

    string GetSummaryLine();
    string GetFilesLine();
    virtual bool GetFabricSummarySection(stringstream &summary_str);

    inline bool IsFinished() { return(this->stage_status == FINISHED); }
    inline void MarkStageAsFinished() { this->stage_status = FINISHED; }
    virtual bool IsSkipped() { return(this->stage_status == SKIPPED); }
    inline void MarkStageAsSkipped() { this->stage_status = SKIPPED; }
    virtual bool IsActive() { return(this->stage_status == ACTIVE); }
    inline void MarkStageAsActive() { this->stage_status = ACTIVE; }
    void MarkStage(bool is_active);

    virtual int CleanResources();
    virtual int Prepare() = 0;
    virtual int RetrieveInfo() = 0;
    virtual int RunCheck() = 0;

    //Returns: 0 - success / 1 - failure / 2 - check fail
    int AnalyzeCheckResults(list_p_fabric_general_err& errors_list,
            string check_name,
            int check_rc,
            int err_code_value_for_report,
            u_int32_t& num_errors,
            u_int32_t& num_warnings,
            bool only_warning = false);

    inline void SetNumErrsToScreen(unsigned int *p_num) { this->p_num_of_errors_to_screen = p_num; }
    inline void SetBasePath(string *p_path) { this->p_base_path = p_path; }
    inline void SetCSVOut(CSVOut *p_csv_out) { this->p_csv_out = p_csv_out; }
};


/******************************************************/
class Plugin : public Stage, public CommandLineRequester {
private:

protected:
    ////////////////////
    //members
    ////////////////////
    string name;
    string last_error;

public:
    ////////////////////
    //methods
    ////////////////////
    Plugin(string plugin_name, IBDiag *p_ibdiag_obj) :
        Stage(plugin_name, p_ibdiag_obj), CommandLineRequester(plugin_name) {
        this->name = "Plugin: ";
        this->name += plugin_name;
    }
    virtual ~Plugin() {}

    const char* GetLastError();
    void SetLastError(const char *fmt, ...);

    virtual void Init() = 0;
};


/* factories functions for the plugin */
typedef Plugin* create_plugin_t(IBDiag *p_ibdiag_obj);
typedef void destroy_plugin_t(Plugin*);


#endif          /* not defined _IBDIAGNET_PLUGINS_INTERFACE_H_ */
