/*
  Ido Barnea
  Cisco Systems, Inc.
*/

/*
  Copyright (c) 2016-2016 Cisco Systems, Inc.

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

class Cxl710Parser {
 public:
    Cxl710Parser();
    void reset();
    int parse(uint8_t *pkt, uint16_t len);
    bool is_fdir_supported() {return m_fdir_supported == true;};
    int get_ip_id(uint16_t &ip_id);
    int set_ip_id(uint16_t ip_id);
    int get_l4_proto(uint8_t &proto);
    int test();

 private:
    IPHeader *m_ipv4;
    bool m_fdir_supported;
    uint8_t m_l4_proto;
};
