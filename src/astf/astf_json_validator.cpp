/*
 Hanoh Haim
 Cisco Systems, Inc.
*/

/*
Copyright (c) 2015-2017 Cisco Systems, Inc.

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
#include "astf_json_validator.h"
#include "inet_pton.h"

using std::cerr;
using std::endl;


bool CAstfJsonValidator::Create(std::string input_schema_file){
    m_input_schema_file = input_schema_file;
    if (!valijson::utils::loadDocument(m_input_schema_file, m_schema_doc)) {
        return(false);
    }

    m_schema_adapter = new JsonCppAdapter(m_schema_doc);
    m_parser.populateSchema(*m_schema_adapter, m_schema);
    return(true);
}

int CAstfJsonValidator::get_max_buffer(Json::Value  program){

    int max_val=-1;
    Json::Value commands=program["commands"];
    int i; 
    for (i=0; i<commands.size(); i++) {
        Json::Value cmd=commands[i];
        Json::Value buf_index=cmd["buf_index"];
        if (buf_index != Json::nullValue){
            if ( max_val <buf_index.asInt() ){
                max_val =buf_index.asInt();
            }
        }
    }
    return(max_val);
}


bool CAstfJsonValidator::validate_ip_gen(Json::Value  ip_gen,
                                        std::string & err){

    std::string a[]={"ip_start","ip_end","ip_offset"};
    for (const std::string &key : a) {
        std::string s=ip_gen[key].asString();

        int rc;
        uint32_t ip_num;
        rc = my_inet_pton4(s.c_str(), (unsigned char *)&ip_num);
        if (!rc) {
            std::stringstream ss;
            ss << "Validation failed : Bad IP address " << s <<std::endl;
            err = ss.str();
            return(false);
        }
    }
    return(true);;
}


bool CAstfJsonValidator::validate_program(Json::Value  profile,
                                          std::string & err){
    int program_size = profile["program_list"].size() ;
    if (program_size < 1) {
        err = "Validation failed: profile must have at least one programs";
        return(false);
    }

    int max_temp=-1;
    int ip_gen_max=-1;
    int i;
    for (i=0; i<profile["templates"].size(); i++) {
        Json::Value temp= profile["templates"][i];
        int pinx_c=temp["client_template"]["program_index"].asInt();
        int pinx_s=temp["server_template"]["program_index"].asInt();
        int ip_gen_c=temp["client_template"]["ip_gen"]["dist_client"]["index"].asInt();
        int ip_gen_s=temp["client_template"]["ip_gen"]["dist_server"]["index"].asInt();
        max_temp  = std::max(max_temp,std::max(pinx_c,pinx_s));
        ip_gen_max = std::max(ip_gen_max,std::max(ip_gen_c,ip_gen_s));
    }

    /* check #templates */
    if ( max_temp >= profile["program_list"].size() ) {
        std::stringstream ss;
        ss << "Validation failed : max program index is " << max_temp << " bigger than the number of programs " << profile["program_list"].size() <<std::endl;
        err = ss.str();
        return(false);
    }
    /* check # ip_gen */
    if ( ip_gen_max >= profile["ip_gen_dist_list"].size() ) {
        std::stringstream ss;
        ss << "Validation failed : max ip generator index is " << ip_gen_max << " bigger than the number of generators " << profile["ip_gen_dist_list"].size() <<std::endl;
        err = ss.str();
        return(false);
    }

    /* check # buffers */
    int max_buffer=-1;
    for (i=0; i<program_size; i++) {
        Json::Value program= profile["program_list"][i];
        max_buffer=std::max(max_buffer,get_max_buffer(program));
    }

    /* check buffer size */
    if ( max_buffer >= profile["buf_list"].size() ) {
        std::stringstream ss;
        ss << "Validation failed : max buffer size is " << max_buffer << " bigger than " << profile["buf_list"].size() <<std::endl;
        err = ss.str();
        return(false);
    }

    /* validate generator IPs */
    for (i=0; i<profile["ip_gen_dist_list"].size(); i++) {
        Json::Value ip_gen= profile["ip_gen_dist_list"][i];
        if (!validate_ip_gen(ip_gen,err)){
            return(false);
        }
    }

    return(true);
}


bool CAstfJsonValidator::validate_profile(Json::Value  profile, 
                                          std::string & err){

    Validator validator;
    JsonCppAdapter myTargetAdapter(profile);

    ValidationResults results;
    if (!validator.validate(m_schema, myTargetAdapter, &results)) {
        std::stringstream ss;
        ss << "Validation failed." << std::endl;
        ValidationResults::Error error;
        unsigned int errorNum = 1;
        while (results.popError(error)) {
            ss << "Error #" << errorNum << std::endl;
            ss << "  ";
            for (const std::string &contextElement : error.context) {
                ss << contextElement << " ";
            }
            ss << std::endl;
            ss << "    - " << error.description << std::endl;
            ++errorNum;
        }
        err = ss.str();
        return(false);
    }
    return (validate_program(profile,err));
}

bool CAstfJsonValidator::validate_profile_file(std::string  input_json_filename, 
                                               std::string & err){
    Json::Value myTargetDoc;

    if (!valijson::utils::loadDocument(input_json_filename, myTargetDoc)) {
        std::stringstream ss;
        ss << "ERROR loading "<< input_json_filename<< "  \n";
        err = ss.str();
        return(false);
    }
    return (validate_profile(myTargetDoc, err));
}

void CAstfJsonValidator::Delete(){
    if (m_schema_adapter) {
        delete  m_schema_adapter;
        m_schema_adapter= NULL;
    }
}




