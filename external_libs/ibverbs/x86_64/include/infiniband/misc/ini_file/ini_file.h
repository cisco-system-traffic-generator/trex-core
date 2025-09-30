/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */

#pragma once

#include <string>
#include <map>
#include <vector>
#include <limits>
#include <iostream>
#include <algorithm>
#include <type_traits>


/**
 * @brief Macro for normalizing section names in INI files
 * 
 * @param s The section name string to normalize
 * @return The normalized section name string
 *
 * @details
 * Controls case sensitivity of section names through conditional compilation.
 * This macro ensures consistent handling of section names throughout the INI file
 * processing by applying normalization rules based on configuration.
 * 
 * When INI_FILE_SECTION_NAME_CASE_SENSITIVE is not defined:
 * - Section names are converted to lowercase using to_lower()
 * - Case-insensitive matching is performed (e.g. "Section1" matches "SECTION1")
 * - Leading/trailing whitespace is removed via trim()
 * - Useful for configurations where section case should not matter
 *
 * When INI_FILE_SECTION_NAME_CASE_SENSITIVE is defined:
 * - Section names are preserved in their original case
 * - Only whitespace trimming is performed
 * - Case-sensitive matching is used (e.g. "Section1" differs from "SECTION1") 
 * - Appropriate when section name case carries semantic meaning
 *
 * Example usage:
 * @code
 * std::string section = "[MySection]";
 * std::string normalized = INI_FILE_SECTION_NORMALIZE(section);
 * // Without INI_FILE_SECTION_NAME_CASE_SENSITIVE: normalized == "mysection"
 * // With INI_FILE_SECTION_NAME_CASE_SENSITIVE: normalized == "MySection"
 * @endcode
 *
 * @warning Must be used consistently throughout the codebase to maintain uniform behavior
 * @warning Case sensitivity affects section name matching and duplicate detection
 *
 * @see IniFile::to_lower() For the lowercase conversion implementation
 * @see IniFile::trim() For whitespace removal implementation
 * @see INI_FILE_KEY_NAME_CASE_SENSITIVE For related key name handling
 */
#ifndef INI_FILE_SECTION_NAME_CASE_SENSITIVE
    #define INI_FILE_SECTION_NORMALIZE(s)    ::IniFile::to_lower(::IniFile::trim(s))
#else
    #define INI_FILE_SECTION_NORMALIZE(s)    ::IniFile::trim(s)
#endif

/**
 * @brief Macro for normalizing key names in INI files
 * 
 * @param s The key name string to normalize
 * @return The normalized key name string
 *
 * @details
 * Controls case sensitivity of key names through conditional compilation.
 * This macro ensures consistent handling of key names throughout the INI file
 * processing by applying normalization rules based on configuration.
 *
 * When INI_FILE_KEY_NAME_CASE_SENSITIVE is not defined:
 * - Key names are converted to lowercase using to_lower()
 * - Case-insensitive matching is performed (e.g. "Key1" matches "KEY1")
 * - Leading/trailing whitespace is removed via trim()
 * - Useful for configurations where key case should not matter
 * - Improves usability by accepting varied key case formats
 *
 * When INI_FILE_KEY_NAME_CASE_SENSITIVE is defined:
 * - Key names are preserved in their original case
 * - Only whitespace trimming is performed
 * - Case-sensitive matching is used (e.g. "Key1" differs from "KEY1")
 * - Appropriate when key name case carries semantic meaning
 * - Required for systems with case-sensitive configuration requirements
 *
 * Example usage:
 * @code
 * std::string key = "DatabasePort";
 * std::string normalized = INI_FILE_KEY_NORMALIZE(key);
 * // Without INI_FILE_KEY_NAME_CASE_SENSITIVE: normalized == "databaseport"
 * // With INI_FILE_KEY_NAME_CASE_SENSITIVE: normalized == "DatabasePort"
 * @endcode
 *
 * @warning Must be used consistently throughout the codebase to maintain uniform behavior
 * @warning Case sensitivity affects key lookups and duplicate detection
 * @warning Changing case sensitivity settings may break existing configurations
 *
 * @see IniFile::to_lower() For the lowercase conversion implementation
 * @see IniFile::trim() For whitespace removal implementation
 * @see INI_FILE_SECTION_NAME_CASE_SENSITIVE For related section name handling
 */
#ifndef INI_FILE_KEY_NAME_CASE_SENSITIVE
    #define INI_FILE_KEY_NORMALIZE(s)    ::IniFile::to_lower(::IniFile::trim(s))
#else
    #define INI_FILE_KEY_NORMALIZE(s)    ::IniFile::trim(s)
#endif


/**
 * @brief Main class for parsing, manipulating and writing INI configuration files
 *
 * @details
 * The IniFile class provides a complete solution for working with INI format configuration files.
 * It supports:
 * - Reading/writing INI files from disk or streams
 * - Case-sensitive or case-insensitive section/key names (configurable at compile time)
 * - Section-based organization of configuration data
 * - Type-safe value access with automatic conversion
 * - Detailed error reporting and validation
 * - Proper handling of common INI file features:
 *   - Comments (lines starting with ; or #)
 *   - Section headers in [brackets]
 *   - Key=Value pairs
 *   - Multi-line values
 *   - Default section for keys before first section header
 *
 * Key Features:
 * - Robust error handling with line numbers and detailed messages
 * - Configurable case sensitivity for section and key names
 * - Type-safe value access with conversion validation
 * - Support for multiple value types (string, numeric, boolean)
 * - Preservation of file structure during write operations
 * - Memory-efficient storage using string-based containers
 *
 * Usage Example:
 * @code
 * IniFile ini;
 * if (ini.load("config.ini")) {
 *     std::string value;
 *     if (ini["section"].get("key", value)) {
 *         // Use value
 *     }
 * }
 * @endcode
 *
 * @note All section and key names are normalized according to INI_FILE_SECTION_NORMALIZE
 *       and INI_FILE_KEY_NORMALIZE macros respectively
 *
 * @warning Thread safety must be handled by the caller
 *
 * @see Section For accessing individual configuration sections
 * @see Message For error and warning information
 * @see INI_FILE_SECTION_NORMALIZE For section name normalization rules
 * @see INI_FILE_KEY_NORMALIZE For key name normalization rules
 */
class IniFile {
    public:
        /**
         * @brief Enumeration class defining severity levels for parsing messages
         * 
         * @details
         * This enum class provides two severity levels for messages generated during INI file parsing:
         * 
         * Warning:
         * - Indicates non-critical issues that don't prevent parsing
         * - Examples include:
         *   - Duplicate section names (later sections override earlier ones)
         *   - Duplicate keys within a section (later values override earlier ones)
         *   - Empty lines or comment-only lines
         * - Program can continue execution with warnings
         * 
         * Error:
         * - Indicates critical issues that may affect parsing correctness
         * - Examples include:
         *   - Malformed section headers
         *   - Invalid key-value pair syntax
         *   - File access/IO errors
         *   - Keys outside any section
         * - May prevent successful parsing or lead to incomplete/incorrect results
         * 
         * Using enum class rather than plain enum provides:
         * - Type safety
         * - Scoped enumerators
         * - No implicit conversions
         * 
         * @see Message For the structure containing message details
         * @see IniFile::errors() For accessing collected messages
         */
        enum class MessageLevel {
            Warning,    /**< Non-critical issues that don't prevent parsing */
            Error      /**< Critical issues that may affect parsing correctness */
        };

    public:
        /**
         * @brief Structure containing details about a parsing message
         * 
         * @details
         * This structure encapsulates all information about a message generated
         * during INI file parsing. Each Message instance contains:
         * 
         * Members:
         * @var level MessageLevel indicating severity (Warning or Error)
         * @var line Size_t storing the line number where issue occurred (1-based)
         * @var message String containing detailed description of the issue
         * 
         * Usage:
         * - Created during parsing when issues are encountered
         * - Stored in errors_type vector for later retrieval
         * - Line numbers help locate issues in source file
         * - Messages provide specific details about what went wrong
         * 
         * Example message contents:
         * - "Duplicate section 'config' at line 42"
         * - "Invalid key-value pair syntax at line 17"
         * - "File access error: permission denied"
         * 
         * @note Line number 0 indicates file-level issues (e.g., IO errors)
         * @see MessageLevel For severity level definitions
         * @see errors_type For the container storing Message instances
         */
        struct Message {
            MessageLevel    level;      /**< Severity level of the message */
            std::size_t     line;       /**< Line number where issue occurred (0 for file-level issues) */
            std::string     message;    /**< Detailed description of the issue */
        };

    public:
        /**
         * @brief Type alias for a section's key-value storage
         * 
         * @details
         * Defines the storage type for a single INI file section:
         * - Uses std::map for efficient key lookup
         * - Keys are normalized strings (case handling depends on INI_FILE_KEY_NAME_CASE_SENSITIVE)
         * - Values stored as raw strings, converted when accessed
         * - Maintains sorted order of keys
         * - Provides O(log n) lookup complexity
         * 
         * The map structure:
         * - Key: Normalized key name string
         * - Value: Raw string value from INI file
         * 
         * @note Keys are normalized using INI_FILE_KEY_NORMALIZE macro
         * @see INI_FILE_KEY_NORMALIZE
         * @see Section class which manages this storage type
         */
        using section_type = std::map<std::string, std::string>;

        /**
         * @brief Type alias for the main INI file data storage
         * 
         * @details
         * Defines the primary storage structure for all INI file data:
         * - Uses nested std::map for section -> key-value organization
         * - Outer map keys are normalized section names
         * - Inner map is section_type containing key-value pairs
         * - Maintains sorted order of sections
         * - Provides O(log n) section lookup
         * 
         * The map structure:
         * - Key: Normalized section name string
         * - Value: section_type map containing section's key-value pairs
         * 
         * @note Section names normalized using INI_FILE_SECTION_NORMALIZE macro
         * @see section_type
         * @see INI_FILE_SECTION_NORMALIZE
         */
        using data_type       = std::map<std::string, section_type>;

        /**
         * @brief Type alias for storing parse errors and warnings
         * 
         * @details
         * Defines the storage type for parsing messages:
         * - Uses std::vector for sequential message storage
         * - Maintains order of errors/warnings as encountered
         * - Stores Message structs containing:
         *   - MessageLevel (Error/Warning)
         *   - Line number where issue occurred
         *   - Descriptive message text
         * - Vector allows efficient append operations
         * - Supports iteration over all messages
         * 
         * @note Vector is cleared before each parse operation
         * @see Message struct
         * @see MessageLevel enum class
         */
        using errors_type     = std::vector<Message>;

    public:
        /**
         * @brief A class that provides an interface for managing a single section of an INI file
         * 
         * @details
         * The Section class encapsulates access to key-value pairs within a single INI file section.
         * It provides methods for:
         * - Getting values with type conversion and default fallbacks
         * - Setting values of various types (string, numeric, boolean)
         * - Checking for key existence
         * - Normalizing keys according to case sensitivity rules
         *
         * Key features:
         * - Holds reference to underlying section data map
         * - Provides type-safe access through templates
         * - Handles string-to-type conversions
         * - Maintains consistent key normalization
         * - Supports default values for missing keys
         *
         * Implementation details:
         * - Uses std::map for key-value storage
         * - Keys are normalized strings (case-sensitive or insensitive)
         * - Values stored as strings, converted on access
         * - SFINAE templates for type-safe conversions
         * - Exception handling for conversion safety
         *
         * @note This class is designed to be created by IniFile class methods
         * and should not typically be instantiated directly by users
         *
         * @warning The Section object must not outlive its referenced section data
         * @warning Key normalization must match IniFile's case sensitivity settings
         *
         * @see IniFile The containing class that manages all sections
         * @see section_type The underlying map type for section data
         * @see INI_FILE_KEY_NORMALIZE For key normalization rules
         */
        class Section {
            private:
                /**
                 * @brief Reference to the underlying section data map
                 * 
                 * @details
                 * This member variable holds a reference to a section_type (std::map<std::string,std::string>) that contains
                 * the actual key-value pairs for this section. Key features:
                 * - Reference member ensures Section always has valid data to work with
                 * - Cannot be reassigned after construction due to reference semantics
                 * - Multiple Section objects can share the same underlying section data
                 * - Changes through this reference directly modify the original section map
                 * - Keys are normalized according to case sensitivity settings
                 * - Values are stored as raw strings and converted on access
                 *
                 * @note This is a reference member, which requires:
                 * - Initialization in constructor's member initializer list
                 * - Deletion of default constructor
                 * - Deletion of copy assignment operator
                 * - Special care when copying Section objects
                 *
                 * @see section_type For the underlying map type definition
                 * @see INI_FILE_KEY_NORMALIZE For key normalization rules
                 * @see Section::Section For initialization details
                 */
                section_type& m_section;

            public:
                /**
                 * @brief Default constructor is deleted
                 * 
                 * @details
                 * The default constructor is explicitly deleted because:
                 * - Section objects must be initialized with a reference to a section_type
                 * - There is no valid default state for the m_section reference member
                 * - Prevents creation of Section objects without proper initialization
                 */
                Section() = delete;

                /**
                 * @brief Constructs a Section that references an existing section_type
                 * 
                 * @param section Reference to the section_type this Section will manage
                 * 
                 * @details
                 * Primary constructor that:
                 * - Takes a reference to an existing section_type map
                 * - Initializes the m_section reference member to refer to that map
                 * - Allows the Section object to modify the referenced section data
                 * - Reference member ensures Section always has valid data to work with
                 */
                Section(section_type& section) : m_section(section) {}

                /**
                 * @brief Copy constructor that shares the section reference
                 * 
                 * @param other The Section object to copy from
                 * 
                 * @details
                 * Copy constructor implementation:
                 * - Initializes m_section to refer to the same section_type as other
                 * - Both Section objects will share and modify the same underlying data
                 * - Shallow copy is appropriate since Section only manages references
                 * - No deep copy needed as Section doesn't own the data it references
                 */
                Section(const Section& other) : m_section(other.m_section) {}

                /**
                 * @brief Copy assignment operator is deleted
                 * 
                 * @details
                 * Copy assignment is explicitly deleted because:
                 * - m_section is a reference which cannot be reassigned
                 * - Section objects must maintain their initial reference
                 * - Prevents accidental reference reassignment
                 * - Forces users to create new Section objects instead of reassigning
                 * 
                 * @return Section& Reference to this object (not actually returned since deleted)
                 */
                Section& operator=(const Section& other) = delete;

            public:
                /**
                 * @brief Converts a string value to a signed integral type
                 * 
                 * @tparam T The target signed integral type (int, long, etc.) or char type
                 * @param value The string value to convert, supports decimal and hex (0x) formats
                 * @param result Reference to store the converted value
                 * @return bool True if conversion succeeded, false otherwise
                 * 
                 * @details
                 * This template specialization handles conversion to any signed integral type or char:
                 * - Uses SFINAE to match signed integral types and char specifically
                 * - Attempts to parse string using std::stoll for maximum range
                 * - Validates result fits within target type's range
                 * - Returns false if:
                 *   - String cannot be parsed as a number
                 *   - Value exceeds target type's range
                 *   - Any other conversion error occurs
                 */
                template<
                    typename T,
                    typename std::enable_if<
                        (std::is_integral<T>::value && std::is_signed<T>::value) || std::is_same<T, char>::value, int
                    >::type = 0
                >
                static bool convert(const std::string& value, T& result) {
                    try {
                        long long temp = std::stoll(value, nullptr, 0); // Parse as signed long long first
                        if (temp < std::numeric_limits<T>::min() || temp > std::numeric_limits<T>::max())
                            return false;

                        result = static_cast<T>(temp);
                        return true;
                    } catch (...) {
                        return false;
                    }
                }

            public:
                /**
                 * @brief Converts a string value to an unsigned integral type
                 * 
                 * @tparam T The target unsigned integral type (unsigned int, size_t, etc.)
                 * @param value The string value to convert
                 * @param result Reference to store the converted value
                 * @return bool True if conversion succeeded, false otherwise
                 * 
                 * @details
                 * This template specialization handles conversion to any unsigned integral type:
                 * - Uses SFINAE to only match unsigned integral types
                 * - Attempts to parse string using std::stoull for maximum range
                 * - Validates result fits within target type's range
                 * - Returns false if:
                 *   - String cannot be parsed as a number
                 *   - Value exceeds target type's range
                 *   - Any other conversion error occurs
                 */
                template<
                    typename T,
                    typename std::enable_if<
                        std::is_integral<T>::value&& std::is_unsigned<T>::value, int
                    >::type = 0
                >
                static bool convert(const std::string& value, T& result) {
                    try {
                        unsigned long long temp = std::stoull(value, nullptr, 0); // Parse as unsigned long long first
                        if (temp > std::numeric_limits<T>::max())
                            return false;
                        result = static_cast<T>(temp);
                        return true;
                    } catch (...) {
                        return false;
                    }
                }

            public:
                /**
                 * @brief Converts a string value to a floating point type
                 * 
                 * @tparam T The target floating point type (float, double, long double)
                 * @param value The string value to convert
                 * @param result Reference to store the converted value
                 * @return bool True if conversion succeeded, false otherwise
                 * 
                 * @details
                 * This template specialization handles conversion to any floating point type:
                 * - Uses SFINAE to only match floating point types
                 * - Selects appropriate conversion function based on target type:
                 *   - std::stof for float
                 *   - std::stod for double
                 *   - std::stold for long double
                 * - Returns false if:
                 *   - String cannot be parsed as a number
                 *   - Target type is not float/double/long double
                 *   - Any other conversion error occurs
                 */
                template<
                    typename T,
                    typename std::enable_if<
                        std::is_floating_point<T>::value, int
                    >::type = 0
                >
                static bool convert(const std::string& value, T& result) {
                    try {
                        if (std::is_same<T, float>::value)
                            result = std::stof(value);
                        else if (std::is_same<T, double>::value)
                            result = std::stod(value);
                        else if (std::is_same<T, long double>::value)
                            result = std::stold(value);
                        else
                            return false; // Should never reach here due to SFINAE

                        return true;
                    } catch (...) {
                        return false;
                    }
                }                

            public:
                /**
                 * @brief Converts a string value to a boolean
                 * 
                 * @param value The string value to convert
                 * @param result Reference to store the converted boolean
                 * @return bool True if conversion succeeded, false otherwise
                 * 
                 * @details
                 * Handles conversion of various string representations to boolean:
                 * - Case-insensitive comparison (converts to lowercase first)
                 * - Trims whitespace before comparison
                 * - True values:
                 *   - "true", "1", "yes", "on"
                 * - False values:
                 *   - "false", "0", "no", "off"
                 * - Returns false if string doesn't match any known boolean representation
                 */
                static bool convert(const std::string& value, bool& result) {
                    std::string x = ::IniFile::to_lower(::IniFile::trim(value));
                    if (x == "true" || x == "1" || x == "yes" || x == "on") {
                        result = true;
                        return true;
                    } else if (x == "false" || x == "0" || x == "no" || x == "off") {
                        result = false;
                        return true;
                    }
                    
                    return false;
                }

            public:
                /**
                 * @brief Converts a string value to a string (special case)
                 * 
                 * @param value The input string value to convert
                 * @param result Reference to store the converted string value
                 * @return bool True if conversion succeeded, false otherwise
                 * 
                 * @details
                 * This is a special case conversion function for string-to-string:
                 * - Simply assigns the input value directly to the result
                 * - No actual type conversion is performed
                 * - Returns true to indicate successful assignment
                 * - This behavior differs from other convert() overloads which may fail
                 * - The true return value indicates the value was successfully assigned
                 * 
                 * @note This function always succeeds since no actual conversion is needed
                 * @see Section::read() For how this is used in value retrieval
                 */
                static bool convert(const std::string& value, std::string& result) {
                    result = value;
                    return true;
                }

            public:
                /**
                 * @brief Reads a value from the section with type conversion and default fallback
                 * 
                 * @tparam T The type to convert the value to
                 * @param key The key to look up in the section
                 * @param value Reference to store the retrieved value
                 * @param default_value Value to use if key doesn't exist or conversion fails
                 * @return bool True if key exists and conversion succeeded, false otherwise
                 * 
                 * @details
                 * Reads and converts a value from the section:
                 * - Normalizes key name according to case sensitivity rules
                 * - Looks up key in section's key-value map
                 * - If key exists, attempts to convert string value to type T
                 * - If key doesn't exist or conversion fails, uses default_value
                 * - Returns success/failure of entire operation
                 * 
                 * @note The default_value is only used if:
                 * - The key does not exist in the section
                 * - The value cannot be converted to type T
                 * 
                 * @see Section::convert() For type conversion implementation
                 * @see INI_FILE_KEY_NORMALIZE For key normalization rules
                 */
                template<typename T>
                    bool read(const std::string& key, T& value, const T& default_value = T()) const {
                        auto it = m_section.find(INI_FILE_KEY_NORMALIZE(key));

                        if (it == m_section.end()) {
                            value = default_value;
                            return false;
                        }

                        bool x = convert(it->second, value);

                        if (!x)
                            value = default_value;

                        return x;
                    }

            public:
                /**
                 * @brief Gets a value from the section with type conversion and default fallback
                 * 
                 * @tparam T The type to convert the value to
                 * @param key The key to look up in the section
                 * @param default_value Value to return if key doesn't exist or conversion fails
                 * @return T The retrieved and converted value, or default_value on failure
                 * 
                 * @details
                 * Convenience wrapper around the reference-based read():
                 * - Creates temporary variable to store result
                 * - Calls reference-based read() to do actual work
                 * - Returns either converted value or default value
                 * - Useful when default value handling is simple
                 * 
                 * @note This is a convenience method that wraps read() for simpler usage
                 * when you don't need to know if the value was found or defaulted
                 * 
                 * @see Section::read() For the underlying implementation
                 */
                template<typename T>
                    T get(const std::string& key, const T& default_value) const {
                        T value{};
                        if (read(key, value, default_value))
                            return value;

                        return default_value;
                    }

            public:
                /**
                 * @brief Sets a string value for a key in the section
                 * 
                 * @param key The key to set in the section
                 * @param value The string value to store
                 * 
                 * @details
                 * Basic string value setter:
                 * - Normalizes key name according to case sensitivity rules
                 * - Stores value directly as string
                 * - Creates key if it doesn't exist, updates if it does
                 * 
                 * @note The value is stored as-is without any conversion
                 * @see INI_FILE_KEY_NORMALIZE For key normalization rules
                 */
                void set(const std::string& key, const std::string& value) {
                    m_section[INI_FILE_KEY_NORMALIZE(key)] = value;
                }
                
            public:
                /**
                 * @brief Sets a C-string value for a key in the section
                 * 
                 * @param key The key to set
                 * @param value The C-string value to store
                 * 
                 * @details
                 * C-string convenience overload:
                 * - Converts C-string to std::string
                 * - Delegates to string version of set()
                 */
                void set(const std::string& key, const char * value) {
                    set(key, std::string(value));
                }
                
            public:
                /**
                 * @brief Sets a numeric value for a key in the section
                 * 
                 * @tparam T The numeric type (integral or floating point)
                 * @param key The key to set
                 * @param value The numeric value to store
                 * 
                 * @details
                 * Numeric type setter:
                 * - Uses SFINAE to only match numeric types
                 * - Converts value to string using std::to_string
                 * - Delegates to string version of set()
                 */
                template<
                    typename T,
                    typename std::enable_if<
                        std::is_integral<T>::value || std::is_floating_point<T>::value, int
                    >::type = 0
                >
                void set(const std::string& key, const T& value) {
                    set(key, std::to_string(value));
                }

            public:
                /**
                 * @brief Sets a boolean value for a key in the section
                 * 
                 * @param key The key to set
                 * @param value The boolean value to store
                 * 
                 * @details
                 * Boolean specialization:
                 * - Converts bool to "true" or "false" string
                 * - Delegates to string version of set()
                 */
                void set(const std::string& key, bool value) {
                    set(key, std::string(value ? "true" : "false"));
                }

            public:
                /**
                 * @brief Checks if a key exists in the section
                 * 
                 * @param key The key to check for
                 * @return bool True if key exists, false otherwise
                 * 
                 * @details
                 * Key existence checker:
                 * - Normalizes key name according to case sensitivity rules
                 * - Uses map::find() to check for key
                 * - Does not modify section contents
                 */
                bool has(const std::string& key) const {
                    return m_section.find(INI_FILE_KEY_NORMALIZE(key)) != m_section.end();
                }
        };

    public:
        /**
         * @brief Default constructor
         * 
         * @details
         * Creates an empty INI file object:
         * - Initializes m_data as empty map of maps
         * - Initializes m_errors as empty vector
         * - No dynamic memory allocation occurs
         * - Exception safe since no resources are acquired
         *
         * @note The default section is automatically created to ensure
         *       all INI files have at least one section for key-value pairs
         */
        IniFile() = default;


        /**
         * @brief Default destructor
         * 
         * @details
         * Cleans up INI file object:
         * - Automatically cleans up m_data and m_errors
         * - std::map and std::vector handle their own cleanup
         * - No manual resource cleanup needed
         * - Exception safe since no resources need explicit release
         */
        ~IniFile() = default;

    private:
        /**
         * @brief Main storage for INI file data
         * 
         * @details
         * Stores all INI file data in a nested map structure:
         * - Outer map: section name -> inner map
         * - Inner map: key -> value pairs for each section
         * - All strings normalized according to case sensitivity rules
         * - Maintains insertion order of sections and keys
         * 
         * @see data_type For the type definition
         * @see section_type For the inner map type
         */
        data_type   m_data;

        /**
         * @brief Collection of error/warning messages from parsing
         * 
         * @details
         * Stores all messages generated during parsing:
         * - Stores Message structs with level, line number and text
         * - Cleared before each parse operation
         * - Used to track issues without interrupting parsing
         * - Allows caller to check success/validity after operations
         * 
         * @see errors_type For the type definition
         * @see Message For the message structure
         */
        errors_type m_errors;

    public:
        /**
         * @brief Name of the implicit default section
         * 
         * @details
         * Constant value defined in cpp file:
         * - Used for key-value pairs not in any named section
         * - Always exists even in empty files
         * - Written first when saving to file
         * - Name normalized using INI_FILE_SECTION_NORMALIZE
         * 
         * @see INI_FILE_SECTION_NORMALIZE For normalization rules
         */
        static const std::string DEFAULT_SECTION_NAME;

    public:
        /**
         * @brief Reads a value from a specific section and key, with a default fallback
         * 
         * @tparam T The type of value to retrieve (must match Section::read requirements)
         * @param section Name of the section to look up
         * @param key Name of the key within the section
         * @param value Reference where the retrieved value will be stored
         * @param default_value Value to use if section/key not found
         * @return bool True if value was found and retrieved, false if default was used
         * 
         * @details
         * This function attempts to read a value from the INI data:
         * 1. Normalizes the section name according to case sensitivity rules
         * 2. Searches for the specified section in the data map
         * 3. If section not found:
         *    - Sets value parameter to default_value
         *    - Returns false to indicate default was used
         * 4. If section found:
         *    - Creates temporary Section object to handle the lookup
         *    - Delegates to Section::read() to retrieve the value
         *    - Returns true/false based on key lookup success
         * 
         * @note The default_value is only used if:
         * - The section does not exist
         * - The key does not exist in the section
         * - The value cannot be converted to type T
         * 
         * @see Section::read() For the underlying implementation
         * @see INI_FILE_SECTION_NORMALIZE For section name normalization
         * @see INI_FILE_KEY_NORMALIZE For key name normalization
         */
        template<typename T>
            bool read(const std::string& section_name, const std::string& key, T& value, const T& default_value = T()) const {
                auto x = m_data.find(INI_FILE_SECTION_NORMALIZE(section_name));

                if (x == m_data.end()) {
                    value = default_value;
                    return false;
                }

                return Section(x->second).read(key, value, default_value);
            }

    public:
        /**
         * @brief Convenience wrapper that returns the value directly instead of through reference
         * 
         * @tparam T The type of value to retrieve (must match Section::read requirements)
         * @param section Name of the section to look up
         * @param key Name of the key within the section
         * @param default_value Value to return if section/key not found
         * @return T The retrieved value or default_value if not found
         * 
         * @details
         * This is a wrapper around the reference-based read() that:
         * 1. Creates a temporary value of type T
         * 2. Calls the reference-based read() to populate it
         * 3. Returns either the found value or default based on lookup success
         * 4. Provides a more convenient API when default value handling is simple
         * 
         * @note This is a convenience method that wraps read() for simpler usage
         * when you don't need to know if the value was found or defaulted
         * 
         * @see read() For the underlying implementation
         */
        template<typename T>
            T get(const std::string& section_name, const std::string& key, const T& default_value) const {
                T value{};
                if (read(section_name, key, value, default_value))
                    return value;

                return default_value;
            }

    public:
        /**
         * @brief Sets a value for a specific section and key
         * 
         * @tparam T The type of value to set (must match Section::set requirements)
         * @param section Name of the section to update
         * @param key Name of the key within the section
         * @param value Value to store
         * 
         * @details
         * This function updates or creates a value in the INI data:
         * 1. Normalizes the section name according to case sensitivity rules
         * 2. Uses operator[] to get or create the section map
         * 3. Creates temporary Section object to handle the update
         * 4. Delegates to Section::set() to store the value
         * 5. Creates section and/or key if they don't exist
         * 
         * @note The value is converted to string before storage
         * @see Section::set() For the underlying implementation
         * @see INI_FILE_SECTION_NORMALIZE For section name normalization
         * @see INI_FILE_KEY_NORMALIZE For key name normalization
         */
        template<typename T>
            void set(const std::string& section_name, const std::string& key, const T& value) {
                Section(m_data[INI_FILE_SECTION_NORMALIZE(section_name)]).set(key, value);
            }

    public:
        /**
         * @brief Parses INI format data from an input stream
         * 
         * @param in Input stream containing INI format data
         * @return bool True if parsing succeeded with no errors, false otherwise
         * 
         * @details
         * This function reads and parses INI format data line by line from the input stream:
         * - Handles section headers in [section] format
         * - Processes key=value pairs within sections
         * - Supports quoted values with both single and double quotes
         * - Handles comments starting with ; or #
         * - Normalizes section names and keys according to case sensitivity rules
         * - Collects errors and warnings during parsing
         * - Stores parsed data in internal map structure
         * 
         * Error Handling:
         * - Syntax errors (missing brackets, equals signs) generate warnings
         * - Invalid section names (containing whitespace) generate warnings
         * - Invalid key names (empty or containing whitespace) generate warnings
         * - Duplicate sections or keys generate warnings
         * - Keys found outside of sections generate warnings
         * - All errors/warnings are stored in m_errors with line numbers
         * 
         * Validation Rules:
         * - Section names must be enclosed in square brackets
         * - Section names cannot contain whitespace
         * - Keys must contain at least one character after normalization
         * - Keys cannot contain whitespace characters
         * - Values can be quoted (single or double quotes)
         * - Quoted values can contain escaped quotes
         * - Unquoted values can have trailing comments
         * 
         * @see m_data For the storage structure
         * @see m_errors For error collection
         * @see Message For error message structure
         * @see MessageLevel For error severity levels
         */
        bool parse(std::istream& in);

    public:
        /**
         * @brief Loads and parses an INI file from disk
         * 
         * @param filename Path to the INI file to load
         * @return bool True if file was successfully loaded and parsed, false otherwise
         * 
         * @details
         * Opens the specified file and reads its contents:
         * - Creates an input file stream
         * - Checks if file opened successfully
         * - Delegates to parse() for actual parsing
         * - Records file access errors in m_errors
         * - Returns false if file cannot be opened
         * 
         * @note The file must exist and be readable
         * @see parse() For the parsing implementation
         */
        bool load(const std::string& filename);

    public:
        /**
         * @brief Writes a single section's contents to an output stream
         * 
         * @param section The section (map of key-value pairs) to write
         * @param out Output stream to write to
         * @return bool True if write succeeded, false otherwise
         * 
         * @details
         * Formats and writes all key-value pairs in a section:
         * - Each pair written as "key = value"
         * - Adds newline after each pair
         * - Adds blank line after section
         * - Used internally by write(std::ostream&)
         * 
         * @note The section is written in its normalized form
         * @see INI_FILE_KEY_NORMALIZE For key normalization rules
         */
        bool write(const section_type& section, std::ostream& out) const;

    public:
        /**
         * @brief Writes entire INI file contents to an output stream
         * 
         * @param out Output stream to write to
         * @return bool True if write succeeded, false otherwise
         * 
         * @details
         * Writes all sections and their contents:
         * - Writes default section first
         * - Each section starts with [section_name]
         * - Delegates to write(section_type&) for section contents
         * - Maintains original section ordering
         * - Returns false if any write operation fails
         * 
         * Formatting Rules:
         * - Section headers are written as [section_name] followed by newline
         * - Key-value pairs are written as "key = value" followed by newline
         * - Equals sign is padded with spaces for readability
         * - Blank line is added after each section
         * - Default section is always written first
         * - Section names are written in their normalized form
         * - Keys are written in their normalized form
         * - Values are written as-is without modification
         * 
         * Error Handling:
         * - Returns false if any write operation fails
         * - Checks for stream errors after each write
         * - Handles stream failures gracefully
         * - Preserves partial output if error occurs
         * 
         * @see write(const section_type&, std::ostream&) For section writing details
         * @see DEFAULT_SECTION_NAME For default section handling
         * @see INI_FILE_SECTION_NORMALIZE For section name normalization
         * @see INI_FILE_KEY_NORMALIZE For key name normalization
         */
        bool write(std::ostream& out) const;
        
    public:
        /**
         * @brief Saves entire INI file contents to disk
         * 
         * @param filename Path where file should be saved
         * @return bool True if save succeeded, false otherwise
         * 
         * @details
         * Creates/overwrites file and saves INI data:
         * - Opens output file stream
         * - Checks if file opened successfully
         * - Delegates to write() for content formatting
         * - Returns false if file cannot be opened
         * - Truncates any existing file content
         * 
         * File Handling:
         * - Creates new file if it doesn't exist
         * - Overwrites existing file if it exists
         * - Uses platform-specific path separators
         * - Handles file permissions appropriately
         * - Ensures atomic write operation
         * - Closes file handle after writing
         * 
         * Error Conditions:
         * - Returns false if file cannot be opened
         * - Returns false if write operation fails
         * - Returns false if file system is read-only
         * - Returns false if path is invalid
         * - Returns false if disk is full
         * - Returns false if file is locked
         * 
         * @see write() For content formatting details
         * @see std::ofstream For file stream handling
         */
        bool save(const std::string& filename) const;

    public:
        /**
         * @brief Returns a const reference to the collection of parsing errors and warnings
         *
         * @return const errors_type& A constant reference to the internal errors collection
         *
         * @details
         * This method provides read-only access to any errors or warnings that occurred during
         * parsing of an INI file. The errors are stored internally in m_errors and may include:
         * 
         * - Syntax errors (missing brackets, equals signs, etc.)
         * - Invalid section names (containing whitespace)
         * - Invalid key names (empty or containing whitespace) 
         * - Duplicate sections or keys (generates warnings)
         * - Keys found outside of sections
         * - File access errors during load/save operations
         *
         * Each error/warning contains:
         * - The line number where it occurred (0 for file access errors)
         * - The severity level (Error or Warning)
         * - A descriptive message explaining the issue
         *
         * The collection is cleared before each parse operation to only show
         * errors from the most recent parse.
         *
         * @note The returned reference remains valid until the next parse operation
         * @see Message For error message structure
         * @see MessageLevel For severity level definitions
         * @see parse() For parsing implementation
         * @see load() For file loading
         */
        const errors_type& errors() const {
            return m_errors;
        }

    public:
        /**
         * @brief Checks if a section exists in the INI file
         *
         * @param section The name of the section to check. Will be normalized using INI_FILE_SECTION_NORMALIZE.
         * @return bool True if the section exists, false otherwise
         *
         * @details
         * This function checks for the existence of a section in the INI file's data structure.
         * The process:
         * 1. Takes a section name as input and normalizes it using INI_FILE_SECTION_NORMALIZE
         * 2. Uses std::map::find() to search m_data for the normalized section name
         * 3. Compares the result against m_data.end() to determine if section was found
         *
         * The search is case-insensitive due to the normalization of section names.
         * Using find() is more efficient than count() for existence checks.
         *
         * @note Section names are normalized according to INI_FILE_SECTION_NORMALIZE rules
         * to ensure consistent case-insensitive lookups
         *
         * @see INI_FILE_SECTION_NORMALIZE For normalization rules
         * @see m_data For the storage structure
         */
        bool has(const std::string& section_name) const {
            return m_data.find(INI_FILE_SECTION_NORMALIZE(section_name)) != m_data.end();
        };
        
    public:
        /**
         * @brief Checks if a specific key exists within a given section of the INI file
         *
         * @param section The name of the section to check. Will be normalized using INI_FILE_SECTION_NORMALIZE.
         * @param key The name of the key to check. Will be normalized using INI_FILE_KEY_NORMALIZE.
         * @return bool True if both the section and key exist, false otherwise
         *
         * @details
         * This function performs a two-step check:
         * 1. First looks for the section in m_data using the normalized section name
         * 2. If section exists, then searches for the key within that section's key-value map
         *
         * The search process:
         * - Normalizes section name for case-insensitive comparison
         * - Uses map::find() which is more efficient than count() for existence checks
         * - Only searches for key if section exists (short-circuit evaluation)
         * - Normalizes key name before searching within section
         *
         * @note Both section and key names are normalized according to the defined rules
         * to ensure consistent case-insensitive lookups
         *
         * @see INI_FILE_SECTION_NORMALIZE For section name normalization
         * @see INI_FILE_KEY_NORMALIZE For key name normalization
         * @see m_data For the storage structure
         */
        bool has(const std::string& section_name, const std::string& key) const {
            auto x = m_data.find(INI_FILE_SECTION_NORMALIZE(section_name));
            return x != m_data.end() && x->second.find(INI_FILE_KEY_NORMALIZE(key)) != x->second.end();
        };

    public:
        /**
         * @brief Provides array-style access to sections in the INI file
         *
         * @param section The name of the section to access. Will be normalized according to 
         *               INI_FILE_SECTION_NORMALIZE rules for case-insensitive comparison.
         * @return Section A wrapper object providing access to the key-value pairs in the section
         *
         * @details
         * This operator enables convenient array-style syntax for accessing INI file sections.
         * For example: ini_file["mysection"]["mykey"] = "myvalue";
         *
         * The function:
         * 1. Normalizes the section name using INI_FILE_SECTION_NORMALIZE
         * 2. Uses std::map::operator[] which creates the section if it doesn't exist
         * 3. Returns a Section wrapper that provides access to the key-value pairs
         *
         * @note Using operator[] will create an empty section if it doesn't exist,
         * unlike the has() method which only checks for existence.
         *
         * @see Section class For the returned wrapper object
         * @see INI_FILE_SECTION_NORMALIZE For normalization rules
         * @see m_data For the storage structure
         */
        Section operator[](const std::string& section_name) {
            return m_data[INI_FILE_SECTION_NORMALIZE(section_name)];
        }

    public:
        /**
         * @brief Provides const array-style access to sections in the INI file
         *
         * @param section The name of the section to access. Will be normalized according to
         *               INI_FILE_SECTION_NORMALIZE rules for case-insensitive comparison.
         * @return const Section A wrapper object providing read-only access to the key-value pairs in the section
         *
         * @details
         * This const operator enables convenient array-style syntax for accessing INI file sections
         * in read-only contexts. For example: auto value = ini_file["mysection"]["mykey"];
         *
         * The function:
         * 1. Normalizes the section name using INI_FILE_SECTION_NORMALIZE
         * 2. Checks if the section exists using the has() method
         * 3. If section doesn't exist, returns a reference to a static empty section_type
         * 4. If section exists, returns a const Section wrapper providing read-only access
         *
         * Key differences from non-const version:
         * - Does not create sections if they don't exist (safe for const operations)
         * - Returns const Section wrapper preventing modifications
         * - Uses static empty section for non-existent sections to avoid exceptions
         * - Uses const_cast to access m_data.at() while maintaining const-correctness
         *
         * @note Unlike the non-const operator[], this version will not create sections
         * if they don't exist, making it safe for const objects and read-only access.
         * @note The static empty section_type instance is shared across all calls and
         * provides a safe fallback for non-existent sections.
         *
         * @warning The returned Section object references either the actual section data
         * or a static empty section. Care must be taken not to store long-lived references.
         *
         * @see Section class For the returned wrapper object
         * @see INI_FILE_SECTION_NORMALIZE For normalization rules
         * @see has() For section existence checking
         * @see m_data For the storage structure
         */
        const Section operator[](const std::string& section_name) const {
            auto it = m_data.find(INI_FILE_SECTION_NORMALIZE(section_name));

            if (it == m_data.end()) {
                static section_type empty_instance;
                return empty_instance;
            }

            return const_cast<section_type&>(it->second);
        }

    private:
        /**
         * @brief Converts a string to lowercase
         *
         * @param s The input string to convert
         * @return A new string with all characters converted to lowercase
         *
         * @details
         * This function performs the following:
         * 1. Creates a new string of the same size as input, initialized with null chars
         * 2. Uses std::transform with ::tolower to convert each character
         * 3. Returns the resulting lowercase string
         *
         * The function:
         * - Preserves the original string (const parameter)
         * - Handles ASCII characters only (::tolower is used)
         * - Pre-allocates the result string for efficiency
         * - Processes each character exactly once
         * 
         * @note This uses C's tolower function which only works reliably for ASCII.
         * For Unicode support, a more sophisticated approach would be needed.
         *
         * @warning The behavior is undefined if the input string contains non-ASCII chars
         *
         * @see std::transform For the transformation algorithm
         * @see ::tolower For the character conversion function
         */
        static std::string to_lower(const std::string& s);

    private:
        /**
         * @brief Checks if a string contains any whitespace characters
         * 
         * @param str The input string to check for whitespace characters
         * @return true if the string contains any whitespace characters, false otherwise
         *
         * @details
         * This function performs a character-by-character scan of the input string to detect
         * any whitespace characters. The following are considered whitespace:
         * - Space ' ' (ASCII 32)
         * - Tab '\t' (ASCII 9)  
         * - Newline '\n' (ASCII 10)
         * - Carriage return '\r' (ASCII 13) 
         * - Form feed '\f' (ASCII 12)
         * - Vertical tab '\v' (ASCII 11)
         *
         * Implementation details:
         * - Uses range-based for loop for clean iteration over string characters
         * - Each character is cast to unsigned char before calling std::isspace()
         * - The cast prevents undefined behavior when char is signed and > 127
         * - Short-circuits on first whitespace character found (optimal performance)
         * - O(n) time complexity in worst case where n is string length
         * - O(1) space complexity as only single character is stored at a time
         * - Thread-safe since input is const and no shared state is modified
         *
         * @note This function is used for validating section and key names
         * @see std::isspace For the whitespace detection function
         */
        static bool has_whitespace(const std::string& str);

    private:
        /**
         * @brief Removes leading and trailing whitespace characters from a string
         *
         * @param s The input string to trim
         * @return A new string with leading and trailing whitespace removed
         *
         * @details
         * This function performs the following steps:
         * 1. Checks for empty input string and returns empty string if true
         * 2. Finds first non-whitespace character from the start
         * 3. Finds last non-whitespace character from the end
         * 4. Returns substring containing only non-whitespace content
         *
         * Whitespace characters include:
         * - Space ' '
         * - Tab '\t'
         * - Newline '\n' 
         * - Carriage return '\r'
         * - Form feed '\f'
         * - Vertical tab '\v'
         *
         * Characters are cast to unsigned char before checking to handle extended ASCII
         * characters correctly, avoiding undefined behavior with negative char values.
         *
         * Implementation details:
         * - Uses two indices (start, end) to track the bounds of non-whitespace content
         * - start index moves forward from beginning until non-whitespace found
         * - end index moves backward from end until non-whitespace found
         * - If start > end after scanning, string contained only whitespace
         * - std::isspace() used to detect whitespace characters
         * - static_cast to unsigned char prevents undefined behavior with extended ASCII
         * - substr() extracts final trimmed result using calculated bounds
         * - O(n) time complexity where n is string length
         * - O(1) space complexity excluding return value
         * - Thread-safe since input is const and no shared state
         * 
         * @note This function is used for normalizing section and key names
         * @see std::isspace For the whitespace detection function
         */
        static std::string trim(const std::string& s);
};

