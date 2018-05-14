#ifndef ASTF_JSON_VALIDATOR_H
#define ASTF_JSON_VALIDATOR_H

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
#include <json/json.h>
#include <valijson/adapters/jsoncpp_adapter.hpp>
#include <valijson/utils/jsoncpp_utils.hpp>
#include <valijson/schema.hpp>
#include <valijson/schema_parser.hpp>
#include <valijson/validator.hpp>
#include <valijson/validation_results.hpp>



using valijson::adapters::JsonCppAdapter;
using valijson::Validator;
using valijson::Schema;
using valijson::SchemaParser;
using valijson::ValidationResults;
using valijson::SchemaParser;
using valijson::ConstraintBuilder;
using valijson::constraints::Constraint;


class CAstfJsonValidator {
public:
    CAstfJsonValidator(){
        m_schema_adapter = NULL;
    }
    
    bool Create(std::string input_schema_file);
    void Delete();

    bool validate_profile_file(std::string  input_json_filename, 
                               std::string & err);

    bool validate_profile(Json::Value  profile, 
                          std::string & err);

private:

    bool validate_program(Json::Value  profile,
                          std::string & err);
    int get_max_buffer(Json::Value  program);
    bool validate_ip_gen(Json::Value  ip_gen,
                        std::string & err);



private:
    std::string  m_input_schema_file;
    Json::Value  m_schema_doc;
    Schema       m_schema;
    SchemaParser m_parser;
    JsonCppAdapter * m_schema_adapter;
};


#endif
