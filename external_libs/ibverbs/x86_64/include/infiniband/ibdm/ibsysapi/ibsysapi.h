/*
 * Copyright (c) 2004-2010 Mellanox Technologies LTD. All rights reserved.
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


#ifndef IBDM_SYS_API_H
#define IBDM_SYS_API_H

#ifdef __cplusplus
extern "C" {
#endif

/* verbose mask */
enum ibSysVerboseLevel {
    IBSYS_QUITE = 0,
    IBSYS_ERROR = 1,
    IBSYS_WARN  = 2,
    IBSYS_INFO  = 4,
    IBSYS_DEBUG = 8
};


//typedef sysapi_t* sysapi_handler;
typedef struct sysapi* sysapi_handler_t;


/*create an ib sysapi handler */
sysapi_handler_t ibSysCreate(void);

/* destroy the ib sysapi handler */
void ibSysDestroy(sysapi_handler_t ibSysHandler);

/* read the IBNL with specific configuration and prepare the local fabric */
int ibSysInit(sysapi_handler_t ibSysHandler,
        char *sysType, char *cfg);

/* query DR paths to a node  - paths are pre allocated */
/* return the number of filled paths in numPaths */
/* end of DR is a -1 */
int ibSysGetDrPathsToNode(sysapi_handler_t ibSysHandler,
        char *fromNode, char *toNode,
        int *numPaths, int**drPaths);

/* query all node names in the system */
int ibSysGetNodes(sysapi_handler_t ibSysHandler,
        int *numNodes, const char **nodeNames);

/* query name of front pannel port of a node port */
int ibSysGetNodePortSysPort(sysapi_handler_t ibSysHandler,
        char *nodeName, phys_port_t portNum,
        const char **sysPortName);

/* query node name and port given front pannel port name */
int ibSysGetNodePortOnSysPort(sysapi_handler_t ibSysHandler,
        char *sysPort,
        const char**nodeName, int *portNum);

/* query other side of the port node port */
int ibSysGetRemoteNodePort(sysapi_handler_t ibSysHandler,
        char *nodeName, phys_port_t portNum,
        const char **remNode, int *remPortNum);

#ifdef __cplusplus
}                   /* extern "C" */
#endif          /* ifdef __cplusplus */


#endif /* IBDM_SYS_API_H */
