/*
 * Copyright (c) 2004-2021 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 2025-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

/**
 * @file ibdiag_progress_bar.h
 * @brief Progress bar implementation for tracking InfiniBand network discovery and data retrieval operations
 * 
 * This header file defines a set of classes for tracking and displaying progress of various
 * InfiniBand network operations. It provides a flexible framework for monitoring:
 * - Node discovery progress (switches and channel adapters)
 * - Port information retrieval
 * - MAD request completion
 * - Overall network discovery status
 * 
 * The implementation uses a base class (ProgressBar) with specialized derived classes
 * for different types of operations. Each class maintains its own progress tracking
 * and display format.
 * 
 * @note This header requires C++11 or later
 * @note Dependencies: <map>, <stdint.h>, <time.h>
 * @note Forward declarations: IBNode, IBPort
 * 
 * @author Mellanox Technologies LTD
 * @author NVIDIA CORPORATION & AFFILIATES
 * @copyright Copyright (c) 2004-2021 Mellanox Technologies LTD. All rights reserved.
 * @copyright Copyright (c) 2025-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * @license BSD
 */

#ifndef IBDIAG_PROGRESS_BAR_H
#define IBDIAG_PROGRESS_BAR_H

#include <map>
#include <stdint.h>
#include <time.h>


class IBNode;
class IBPort;

/**
 * @brief Base class for tracking progress of InfiniBand network operations
 * 
 * This class provides the core functionality for tracking and displaying progress of various
 * InfiniBand network operations. It maintains separate counters for:
 * - Switch nodes and their ports
 * - Channel adapter nodes and their ports
 * - Total MAD requests
 * 
 * The class uses a map-based approach to track individual node and port statistics,
 * allowing for granular progress monitoring. It implements a rate-limited update mechanism
 * to prevent excessive screen updates while maintaining responsive progress display.
 * 
 * @note This is an abstract base class - derived classes must implement the output() method
 *       to define their specific display format.
 * @note The class is not thread-safe and should be used from a single thread
 * @note Progress updates are rate-limited to once per second by default
 */
class ProgressBar
{
    protected:
        /**
         * @brief Structure to track progress of a specific operation
         * 
         * This structure maintains counters for tracking the progress of any operation:
         * - m_total: The total number of items that need to be processed
         * - m_complete: The number of items that have been completed
         * 
         * @note Both counters are initialized to 0 in the constructor
         * @note Counters are 64-bit unsigned integers to handle large networks
         */
        typedef struct entry {
            uint64_t m_total;    ///< Total number of items to process
            uint64_t m_complete; ///< Number of completed items

            entry() : m_total(0), m_complete(0) {}
        } entry_t;

        typedef std::map<const IBPort*, uint64_t> ports_stat_t; ///< Map to track port statistics
        typedef std::map<const IBNode*, uint64_t> nodes_stat_t; ///< Map to track node statistics

        entry_t         m_sw;        ///< Switch node progress tracking
        entry_t         m_ca;        ///< Channel adapter node progress tracking
        entry_t         m_sw_ports;  ///< Switch port progress tracking
        entry_t         m_ca_ports;  ///< CA port progress tracking
        entry_t         m_requests;  ///< Total MAD requests progress tracking

        ports_stat_t    m_ports_stat; ///< Statistics for individual ports
        nodes_stat_t    m_nodes_stat; ///< Statistics for individual nodes

        struct timespec m_last_update; ///< Timestamp of last progress update
        bool            m_visible;     ///< Whether progress bar is visible

    public:
        /**
         * @brief Construct a new Progress Bar object
         * 
         * Initializes a new progress bar with the specified visibility setting.
         * Sets up initial timestamps and initializes all counters to zero.
         * 
         * @param visible Whether the progress bar should be visible (default: true)
         * @throw std::runtime_error if clock_gettime() fails
         */
        ProgressBar(bool visible = true) : m_visible(visible) {
            clock_gettime(CLOCK_REALTIME, &m_last_update);
        }

        /**
         * @brief Virtual destructor
         * 
         * Ensures proper cleanup of derived class resources
         */
        virtual ~ProgressBar() {}

    public:
        /**
         * @brief Output the current progress state
         * 
         * Pure virtual function that must be implemented by derived classes
         * to display progress in a specific format. The implementation should:
         * - Format the current progress statistics
         * - Display the formatted output
         * - Handle any necessary screen updates
         * 
         * @note This method is called by update() when progress display needs refreshing
         * @note Derived classes should ensure thread-safe output if used in multi-threaded contexts
         */
        virtual void output() = 0;

    protected:
        /**
         * @brief Update the progress display
         * 
         * Manages the frequency of progress updates to prevent excessive screen refreshes.
         * Updates will occur if:
         * - The progress bar is visible (m_visible is true)
         * - More than 1 second has elapsed since last update
         * - The force parameter is true
         * 
         * @param force Force update regardless of time elapsed (default: false)
         * @note This method is rate-limited to prevent excessive screen updates
         */
        void update(bool force=false);

    public:
        /**
         * @brief Add a node to the progress tracking
         * 
         * Initializes or updates tracking for a specific node. This method:
         * - Creates a new entry if the node isn't being tracked
         * - Updates existing tracking if the node is already known
         * - Increments appropriate counters based on node type
         * - Triggers a progress update
         * 
         * @param node Pointer to the IBNode to track
         * @throw std::invalid_argument if node is nullptr
         */
        void push(const IBNode *node);

    public:
        /**
         * @brief Add a port to the progress tracking
         * 
         * Initializes or updates tracking for a specific port. This method:
         * - Creates a new entry if the port isn't being tracked
         * - Updates existing tracking if the port is already known
         * - Increments appropriate counters based on port type
         * - Triggers a progress update
         * - Coordinates with node-level tracking
         * 
         * @param port Pointer to the IBPort to track
         * @throw std::invalid_argument if port is nullptr
         */
        void push(const IBPort *port);

    public:
        /**
         * @brief Mark a node as completed
         * 
         * Updates progress tracking when a node operation completes. This method:
         * - Verifies the node exists and has pending operations
         * - Decrements the node's request counter
         * - Updates completion counters if all requests are done
         * - Triggers a progress update
         * 
         * @param node Pointer to the completed IBNode
         * @throw std::invalid_argument if node is nullptr
         * @throw std::runtime_error if node is not being tracked
         */
        void complete(const IBNode *node);

    public:
        /**
         * @brief Mark a port as completed
         * 
         * Updates progress tracking when a port operation completes. This method:
         * - Verifies the port exists and has pending operations
         * - Decrements the port's request counter
         * - Updates completion counters if all requests are done
         * - Coordinates with node-level completion tracking
         * - Triggers a progress update
         * 
         * @param port Pointer to the completed IBPort
         * @throw std::invalid_argument if port is nullptr
         * @throw std::runtime_error if port is not being tracked
         */
        void complete(const IBPort *port);

    public:
        /**
         * @brief Template method to complete an object and update progress
         * 
         * Generic method for completing any tracked object and updating progress.
         * This template method:
         * - Casts the generic object pointer to the correct type
         * - Calls the appropriate complete() method
         * - Returns the completed object
         * 
         * @tparam T Type of the object to complete (must be IBNode or IBPort)
         * @param p_progress_bar Pointer to the progress bar instance
         * @param p_obj Pointer to the object to complete
         * @return T* Pointer to the completed object
         * @throw std::invalid_argument if p_progress_bar or p_obj is nullptr
         */
        template<typename T>
            static T *complete(void *p_progress_bar, void *p_obj) {
               T *p_t_obj = (T *)p_obj;
               if (p_progress_bar && p_t_obj)
                   ((ProgressBar *)p_progress_bar)->complete(p_t_obj);

               return p_t_obj;
            }
};

/**
 * @brief Progress bar implementation for tracking node discovery operations
 * 
 * Specialized progress bar for monitoring node discovery operations. This class:
 * - Tracks discovery of switches and channel adapters
 * - Displays progress in a format specific to node discovery
 * - Updates the display with current discovery statistics
 * 
 * The output format shows:
 * - Total number of nodes discovered
 * - Number of switches discovered
 * - Number of channel adapters discovered
 * 
 * @note This class is designed for use during the initial network discovery phase
 * @note The output is formatted as a single line that updates in place
 */
class ProgressBarNodes: public ProgressBar
{
    public:
        /**
         * @brief Construct a new Progress Bar Nodes object
         * 
         * Initializes a new node discovery progress bar with the specified visibility.
         * 
         * @param visible Whether the progress bar should be visible (default: true)
         */
        ProgressBarNodes(bool visible = true): ProgressBar(visible) { }

        /**
         * @brief Destructor
         * 
         * Ensures final progress update is displayed before destruction
         */
        virtual ~ProgressBarNodes() { this->update(true); }

        /**
         * @brief Output the current node discovery progress
         * 
         * Displays a formatted progress message showing:
         * - Total completed/total node requests
         * - Completed/total switch nodes
         * - Completed/total CA nodes
         * 
         * The output is formatted as a single line that updates in place.
         * Format: "Nodes: X/Y (Switches: A/B, CAs: C/D)"
         */
        virtual void output();
};

/**
 * @brief Progress bar implementation for tracking port information retrieval
 * 
 * Specialized progress bar for monitoring port information retrieval operations. This class:
 * - Tracks retrieval of switch port and CA port information
 * - Displays progress in a format specific to port operations
 * - Updates the display with current port statistics
 * 
 * The output format shows:
 * - Total number of port MAD requests completed
 * - Number of switch ports processed
 * - Number of CA ports processed
 * 
 * @note This class is designed for use during the port information gathering phase
 * @note The output is formatted as a single line that updates in place
 */
class ProgressBarPorts: public ProgressBar
{
    public:
        /**
         * @brief Construct a new Progress Bar Ports object
         * 
         * Initializes a new port progress bar with the specified visibility.
         * 
         * @param visible Whether the progress bar should be visible (default: true)
         */
        ProgressBarPorts(bool visible = true): ProgressBar(visible) { }

        /**
         * @brief Destructor
         * 
         * Ensures final progress update is displayed before destruction
         */
        virtual ~ProgressBarPorts() { this->update(true); }

        /**
         * @brief Output the current port retrieval progress
         * 
         * Displays a formatted progress message showing:
         * - Total completed/total port MAD requests
         * - Completed/total switch ports
         * - Completed/total CA ports
         * 
         * The output is formatted as a single line that updates in place.
         * Format: "Ports: X/Y (Switch ports: A/B, CA ports: C/D)"
         */
        virtual void output();
};

/**
 * @brief Progress bar implementation for tracking network discovery operations
 * 
 * Specialized progress bar for monitoring overall network discovery progress. This class:
 * - Tracks discovery of all network elements
 * - Displays progress in a format specific to network discovery
 * - Updates the display with current discovery statistics
 * 
 * The output format shows:
 * - Total number of nodes discovered
 * - Number of switches discovered
 * - Number of channel adapters discovered
 * 
 * @note This class provides a high-level overview of the entire discovery process
 * @note The output is formatted as a single line that updates in place
 */
class ProgressBarDiscover: public ProgressBar
{
    public:
        /**
         * @brief Construct a new Progress Bar Discover object
         * 
         * Initializes a new network discovery progress bar with the specified visibility.
         * 
         * @param visible Whether the progress bar should be visible (default: true)
         */
        ProgressBarDiscover(bool visible = true): ProgressBar(visible) { }

        /**
         * @brief Destructor
         * 
         * Ensures final progress update is displayed before destruction
         */
        virtual ~ProgressBarDiscover() { this->update(true); }

        /**
         * @brief Output the current network discovery progress
         * 
         * Displays a formatted progress message showing:
         * - Total number of nodes discovered
         * - Number of switches discovered
         * - Number of channel adapters discovered
         * 
         * The output is formatted as a single line that updates in place.
         * Format: "Network Discovery: X/Y nodes (Switches: A/B, CAs: C/D)"
         */
        virtual void output();
};

#endif   /* IBDIAG_PROGRESS_BAR_H */
