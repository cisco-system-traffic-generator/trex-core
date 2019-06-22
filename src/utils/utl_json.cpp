#include "utl_json.h"


std::string pretty_json_str(const std::string &json_str) {
    Json::Reader reader;
    Json::Value  value;

    /* basic JSON parsing */
    bool rc = reader.parse(json_str, value, false);
    if (!rc) {
        /* duplicate the soruce */
        return json_str;
    }

    /* successfully parsed */
    Json::StyledWriter writer;
    return writer.write(value);
}


std::string pretty_json_str(const Json::Value & value) {
    /* successfully parsed */
    Json::StyledWriter writer;
    return writer.write(value);
}

void dump_json(Json::Value & value,FILE *fd) {
    fprintf(fd,"%s",pretty_json_str(value).c_str());
}



