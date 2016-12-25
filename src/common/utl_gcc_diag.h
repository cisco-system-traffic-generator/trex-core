#ifndef UTL_GCC_DIAG_H
#define UTL_GCC_DIAG_H

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


#define GCC_DIAG_STR(s) #s
#define GCC_DIAG_JOINSTR(x,y) GCC_DIAG_STR(x ## y)
# define GCC_DIAG_DO_PRAGMA(x) _Pragma (#x)
# define GCC_DIAG_PRAGMA(x) GCC_DIAG_DO_PRAGMA(GCC diagnostic x)
#define GCC_DIAG_OFF(x) GCC_DIAG_PRAGMA(push) \
    GCC_DIAG_PRAGMA(ignored GCC_DIAG_JOINSTR(-W,x))
#define GCC_DIAG_ON() GCC_DIAG_PRAGMA(pop)

#define UNSAFE_CONTAINER_OF_PUSH GCC_DIAG_OFF(invalid-offsetof)
#define UNSAFE_CONTAINER_OF_POP  GCC_DIAG_ON()



#endif
