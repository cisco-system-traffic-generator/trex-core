/*
 * Copyright (c) 2004-2021 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 2021-2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 *
 * This software is available to you under the terms of the
 * OpenIB.org BSD license included below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

/*
 * This file provides a loosely coupled (LC) API between ibdiagnet and its plugins.
 * The API is in standard C, since it needs to support a binary compatibility,
 * so no STL types here are allowed.
 * PF - stands for plugin framework.
 */
#ifndef _IBDIAGAPP_LC_PLUGIN_API_H_
#define _IBDIAGAPP_LC_PLUGIN_API_H_

#define IBDIAGNET_LC_PLUGIN_API_VERSION_MAJOR 1
#define IBDIAGNET_LC_PLUGIN_API_VERSION_MINOR 0

#include <infiniband/ibis/ibis_clbck.h>

#ifdef __cplusplus
extern "C" {
#endif

struct PF_PlatformServices;

typedef struct PF_ObjectParams
{
  const char  *objectType;
  const struct PF_PlatformServices * platformServices;
} PF_ObjectParams;

typedef struct PF_PluginAPI_Version
{
  u_int32_t major;
  u_int32_t minor;
} PF_PluginAPI_Version;

typedef void * (*PF_CreateFunc)(PF_ObjectParams *);
typedef int32_t (*PF_DestroyFunc)(void *);

typedef struct PF_RegisterParams
{
  PF_PluginAPI_Version version;
  PF_CreateFunc createFunc;
  PF_DestroyFunc destroyFunc;
  u_int64_t plugin_handle; //a handle that represents the plugin in ibdiagnet
                           //that was presented in PF_PlatformServices
} PF_RegisterParams;

typedef int32_t (*PF_RegisterFunc)(const char * objectType,
                                   const PF_RegisterParams * params);
typedef int32_t (*PF_InvokeServiceFunc)(const char * serviceName,
                                        void * serviceParams);

typedef struct PF_PlatformServices
{
  PF_PluginAPI_Version version; //version that ibdiagnet uses
  PF_RegisterFunc registerObject; //function to register plugin objects
  PF_InvokeServiceFunc invokeService; //function to get ibdiagnet services
  u_int64_t plugin_handle; //a handle that represents the plugin in ibdiagnet
} PF_PlatformServices;

typedef int32_t (*PF_ExitFunc)();

typedef PF_ExitFunc (*PF_InitFunc)(const PF_PlatformServices *);

#ifndef PLUGIN_API
  #ifdef WIN32
    #define PLUGIN_API __declspec(dllimport)
  #else
    #define PLUGIN_API
  #endif
#endif

extern
#ifdef  __cplusplus
"C"
#endif
PLUGIN_API PF_ExitFunc PF_initPlugin(const PF_PlatformServices * params);

#ifdef  __cplusplus
}
#endif

//types of nodes in ib
typedef enum {
    PL_IB_UNKNOWN_NODE_TYPE,
    PL_IB_CA_NODE,
    PL_IB_SW_NODE
} plIBNodeType_t;

//The base class for plugin's object
class IObjectLCPlugin {
public:
    virtual ~IObjectLCPlugin() {};
    virtual int32_t InvokeService(const char* service, void* params) = 0;
};

//Plugin's services for application
typedef struct runCheckParams {
    const char** results;//out parameter - fill result lines to print
    u_int32_t *num_lines;//out parameter - fill how many lines
} runCheckParams_t;

//Application services for plugin by ibdiagnet
//Ibis::MadGetSet
typedef struct madGetSetParams {
    u_int16_t lid;
    u_int32_t d_qp;
    u_int8_t sl;
    u_int32_t qkey;
    u_int8_t mgmt_class;
    u_int8_t method;
    u_int16_t attribute_id;
    u_int32_t attribute_modifier;
    u_int8_t data_offset;
    data_func_set_t class_data;
    data_func_set_t attribute_data;
    const clbck_data_t *p_clbck_data;
} madGetSetParams_t;


typedef struct portObject {
    u_int64_t nodeGuid;
    u_int64_t portGuid;
    u_int8_t  num;
    u_int64_t peerPortGuid;
    u_int8_t  peerNum;
    u_int8_t  type;
    u_int16_t lid;
    u_int32_t gmpCapabilityMask[4];
    u_int32_t smpCapabilityMask[4];
    char      name[50];
} plPortObject_t;

//Application services for ibdiagnet by plugin
//"AddPorts"
typedef struct addPortsParams {
    const plPortObject_t *ports;
    u_int32_t             ports_num;
} addPortsParams_t;

#endif //_IBDIAGAPP_LC_PLUGIN_API_H_
