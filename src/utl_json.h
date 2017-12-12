#ifndef UTL_JSON_H
#define UTL_JSON_H
/*
 Hanoh Haim
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
#include <string>
#include <type_traits>

namespace details {
inline std::string add_json_val(std::string const& name, std::string const& val, bool last){
    std::string s = "\"" + name + "\":" + val;
    if (!last)
        s += ",";
    return s;
}
}

inline std::string add_json(std::string const& name, std::string const& val, bool last=false){
    return details::add_json_val(name, "\"" + val + "\"", last);
}

template <typename N, typename = typename std::enable_if<std::is_integral<N>::value || std::is_floating_point<N>::value>::type>
inline std::string add_json(std::string const& name, N counter, bool last=false){
    return details::add_json_val(name, std::to_string(counter), last);
}

#endif

