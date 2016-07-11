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


#include "GREHeader.h"

void GREHeader::dump(FILE*  fd)
{
    fprintf(fd, "GREHeader\n");
    fprintf(fd, "Checksum enabled: %d, Key enabled: %d, "
        "Sequence number enabled: %d\n", isChecksumEnabled(), isKeyEnabled(),
	isSequenceNumberEnabled());
    fprintf(fd, "Protocol type: 0x%04x \n", getProtocolType());
    fprintf(fd, "Key: 0x%04x\n", getKey());
}
