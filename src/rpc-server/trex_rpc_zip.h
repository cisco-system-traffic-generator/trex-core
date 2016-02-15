/*
 Itay Marom
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

class TrexRpcZip {
public:

    /**
     * return true if message is compressed
     * 
     */
    static bool is_compressed(const std::string &input);

    /**
     * uncompress an 'input' to 'output'
     * on success return true else returns false
     */
    static bool uncompress(const std::string &input, std::string &output);

    /**
     * compress 'input' to 'output'
     * on success return true else returns false
     */
    static bool compress(const std::string &input, std::string &output);

private:

    /**
     * packed header for reading binary compressed messages
     * 
     * @author imarom (15-Feb-16)
     */
    struct header_st {
        uint32_t    magic;
        uint32_t    uncmp_size;
        char        data[0];
    } __attribute__((packed));

  
    static const uint32_t G_HEADER_MAGIC = 0xABE85CEA;
};

