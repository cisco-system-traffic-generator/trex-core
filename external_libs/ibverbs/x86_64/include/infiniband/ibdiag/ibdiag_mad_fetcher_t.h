/*
 * Copyright (c) 2025-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 *
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

/**
 * @file ibdiag_mad_fetcher_t.h
 * @brief Base class for fetching Management Datagram (MAD) information from InfiniBand devices
 *
 * @details
 * This header file defines the MadFetcher base class which provides a framework for sending and 
 * receiving Management Datagrams (MADs) in an InfiniBand fabric. The class supports both Subnet
 * Management Packets (SMP) and General Management Packets (GMP) operations.
 *
 * The MadFetcher class implements:
 * - Flexible node targeting (HCAs, switches, routers, GPUs)
 * - Per-node and per-port operation modes
 * - Direct route and LID-based addressing
 * - Progress tracking with optional visual feedback
 * - Comprehensive error handling and reporting
 * - Node capability validation
 * - Special node/port handling
 * - Configurable validation behaviors
 *
 * Key components:
 * - Address class for unified direct route and LID addressing
 * - Progress tracking through ProgressBar integration
 * - Error collection and reporting mechanisms
 * - Capability mask validation for both GMP and SMP
 * - Template pattern for derived class specialization
 *
 * Usage:
 * Derived classes must implement:
 * - request(): For MAD formatting and transmission
 * - response(): For MAD response processing
 *
 * @note This class is part of the NVIDIA InfiniBand Diagnostic tools suite
 * @see ibdiag_mad_fetcher.cpp for implementation details
 * @see IBDiag for fabric management capabilities
 */


#pragma once

#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>

#include <infiniband/ibis/ibis_clbck.h>
#include <infiniband/misc/ini_file/ini_file.h>
#include "infiniband/ibdiag/ibdiag_fabric_errs.h"

class IBDiag;
class IBNode;
class IBPort;
class ProgressBar;
class IBDMExtendedInfo;
class Ibis;

/**
 * @brief Base class for fetching Management Datagram (MAD) information from InfiniBand devices
 * 
 * @details
 * The MadFetcher class provides a robust framework for sending MAD requests to InfiniBand devices
 * and processing their responses. It supports both Subnet Management Packets (SMP) and General 
 * Management Packets (GMP) through a unified interface.
 *
 * Key features:
 * - Comprehensive node targeting (HCAs, switches, routers, GPUs)
 * - Flexible operation modes (per-node or per-port)
 * - Dual addressing schemes (direct route and LID-based)
 * - Real-time progress tracking with optional progress bar
 * - Sophisticated error handling and reporting
 * - Capability validation for target nodes
 * - Support for special nodes and ports
 * - Configurable validation behavior
 *
 * The class implements a template pattern where derived classes must implement:
 * - request(): Handles MAD formatting and transmission
 * - response(): Processes MAD responses
 *
 * @see IBDiag for fabric management capabilities
 * @see ibdiag_mad_fetcher.cpp for implementation details
 */
class MadFetcher {
    /**
     * @brief Operational flags controlling MadFetcher behavior
     *
     * @details
     * These options can be combined using bitwise OR to configure:
     * - Which types of nodes to target
     * - How to handle various error conditions
     * - Addressing and routing preferences
     * - Progress reporting
     *
     * The flags are organized into four bytes:
     * - First byte: Node type targeting (reserved bit 7)
     * - Second byte: Validation behavior (reserved bit 15)
     * - Third byte: Addressing behavior (reserved bits 22-23)
     * - Fourth byte: Operational modes (reserved bit 30-31)
     */
    public:
        typedef enum {
            None                    = 0,         /**< No options set - default behavior. When this flag is set, the fetcher will use standard default behavior for all operations. */
            Disable                 = 1 << 0,    /**< Completely disable this fetcher instance. When set, all operations will return success without executing any actual MAD requests. Useful for testing or dry-run scenarios. */

            // First byte controls node type targeting
            HCAs                    = 1 << 1,    /**< Target Host Channel Adapters (HCAs) - standard IB end nodes. This flag enables MAD requests to be sent to host channel adapters in the fabric. */
            SWs                     = 1 << 2,    /**< Target switches - IB fabric interconnect devices. Enables MAD requests to be sent to switch nodes in the fabric topology. */
            RTRs                    = 1 << 3,    /**< Target routers - devices that connect different subnets. Enables MAD requests to be sent to router nodes that bridge different IB subnets. */
            GPUs                    = 1 << 4,    /**< Target GPU devices - requires extended node info capability. Enables MAD requests to be sent to GPU nodes that support IB connectivity. */
            SpecialNodes            = 1 << 5,    /**< Allow targeting of special nodes like Aggregation Nodes (AN) agents. Enables MAD requests to special-purpose nodes that may not be standard HCAs or switches. */
            SpecialPort             = 1 << 6,    /**< Allow targeting of special ports like switch management ports. Enables MAD requests to special-purpose ports that may have unique characteristics. */

            /** Target all regular node types (HCAs, switches, routers, GPUs) but exclude special nodes. This is a convenience flag that combines all standard node type flags. */
            AllNodesExcludeSpecial  = HCAs | SWs | RTRs | GPUs,
            /** Target absolutely all node types including special nodes. This is the most inclusive targeting option, combining all node type flags. */
            AllNodes                = AllNodesExcludeSpecial | SpecialNodes,

            // Second byte controls validation behavior
            /** Skip data worthiness validation checks for ports (e.g. active state, training). When set, the fetcher will not validate port state before sending MAD requests. */
            IgnoreDataWorthy        = 1 << 8,

            /** Ignore whether nodes belong to the current sub-fabric partition. When set, MAD requests will be sent to nodes regardless of their partition membership. */
            IgnoreNodeScope         = 1 << 9,
            /** Ignore whether ports belong to the current sub-fabric partition. When set, MAD requests will be sent to ports regardless of their partition membership. */
            IgnorePortScope         = 1 << 10,
            /** Ignore all sub-fabric partition membership checks for both nodes and ports. Combines IgnoreNodeScope and IgnorePortScope for complete partition validation bypass. */
            IgnoreScope             = IgnoreNodeScope | IgnorePortScope,

            /** Log warning but continue processing when encountering null node pointers. When set, null node pointers will be logged but won't stop operation. */
            IgnoreNullNode          = 1 << 11,
            /** Log warning but continue processing when encountering null port pointers. When set, null port pointers will be logged but won't stop operation. */
            IgnoreNullPort          = 1 << 12,
            /** Log warnings but continue for any null node or port pointers. Combines IgnoreNullNode and IgnoreNullPort for complete null pointer tolerance. */
            IgnoreNull              = IgnoreNullNode | IgnoreNullPort,

            /** Stop processing when encountering null node pointers. When set, null node pointers will cause immediate operation termination. */
            StopOnNullNode          = 1 << 13,
            /** Stop processing when encountering null port pointers. When set, null port pointers will cause immediate operation termination. */
            StopOnNullPort          = 1 << 14,
            /** Stop processing when encountering any null node or port pointers. Combines StopOnNullNode and StopOnNullPort for strict null pointer checking. */
            StopOnNullNodePort      = StopOnNullNode | StopOnNullPort,

            // Third byte controls addressing behavior
            /** Log warning but continue when direct route path is null. When set, null direct routes will be logged but won't stop operation. */
            IgnoreNullDR            = 1 << 16,
            /** Log warning but continue when LID is invalid (zero). When set, zero LIDs will be logged but won't stop operation. */
            IgnoreZeroLid           = 1 << 17,
            /** Log warnings but continue for any invalid addressing (null DR or zero LID). Combines IgnoreNullDR and IgnoreZeroLid for complete address validation bypass. */
            IgnoreNullDestAddr      = IgnoreNullDR | IgnoreZeroLid,

            /** Stop processing when direct route path is null. When set, null direct routes will cause immediate operation termination. */
            StopOnNullDR            = 1 << 18,
            /** Stop processing when LID is invalid (zero). When set, zero LIDs will cause immediate operation termination. */
            StopOnZeroLid           = 1 << 19,
            /** Stop processing when encountering any invalid addressing. Combines StopOnNullDR and StopOnZeroLid for strict address validation. */
            StopOnNullDestAddr      = StopOnNullDR | StopOnZeroLid,

            /** Log warning but continue for nodes/ports missing required capabilities. When set, missing capabilities will be logged but won't stop operation. */
            IgnoreNullCap           = 1 << 20,
            /** Stop processing when nodes/ports are missing required capabilities. When set, missing capabilities will cause immediate operation termination. */
            StopOnNullCap           = 1 << 21,

            // Fourth byte controls operational modes
            /** Use Subnet Management Packets (SMP) instead of General Management Packets (GMP). When set, the fetcher will use SMP for all MAD requests. */
            SMP                     = 1 << 24,
            /** Process fabric on a per-port basis instead of per-node basis. When set, MAD requests will be sent to individual ports rather than entire nodes. */
            PerPort                 = 1 << 25,
            /** Allow targeting of switch port 0 (management port) in addition to data ports. When set, switch management ports can be included in operations. */
            AllowSwitchPortZero     = 1 << 26,
            /** Target only switch port 0 (management port), ignoring data ports. When set, only switch management ports will be targeted. */
            AllowSwitchPortZeroOnly = 1 << 27,
            /** Allow multiple errors to be reported for the same node. When set, the fetcher will report all errors encountered rather than stopping after the first error per node. */
            AllowRecurringErrors    = 1 << 28,
            /** Disable visual progress bar display during operations. When set, a progress bar will not be shown to indicate operation progress. */
            DisableProgressBar      = 1 << 29,

        } Options;

    /**
     * @brief Encapsulates addressing information for MAD requests
     *
     * @details
     * This class provides a unified interface for handling both direct route and LID-based addressing:
     * - Direct routes specify exact paths through the fabric topology (used for SMP MADs)
     * - LIDs provide local addressing within the subnet (used for regular MADs)
     *
     * The class maintains both addressing schemes simultaneously, allowing for:
     * - Fallback between addressing methods
     * - Validation of addressing information
     * - Easy conversion between addressing types
     *
     * @see resolve() for address resolution logic
     */
    public:
        class Address {
            public:
                const direct_route_t *  m_dr;    /**< Direct route path or nullptr */
                lid_t                   m_lid;    /**< Local ID or 0 if invalid */

            public:
                /** @brief Default constructor - creates invalid address */
                Address() : m_dr(nullptr), m_lid(0) {}
                /** @brief Create direct route address */
                Address(const direct_route_t *dr) : m_dr(dr), m_lid(0) {}
                /** @brief Create LID-based address */
                Address(lid_t lid) : m_dr(nullptr), m_lid(lid) {}

            public:
                /**
                 * @brief Check if this address is valid for routing
                 * @return true if either direct route or LID is valid
                 */
                bool is_valid() const {
                    return m_dr != nullptr || m_lid != 0;
                }
        };

    protected:
        /** Name identifier for this MAD fetcher instance, used in error messages */
        const std::string           m_name;
        
        /** Reference to vector storing fabric errors encountered during operation */
        vec_p_fabric_err &          m_errors;

        /** Reference to IBDiag instance providing fabric discovery and management */
        IBDiag &                    m_ibdiag;

        /** Reference to Ibis instance providing functionality for sending MADs */
        Ibis &                      m_ibis;

        /** Reference to IBDMExtendedInfo instance providing storage for MAD responses */
        IBDMExtendedInfo &          m_ibdm;

        /** Operational options controlling fetcher behavior */
        Options                     m_options;

        /** Required GMP capability mask for target nodes */
        EnGMPCapabilityMaskBit_t    m_gmp_cap;
        
        /** Required SMP capability mask for target nodes */
        EnSMPCapabilityMaskBit_t    m_smp_cap;

        /** Progress bar for tracking operation completion */
        ProgressBar *               m_progress_bar;
        
        /** Callback data structure for handling MAD responses */
        clbck_data_t                m_clbck_data;

        /** Set tracking nodes that have already reported errors */
        unordered_set<IBNode*>      m_has_errors;

    /**
     * @brief Constructs a MadFetcher instance with the specified configuration.
     *
     * @param name      Unique identifier for this fetcher instance (used in error reporting)
     * @param ibdiag    Reference to the IBDiag object for fabric management operations
     * @param errors    Reference to a vector for collecting errors encountered during fetch operations
     * @param options   Bitmask of Options flags that control fetcher behavior
     * @param gmp_cap   Required GMP capability mask for target devices
     * @param smp_cap   Required SMP capability mask for target devices
     *
     * @details
     * The constructor initializes all internal state for the MadFetcher, including:
     * - Storing configuration parameters and references
     * - Initializing the callback data structure
     * - Creating a progress bar if progress tracking is enabled (unless DisableProgressBar is set)
     * - Setting up error tracking for nodes
     * - Storing required capability masks for GMP and SMP operations
     *
     * The progress bar is created according to the operation mode (per-port or per-node), unless disabled.
     * Additional configuration may be applied after construction.
     *
     * @see MadFetcher::do_request() for request validation and routing
     * @see MadFetcher::callback() for response handling
     * @see MadFetcher::fetch() for the public interface
     */
    public:
        MadFetcher(const std::string &              name,
                   IBDiag &                        ibdiag,
                   vec_p_fabric_err &              errors,
                   Options                         options,
                   EnGMPCapabilityMaskBit_t        gmp_cap,
                   EnSMPCapabilityMaskBit_t        smp_cap);

    public:
        /** Deleted default constructor - all instances must be fully initialized */
        MadFetcher() = delete;
        
        /** Deleted copy constructor - MAD fetchers cannot be copied */
        MadFetcher(const MadFetcher &) = delete;
        
        /** Deleted assignment operator - MAD fetchers cannot be assigned */
        MadFetcher & operator=(const MadFetcher &) = delete;

    /**
     * Virtual destructor to allow proper cleanup of derived classes.
     * Deletes the progress bar object that was allocated in the constructor.
     * The progress bar pointer will be null if progress bars were disabled via options.
     *
     * @see Destructor implementation in ibdiag_mad_fetcher.cpp
     */
    public:
        virtual ~MadFetcher();

    /**
     * @brief Applies a single device option bit from the configuration section.
     *
     * This method checks the specified configuration section for a key corresponding to the given option bit.
     * If the key is present and set, the corresponding bit in the fetcher's options mask is enabled or disabled.
     * This allows fine-grained control over individual fetcher behaviors via configuration files.
     *
     * @param section The configuration section to read from (typically from an IniFile).
     * @param key The configuration key to look up (usually a string prefix + option name).
     * @param bit The Options enum value representing the bit to set or clear.
     *
     * @details
     * - If the key is present and evaluates to true, the bit is set in the options mask.
     * - If the key is present and evaluates to false, the bit is cleared in the options mask.
     * - If the key is not present, the current value of the bit is left unchanged.
     * - This method is typically called for each supported option during configuration loading.
     *
     * @see apply_dev_options(const IniFile::Section&, const std::string&)
     * @see apply_dev_options(const IniFile::Section&)
     */
    private:
        void apply_dev_options_bit(const IniFile::Section & section, const std::string & key, Options bit);

    /**
     * @brief Applies all supported device options from a configuration section using a key prefix.
     *
     * This method iterates over all supported option bits and attempts to set or clear them
     * based on the values found in the configuration section, using the provided prefix to
     * construct the full key for each option.
     *
     * @param section The configuration section to read from (typically from an IniFile).
     * @param prefix The string prefix to prepend to each option key (e.g., "MadFetcher" or "MadFetcher.InstanceName").
     *
     * @details
     * - For each supported Options bit, constructs a key as prefix + option name.
     * - Calls apply_dev_options_bit() for each key/bit pair.
     * - Allows both global and instance-specific configuration of fetcher behavior.
     * - This method is typically called twice: once for the global prefix, and once for the instance-specific prefix.
     *
     * @see apply_dev_options_bit(const IniFile::Section&, const std::string&, Options)
     * @see apply_dev_options(const IniFile::Section&)
     */
    private:
        void apply_dev_options(const IniFile::Section & section, const std::string & prefix);

    /**
     * @brief Applies device options from a configuration section using both global and instance-specific prefixes.
     *
     * This method applies configuration options to the fetcher by calling apply_dev_options()
     * with both the generic "MadFetcher" prefix and the instance-specific "MadFetcher.{name}" prefix.
     * This enables a two-level configuration hierarchy: global defaults and per-instance overrides.
     *
     * @param section The configuration section to read from (typically from an IniFile).
     *
     * @details
     * - First applies options using the "MadFetcher" prefix for global settings.
     * - Then applies options using the "MadFetcher.{instance_name}" prefix for overrides.
     * - Ensures that instance-specific settings take precedence over global defaults.
     * - This method is typically called during fetcher initialization or when reloading configuration.
     *
     * @see apply_dev_options(const IniFile::Section&, const std::string&)
     */
    private:
        void apply_dev_options(const IniFile::Section & section);

    /**
     * @brief Loads and applies device configuration from the IBDiag configuration sections.
     *
     * This method is the main entry point for loading and applying device options to the fetcher.
     * It checks for the presence of both the default and instance-specific configuration sections
     * in the IBDiag configuration, and applies their options if available.
     *
     * @details
     * - Checks for the default configuration section (IniFile::DEFAULT_SECTION_NAME) and applies its options.
     * - Checks for the instance-specific configuration section (using the fetcher's name) and applies its options.
     * - Ensures that all relevant configuration settings are loaded before fetcher operation.
     * - This method is typically called during fetcher construction or when reloading configuration.
     *
     * @see apply_dev_options(const IniFile::Section&)
     * @see IBDiag::GetDevConfig()
     * @see IBDiag::GetDevConfigName()
     */
    private:
        void apply_dev_config();

    /**
     * @brief Fetches Management Datagram (MAD) data from the InfiniBand fabric
     *
     * @param do_clear Boolean flag controlling operation mode:
     *                 true = clear operations on counters/attributes
     *                 false = read operations (default)
     *
     * @return Status code indicating operation result:
     *         - IBDIAG_SUCCESS_CODE: Operation completed successfully
     *         - IBDIAG_ERR_CODE_DISCOVERY_NOT_SUCCESS: Fabric discovery incomplete
     *         - IBDIAG_ERR_CODE_DISABLED: Node validation failed
     *         - Other error codes as defined in ibdiag.h
     *
     * @details
     * This method implements the main entry point for MAD operations across the entire
     * InfiniBand fabric. It orchestrates the process of fetching management data from
     * all relevant nodes based on configured options and capability requirements.
     *
     * The method implements a multi-stage processing pipeline:
     *
     * 1. Discovery State Validation:
     *    - Verifies fabric discovery is complete via m_ibdiag.IsDiscoveryDone()
     *    - Returns IBDIAG_ERR_CODE_DISCOVERY_NOT_SUCCESS if discovery incomplete
     *    - Ensures fabric topology is fully mapped before proceeding
     *
     * 2. Node Iteration and Processing:
     *    - Iterates through all nodes in the fabric
     *    - Applies node-level validation via fetch_node()
     *    - Handles per-port vs per-node operations based on m_options
     *    - Tracks progress via m_progress_bar
     *
     * 3. Error Handling and Reporting:
     *    - Collects errors in m_errors vector
     *    - Supports configurable error handling via m_options flags
     *    - Provides detailed error context for debugging
     *
     * 4. Progress Tracking:
     *    - Updates progress bar based on operation mode
     *    - Supports both per-port and per-node progress tracking
     *    - Only displays progress if AllowProgressBar option enabled
     *
     * The method supports two primary operation modes:
     * - Read Mode (do_clear = false):
     *   * Fetches current values of management attributes
     *   * Used for monitoring and diagnostics
     *   * Non-destructive operation
     *
     * - Clear Mode (do_clear = true):
     *   * Resets counters and attributes to initial values
     *   * Used for maintenance and troubleshooting
     *   * May affect device state
     *
     * @see fetch_node() for node-level processing details
     * @see do_request() for request execution details
     * @see callback() for response handling details
     */
    public:
        int fetch(bool do_clear = false);

    /**
     * @brief Fetches Management Datagram (MAD) data from a specific InfiniBand node
     *
     * @param node Pointer to the target InfiniBand node
     * @param do_clear Boolean flag controlling operation mode:
     *                 true = clear operations on counters/attributes
     *                 false = read operations (default)
     *
     * @return Status code indicating operation result:
     *         - IBDIAG_SUCCESS_CODE: Operation completed successfully
     *         - IBDIAG_ERR_CODE_DISCOVERY_NOT_SUCCESS: Fabric discovery incomplete
     *         - IBDIAG_ERR_CODE_INCORRECT_ARGS: Invalid node pointer
     *         - IBDIAG_ERR_CODE_DISABLED: Node validation failed
     *         - IBDIAG_ERR_CODE_DB_ERR: Database access error
     *
     * @details
     * This method implements node-level MAD operations with comprehensive validation
     * and error handling. It serves as the core processing pipeline for MAD requests
     * at the node level, supporting both per-port and per-node operation modes.
     *
     * The method implements a multi-stage validation pipeline:
     *
     * 1. Discovery State Validation:
     *    - Verifies fabric discovery is complete via m_ibdiag.IsDiscoveryDone()
     *    - Returns IBDIAG_ERR_CODE_DISCOVERY_NOT_SUCCESS if discovery incomplete
     *    - Ensures fabric topology is fully mapped before proceeding
     *
     * 2. Node Type and Scope Validation:
     *    - Validates node pointer is non-null
     *    - Applies node type filtering based on m_options flags:
     *      * HCAs: Processes IB_CA_NODE type nodes (Host Channel Adapters)
     *      * SWs: Processes IB_SW_NODE type nodes (Switches)
     *      * RTRs: Processes IB_RTR_NODE type nodes (Routers)
     *      * GPUs: Processes IB_CA_NODE nodes with IB_GPU_NODE extended type
     *    - Verifies node->getInSubFabric() unless IgnoreNodeScope flag set
     *    - Requires SpecialNodes flag for non-standard node types
     *
     * 3. Management Capability Validation:
     *    - Validates required management capabilities:
     *      * Checks SMP capabilities if m_smp_cap specified
     *      * Checks GMP capabilities if m_gmp_cap specified
     *    - Logs detailed capability mismatch errors
     *    - Returns IBDIAG_ERR_CODE_DISABLED if capabilities not supported
     *
     * 4. MAD Operation Execution:
     *    - Updates callback context with current node information
     *    - Implements dual-mode operation:
     *      * PerPort Mode:
     *        - Iterates through relevant ports based on node type
     *        - Applies port filtering based on configuration
     *        - Handles null ports per SkipNullPort/IgnoreNullPort flags
     *        - Executes fetch(port, do_clear) for each valid port
     *      * PerNode Mode:
     *        - Directly executes do_request(node, nullptr, do_clear)
     *        - Handles node-level MAD operations
     *
     * @see MadFetcher::fetch() Implementation in ibdiag_mad_fetcher.cpp
     * @see MadFetcher::do_request() Core MAD request handling
     */
    private:
        int fetch(IBNode *node, bool do_clear);

    /**
     * @brief Executes a MAD request to a specific InfiniBand port
     *
     * @param port Pointer to the target InfiniBand port
     * @param do_clear Boolean flag controlling operation mode:
     *                 true = clear operations on counters/attributes
     *                 false = read operations (default)
     *
     * @return Status code indicating operation result:
     *         - IBDIAG_SUCCESS_CODE: Operation completed successfully
     *         - IBDIAG_ERR_CODE_DISCOVERY_NOT_SUCCESS: Fabric discovery incomplete
     *         - IBDIAG_ERR_CODE_INCORRECT_ARGS: Invalid port pointer
     *         - IBDIAG_ERR_CODE_DISABLED: Port validation failed
     *         - Other error codes from do_request()
     *
     * @details
     * This method implements port-level Management Datagram (MAD) operations with
     * comprehensive validation and error handling. It serves as the primary entry
     * point for port-specific MAD requests, ensuring proper discovery state,
     * port validation, and callback context before executing the actual request.
     *
     * The method follows a strict validation sequence:
     *
     * 1. Discovery State Validation:
     *    - Verifies fabric discovery is complete via m_ibdiag.IsDiscoveryDone()
     *    - Returns IBDIAG_ERR_CODE_DISCOVERY_NOT_SUCCESS if discovery incomplete
     *    - Ensures fabric topology is fully mapped before proceeding
     *
     * 2. Port Validation:
     *    - Validates port pointer is non-null
     *    - Verifies port->getInSubFabric() unless IgnorePortScope flag set
     *    - For special ports (e.g., management ports), requires SpecialPort flag
     *    - Validates port number based on node type and configuration options
     *    - Returns IBDIAG_ERR_CODE_INCORRECT_ARGS for null ports
     *    - Returns IBDIAG_ERR_CODE_DISABLED for invalid port configurations
     *
     * 3. Callback Context Setup:
     *    - Updates m_clbck_data.m_p_port with current port pointer
     *    - Ensures proper context for asynchronous MAD response handling
     *    - Maintains port context for error reporting and progress tracking
     *
     * 4. MAD Request Execution:
     *    - Forwards validated request to do_request(port->p_node, port, do_clear)
     *    - do_request() implements:
     *      * Address resolution (direct route or LID based on MAD type)
     *      * Data worthiness validation per port capabilities
     *      * MAD transmission with proper addressing
     *      * Comprehensive error handling and reporting
     *
     * @see MadFetcher::fetch() Implementation in ibdiag_mad_fetcher.cpp
     * @see MadFetcher::do_request() Core MAD request handling
     */
    private:
        int fetch(IBPort *port, bool do_clear);

    /**
     * @brief Resolves the appropriate addressing information for a node/port pair
     *
     * @param node Pointer to the target InfiniBand node
     * @param port Pointer to pointer of the target InfiniBand port
     *              May be modified to point to a different port if needed
     *
     * @return Address object containing either:
     *         - Direct route information for SMP MADs
     *         - LID-based addressing for GMP MADs
     *
     * @details
     * This method determines the appropriate addressing mechanism based on:
     * 1. MAD Type (SMP vs GMP):
     *    - SMP MADs require direct route addressing
     *    - GMP MADs use LID-based addressing
     *
     * 2. Node Type:
     *    - Switches: Use direct route for SMP, LID for GMP
     *    - HCAs/Routers: Use LID-based addressing
     *
     * 3. Port Selection:
     *    - For SMP MADs: Uses port 0 (management port) on switches
     *    - For GMP MADs: Finds first valid port with non-zero LID
     *
     * The method handles special cases:
     * - Null port pointers
     * - Invalid LID values
     * - Missing direct routes
     * - Management port requirements
     *
     * @see MadFetcher::do_request() Uses resolved address for MAD transmission
     * @see MadFetcher::fetch() Entry point for port-specific requests
     */
    protected:
        virtual Address resolve(const IBNode *node, const IBPort **port);

    /**
     * @brief Core implementation of MAD request handling
     *
     * @param node Pointer to the target InfiniBand node
     * @param port Pointer to the target InfiniBand port
     * @param do_clear If true, performs clear operations instead of read operations
     *
     * @return int Status code indicating success or failure:
     *         - IBDIAG_SUCCESS_CODE: Operation completed successfully
     *         - IBDIAG_ERR_CODE_DISABLED: Request disabled by options or validation
     *         - IBDIAG_ERR_CODE_DR_NULL: Invalid direct route
     *         - IBDIAG_ERR_CODE_LID_ZERO: Invalid LID
     *         - Other error codes as defined in ibdiag.h
     *
     * @details
     * This method implements the core MAD request handling logic:
     *
     * 1. Address Resolution:
     *    - Resolves appropriate addressing (direct route or LID)
     *    - Handles special cases for different node types
     *    - Validates addressing information
     *
     * 2. Port Validation:
     *    - Checks port data worthiness if enabled
     *    - Validates port capabilities
     *    - Handles special cases for switch ports
     *
     * 3. MAD Type Handling:
     *    - SMP MADs: Requires direct route addressing
     *    - GMP MADs: Uses LID-based addressing
     *
     * 4. Error Handling:
     *    - Validates addressing information
     *    - Checks port capabilities
     *    - Handles null pointers
     *    - Manages error reporting
     *
     * The method implements comprehensive validation and error handling
     * to ensure reliable MAD communication with InfiniBand devices.
     *
     * @see MadFetcher::resolve() For address resolution details
     * @see MadFetcher::callback() For response handling
     * @see MadFetcher::fetch() For the public interface
     */
    private:
        int do_request(const IBNode *node,const IBPort *port, bool do_clear);

    /**
     * @brief Handles MAD response callbacks from the InfiniBand fabric
     *
     * @param clbck Callback data structure containing node and port context
     * @param status Status code from the MAD response (0 indicates success)
     * @param data Pointer to the response data buffer
     *
     * @details
     * This method processes MAD responses by:
     * 1. Progress Tracking:
     *    - Updates progress bar based on operation mode (per-port vs per-node)
     *    - Handles invalid callback data cases
     *    - Maintains operation progress state
     *
     * 2. Response Validation:
     *    - Verifies response data pointer is non-null
     *    - Checks status code for error conditions
     *    - Validates callback context (node/port pointers)
     *
     * 3. Error Handling:
     *    - Tracks recurring errors if configured
     *    - Builds detailed error messages with context
     *    - Adds appropriate error types to collection
     *    - Handles null responses and invalid callback data
     *
     * 4. Response Processing:
     *    - Forwards successful responses to implementation handler
     *    - Maintains error state tracking
     *    - Updates progress tracking
     *
     * Error handling includes:
     * - Tracking recurring errors if configured
     * - Building detailed error messages
     * - Adding appropriate error types to the error collection
     * - Handling null responses and invalid callback data
     *
     * @see MadFetcher::do_request() For request handling
     * @see MadFetcher::response() For response processing
     * @see MadFetcher::fetch() For the public interface
     */
    private:
        void callback(const clbck_data_t &clbck, int status, void *data);

    /**
     * @brief Pure virtual method for derived classes to implement MAD request handling
     *
     * @param node Pointer to the target IBNode for the request
     * @param port Pointer to the target IBPort (may be null for per-node operations)
     * @param address Resolved addressing information (direct route or LID)
     * @param do_clear If true, performs clear operations instead of read operations
     * @return int Status code indicating success or failure
     * @retval IBDIAG_SUCCESS_CODE Operation completed successfully
     * @retval Other error codes as defined in ibdiag.h
     *
     * @details
     * This method must be implemented by derived classes to handle:
     * 1. Request Preparation:
     *    - MAD packet construction
     *    - Parameter validation
     *    - Address verification
     *    - Capability checks
     *
     * 2. Request Execution:
     *    - MAD packet transmission
     *    - Timeout handling
     *    - Retry logic
     *    - Error detection
     *
     * 3. Response Management:
     *    - Callback registration
     *    - Response tracking
     *    - Error collection
     *    - State updates
     *
     * The implementation should:
     * - Validate all input parameters
     * - Construct appropriate MAD packets
     * - Handle addressing based on MAD type (SMP vs GMP)
     * - Manage request timeouts and retries
     * - Register callbacks for response handling
     * - Track request state and progress
     * - Handle any pre-transmission errors
     *
     * @see MadFetcher::do_request() For request validation and routing
     * @see MadFetcher::callback() For response handling
     * @see MadFetcher::fetch() For the public interface
     */
    protected:
        virtual int request(const IBNode *node, const IBPort *port, const Address &address, bool do_clear) = 0;

    /**
     * @brief Pure virtual method for derived classes to implement MAD response handling
     *
     * @param clbck Callback data structure containing MAD response information
     * @param status Status code indicating success or failure of the MAD operation
     * @param data Optional user data pointer passed through from the request
     * @return int Status code indicating success or failure of response processing:
     *         - IBDIAG_SUCCESS_CODE: Response processed successfully
     *         - Other error codes as defined in ibdiag.h for processing failures
     *
     * @details
     * This method must be implemented by derived classes to handle:
     * 1. Response Processing:
     *    - MAD packet parsing
     *    - Data extraction
     *    - Error detection
     *    - State validation
     *
     * 2. Error Handling:
     *    - Status code interpretation
     *    - Error message generation
     *    - Error collection
     *    - Recovery attempts
     *
     * 3. Data Management:
     *    - Response data storage
     *    - Data validation
     *    - State updates
     *    - Progress tracking
     *
     * The implementation should:
     * - Validate the response status
     * - Parse MAD response packets
     * - Extract and validate response data
     * - Handle any response errors
     * - Update operation state
     * - Track progress
     * - Store results
     * - Generate appropriate error messages
     * - Return IBDIAG_SUCCESS_CODE on successful processing
     *   or appropriate error codes for processing failures
     *
     * @see MadFetcher::request() For request handling
     * @see MadFetcher::callback() For callback registration
     * @see MadFetcher::fetch() For the public interface
     */
    protected:
        virtual int response(const clbck_data_t &clbck, int status, void *data) = 0;
};

/**
 * @brief Template class for MAD fetchers that handle specific response data types
 *
 * @details
 * This template class extends the base MadFetcher to provide type-safe handling of MAD responses.
 * It implements a type-safe interface for handling responses of type T.
 *
 * Key features:
 * - Type-safe response data storage
 * - Automatic data initialization
 * - Type checking at compile time
 * - Safe data casting
 * - Memory management
 *
 * The template parameter T must:
 * - Be a POD type
 * - Have a defined size
 * - Support zero initialization
 * - Be suitable for MAD response data
 *
 * @tparam T The type of MAD response data this fetcher will handle
 * @see MadFetcher for base class functionality
 * @see ibdiag_mad_fetcher.cpp for implementation details
 */
template<typename T>
    class MadFetcherT : public MadFetcher {
        protected:
            /** @brief Storage for MAD response data of type T */
            T m_data;

        /**
         * @brief Constructor for MadFetcherT template class
         *
         * @param name The name identifier for this fetcher instance
         * @param ibdiag Reference to the IBDiag instance for fabric operations
         * @param errors Vector to store any errors encountered during operations
         * @param options Bitmask of Options flags controlling fetcher behavior
         * @param gmp_cap Required GMP capability mask for target devices (default: EnGMPCapNone)
         * @param smp_cap Required SMP capability mask for target devices (default: EnSMPCapNone)
         *
         * @details
         * Initializes a new MadFetcherT instance with the specified configuration. The fetcher
         * will operate according to the provided options and capability requirements.
         *
         * The constructor:
         * 1. Initializes the base MadFetcher class with provided parameters
         * 2. Zero-initializes the type-specific data storage (m_data)
         * 3. Sets up error handling and progress tracking
         * 4. Configures MAD operation parameters
         *
         * The template parameter T must:
         * - Be a POD type suitable for MAD response data
         * - Support zero initialization via memset
         * - Have a defined size at compile time
         *
         * @see MadFetcher::MadFetcher() For base class initialization
         * @see MadFetcherT::m_data For type-specific data storage
         */
        public:
            MadFetcherT(const std::string &         name,
                        IBDiag &                    ibdiag,
                        vec_p_fabric_err &          errors,
                        Options                     options,
                        EnGMPCapabilityMaskBit_t    gmp_cap = EnGMPCapNone,
                        EnSMPCapabilityMaskBit_t    smp_cap = EnSMPCapNone)

                : MadFetcher(name, ibdiag, errors, options, gmp_cap, smp_cap)
            {
                memset(&m_data, 0, sizeof(T));
            }

        /**
         * @brief Default constructor explicitly deleted
         *
         * @details
         * The default constructor is explicitly deleted to prevent accidental
         * instantiation without proper initialization parameters. This ensures
         * that all MadFetcherT instances are created with:
         * - A valid name identifier
         * - A reference to an IBDiag instance
         * - An error collection vector
         * - Required capability masks
         * - Appropriate options configuration
         *
         * This prevents potential undefined behavior that could occur from
         * uninitialized MAD fetcher instances.
         */
        public:
            MadFetcherT() = delete;

        /**
         * @brief Copy constructor explicitly deleted
         *
         * @details
         * The copy constructor is explicitly deleted to prevent accidental
         * copying of MadFetcherT instances. This is necessary because:
         * - MAD fetchers maintain internal state that shouldn't be duplicated
         * - The fetcher holds references to external resources (IBDiag, errors)
         * - Copying could lead to resource management issues
         * - Multiple fetchers operating on the same resources could cause conflicts
         *
         * Instead of copying, new instances should be created with appropriate
         * initialization parameters.
         */
        public:
            MadFetcherT(const MadFetcherT &) = delete;

        /**
         * @brief Assignment operator explicitly deleted
         *
         * @details
         * The assignment operator is explicitly deleted to prevent accidental
         * assignment of MadFetcherT instances. This is necessary because:
         * - MAD fetchers maintain internal state that shouldn't be transferred
         * - The fetcher holds references to external resources (IBDiag, errors)
         * - Assignment could lead to resource management issues
         * - Multiple fetchers operating on the same resources could cause conflicts
         *
         * Instead of assignment, new instances should be created with appropriate
         * initialization parameters.
         */
        public:
            MadFetcherT & operator=(const MadFetcherT &) = delete;

        /**
         * @brief Pure virtual method for handling MAD response data
         *
         * @param clbck Callback data structure containing node and port information
         * @param status Status code from the MAD response (0 indicates success)
         * @param data Pointer to the typed response data structure
         * @return int Status code indicating success or failure of response processing:
         *         - IBDIAG_SUCCESS_CODE: Response processed successfully
         *         - Other error codes as defined in ibdiag.h for processing failures
         *
         * @details
         * This method must be implemented by derived classes to process MAD responses.
         * It is called by the base class after successful validation of the response.
         *
         * The implementation should:
         * 1. Process the response data according to the specific MAD type
         * 2. Extract relevant information from the data structure
         * 3. Store or forward the processed data as needed
         * 4. Handle any type-specific error conditions
         * 5. Update any internal state based on the response
         * 6. Return IBDIAG_SUCCESS_CODE on successful processing
         *    or appropriate error codes for processing failures
         *
         * The template parameter T defines the type of data structure expected
         * in the response. This provides type safety and compile-time checking.
         *
         * @note This method is only called for successful responses (status = 0)
         * @note The data pointer is guaranteed to be non-null
         * @note The callback data contains valid node/port information
         *
         * @see MadFetcher::callback() For the calling context
         * @see MadFetcherT::response(const clbck_data_t&, int, void*) For the base implementation
         */
        protected:
            virtual int response(const clbck_data_t &clbck, int status, T *data) = 0;

        /**
         * @brief Final implementation of base class response handler that provides type-safe casting
         *
         * @details
         * This method serves as the final implementation of the base class's response handler,
         * providing a type-safe bridge between the generic void* interface and the strongly-typed
         * template parameter T. It performs the following key functions:
         *
         * 1. Type Casting:
         *    - Safely casts the void* data pointer to the template type T*
         *    - Assumes the data was originally allocated as type T
         *    - Relies on the caller to ensure correct type alignment
         *
         * 2. Type Safety:
         *    - Enforces compile-time type checking through template parameter T
         *    - Prevents accidental type mismatches in derived classes
         *    - Maintains type consistency throughout the response chain
         *
         * 3. Response Forwarding:
         *    - Delegates to the pure virtual response(T*) method
         *    - Preserves all context information in the callback data
         *    - Maintains the original status code
         *    - Returns the status code from the typed response handler
         *
         * 4. Memory Safety:
         *    - Assumes the data pointer remains valid during the cast
         *    - Relies on the caller to manage memory lifetime
         *    - Does not perform any memory allocation or deallocation
         *
         * 5. Error Handling:
         *    - Assumes the status code has already been validated
         *    - Delegates error handling to the typed response handler
         *    - Maintains consistent error propagation
         *
         * @note This method is marked as final to prevent further overriding
         * @note The method assumes the data pointer is non-null and properly aligned
         * @note The status parameter is expected to be 0 for successful responses
         * @note The callback data structure must contain valid node/port information
         *
         * @param clbck [in] Callback data structure containing node and port context information
         * @param status [in] Response status code (0 indicates success)
         * @param data [in] Pointer to the response data to be cast to type T*
         * @return int Status code returned from the typed response handler:
         *         - IBDIAG_SUCCESS_CODE: Response processed successfully
         *         - Other error codes as defined in ibdiag.h for processing failures
         *
         * @see MadFetcher::callback() For the calling context
         * @see MadFetcherT::response(const clbck_data_t&, int, T*) For the typed handler
         */
        protected:
            int response(const clbck_data_t &clbck, int status, void *data) final {
                return response(clbck, status, (T*) data);
            };
    };

/**
 * @brief Converts MadFetcher::Options enum value to its string representation
 *
 * @details
 * This function provides a human-readable string representation of the MadFetcher::Options
 * enum values, which is useful for debugging, logging, and user interface purposes.
 * The function performs the following operations:
 *
 * 1. Enum Value Mapping:
 *    - Maps each enum value to its corresponding string literal
 *    - Provides consistent naming convention for all options
 *    - Maintains a one-to-one relationship between enum and string
 *
 * 2. String Representation:
 *    - Returns a null-terminated C-style string
 *    - Uses descriptive names that match the enum identifiers
 *    - Provides case-sensitive representation matching the enum
 *
 * 3. Usage Context:
 *    - Primarily used for debugging and diagnostic output
 *    - Supports logging of configuration options
 *    - Enables user-friendly display of option values
 *
 * 4. Return Value:
 *    - Returns a pointer to a static string literal
 *    - The returned string remains valid for the lifetime of the program
 *    - No memory management is required by the caller
 *
 * 5. Error Handling:
 *    - Assumes the input enum value is valid
 *    - May return an implementation-defined string for invalid values
 *    - Caller should validate enum values before calling this function
 *
 * @note This function is thread-safe as it returns static string literals
 * @note The returned string should not be modified by the caller
 * @note For invalid enum values, the behavior is implementation-defined
 * @note This function is typically used in conjunction with logging or debugging utilities
 *
 * @param options [in] The MadFetcher::Options enum value to convert to string
 *
 * @return const char* Pointer to the string representation of the enum value
 *
 * @see MadFetcher::Options For the enum definition
 * @see MadFetcher For the context of how options are used
 *
 * @example
 * ```cpp
 * MadFetcher::Options opt = MadFetcher::Options::SOME_OPTION;
 * const char* str = to_str(opt);
 * printf("Selected option: %s\n", str);
 * ```
 */
const char* to_str(MadFetcher::Options options);
