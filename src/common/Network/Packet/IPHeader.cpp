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

#include "IPHeader.h"


char * IPHeader::Protocol::interpretIpProtocolName(uint8_t argType)
{
    switch (argType)
    {
    case TCP:
        return (char *)"TCP";
        break;
    case UDP:
        return (char *)"UDP";
        break;
    case IP:
        return (char *)"IP";
        break;
    case ICMP:
        return (char *)"ICMP";
        break;
    case ESP:
        return (char *)"ESP";
        break;
    case AH:
        return (char *)"AH";
        break;
    case IGMP:
        return (char *)"IGMP";
        break;
    default:
        return (char *)NULL;
        break;
    }
}

void IPHeader::dump(FILE *fd)
{
    fprintf(fd, "\nIPHeader");
    fprintf(fd, "\nSource 0x%.8lX, Destination 0x%.8lX, Protocol 0x%.1X",
            getSourceIp(), getDestIp(), getProtocol());
    fprintf(fd, "\nTTL : %d, Id : 0x%.2X, Ver %d, Header Length %d, Total Length %d",
            getTimeToLive(), getId(), getVersion(), getHeaderLength(), getTotalLength());
    if(isFragmented())
    {
        fprintf(fd,"\nIsFirst %d, IsMiddle %d, IsLast %d, Offset %d",
                isFirstFragment(), isMiddleFragment(), isLastFragment(), getFragmentOffset());
    }
    else
    {
        fprintf(fd, "\nDont fragment %d", isDontFragment());
    }
    fprintf(fd, "\n");
}



