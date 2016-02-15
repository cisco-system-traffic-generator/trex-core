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

#include "trex_rpc_zip.h"
#include <zlib.h>
#include <arpa/inet.h>
#include <iostream>

bool
TrexRpcZip::is_compressed(const std::string &input) {
    /* check for minimum size */
    if (input.size() < sizeof(header_st)) {
        return false;
    }

    /* cast */
    const header_st *header = (header_st *)input.c_str();

    /* check magic */
    uint32_t magic = ntohl(header->magic);
    if (magic != G_HEADER_MAGIC) {
        return false;
    }

    return true;
}

bool
TrexRpcZip::uncompress(const std::string &input, std::string &output) {

    /* sanity check first */
    if (!is_compressed(input)) {
        return false;
    }

    /* cast */
    const header_st *header = (header_st *)input.c_str();

    /* original size */
    uint32_t uncmp_size = ntohl(header->uncmp_size);

    /* alocate dynamic space for the uncomrpessed buffer */
    Bytef *u_buffer = new Bytef[uncmp_size];
    if (!u_buffer) {
        return false;
    }

    /* set the target buffer size */
    uLongf dest_len = uncmp_size;

    /* try to uncompress */
    int z_err = ::uncompress(u_buffer,
                             &dest_len,
                             (const Bytef *)header->data,
                             (uLong)input.size() - sizeof(header_st));

    if (z_err != Z_OK) {
        delete [] u_buffer;
        return false;
    }

    output.append((const char *)u_buffer, dest_len);

    delete [] u_buffer;
    return true;
}


bool
TrexRpcZip::compress(const std::string &input, std::string &output) {

    /* get a bound */
    int bound_size = compressBound((uLong)input.size()) + sizeof(header_st);

    /* alocate dynamic space for the uncomrpessed buffer */
    char *buffer = new char[bound_size];
    if (!buffer) {
        return false;
    }

    header_st *header = (header_st *)buffer;
    uLongf destLen    = bound_size;
    
    int z_err = ::compress((Bytef *)header->data,
                           &destLen,
                           (const Bytef *)input.c_str(),
                           (uLong)input.size());

    if (z_err != Z_OK) {
        delete [] buffer;
        return false;
    }

    /* terminate string */
    header->data[destLen] = 0;

    /* add the header */
    header->magic      = htonl(G_HEADER_MAGIC);
    header->uncmp_size = htonl(input.size());
    
    output.append((const char *)header, bound_size);

    delete [] buffer;

    return true;    
}
