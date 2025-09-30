/*
 * Copyright (c) 2020-2020 Mellanox Technologies LTD. All rights reserved.
 * Copyright (C) 2022-2022, NVIDIA CORPORATION & AFFILIATES. ALL RIGHTS RESERVED.
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

#ifndef _OUTPUT_CONTROL_H_
#define _OUTPUT_CONTROL_H_

#include <map>
#include <bitset>
#include <vector>
#include <string>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <algorithm>

#include <assert.h>

#include <infiniband/ibutils/trim.h>

class OutputControl
{
    public:
        typedef enum
        {
            OutputControl_Flag_None             = 0x00000000,
            OutputControl_Flag_Valid            = 0x00000001,
			OutputControl_Flag_UserFile         = 0x00000002,
			OutputControl_Flag_CustomFileName   = 0x00000004,
			OutputControl_Flag_File_Mask        = 0x00000006,

            OutputControl_Flag_AppDefault       = 0x00000100,
            OutputControl_Flag_All              = 0x00000200,
            OutputControl_Flag_Default          = 0x00000400,
            OutputControl_Flag_Special_Mask     = 0x00000700,

            OutputControl_Flag_Generic          = 0x00010000,
            OutputControl_Flag_CSV              = 0x00020000,
            OutputControl_Flag_Type_Mask        = 0x00030000,
        } Flags;

    public:
        class Identity
        {
            public:
                static const Identity Null;

            protected:
                Flags           m_flags;
                std::string     m_type;
                std::string     m_key;
                std::string     m_text;

            public:
                bool is_valid() const           { return m_flags & OutputControl_Flag_Valid;          }

                bool is_csv() const             { return m_flags & OutputControl_Flag_CSV;            }
                bool is_generic() const         { return m_flags & OutputControl_Flag_Generic;        }

                bool is_app_default() const     { return m_flags & OutputControl_Flag_AppDefault;     }
                bool is_all() const             { return m_flags & OutputControl_Flag_All;            }
                bool is_default() const         { return m_flags & OutputControl_Flag_Default;        }
                bool is_special() const         { return m_flags & OutputControl_Flag_Special_Mask;   }

                std::string type() const        { return m_type;                                      }
                std::string text() const        { return m_text;                                      }

                Flags flags() const             { return m_flags;                                     }

            protected:
                Identity()
                    : m_flags(OutputControl_Flag_None)
                {
                }

            protected:
                Identity(Flags flags);

            public:
                Identity(const std::string & text, Flags flags = OutputControl_Flag_None);

            protected:
                bool build_key();

            public:
                inline bool operator < (const Identity & value) const {
                    return m_key < value.m_key;
                }

                inline bool operator > (const Identity & value) const {
                    return m_key > value.m_key;
                }

                inline bool operator == (const Identity & value) const {
                    return m_key == value.m_key;
                }

                inline bool operator != (const Identity & value) const {
                    return m_key != value.m_key;
                }

            public:
                std::ostream & output(std::ostream & stream, const std::string & prefix) const;
                std::string to_string() const;

            friend class OutputControl;
        };

    public:
        class Aliases
        {
            public:
                typedef std::vector<std::string>         vec_t;
                typedef std::map<std::string, vec_t>     map_t;

            private:
                map_t m_data;

            public:
                inline const map_t & data() const { return m_data; }

            public:
                Aliases() { }

            private:
                inline std::string & prepare_key(const std::string & name, std::string & key) const
                {
                    std::transform(
                            name.begin(),
                            name.end(),
                            std::back_inserter(key),
                            ::tolower);

                    return trim(key);
                }

                inline std::string prepare_key(const std::string & name) const
                {
                    std::string key;
                    return prepare_key(name, key);
                }

            public:
                inline vec_t & operator[](const std::string & name) {
                    return m_data[prepare_key(name)];
                }

            public:
                inline bool exist(const std::string & name) const
                {
                    map_t::const_iterator x = m_data.find(prepare_key(name));
                    return x != m_data.end() && !x->second.empty();
                }

            public:
                inline size_t size(const std::string & name) const
                {
                    map_t::const_iterator x = m_data.find(prepare_key(name));
                    return x != m_data.end() ? x->second.size() : 0;
                }


            public:
                std::ostream & output(std::ostream & stream, const std::string & prefix) const
                {
                    stream << prefix << "Aliases:" << std::endl;

                    for(map_t::const_iterator x = m_data.begin(); x != m_data.end(); ++x) {
                        stream << std::left << prefix << '\t' << std::setw(15) << x->first << " : " << std::right;
                        const char * splitter = "";
                        for(vec_t::const_iterator y = x->second.begin(); y != x->second.end(); ++y) {
                            stream << splitter << '\"' << *y << '\"';
                            splitter = ", ";
                        }
                        stream << std::endl;
                    }

                    return stream;
                }
        };

        template<typename _Type>
            class Group
            {
                public:
                    typedef std::map<Identity, _Type> map_t;

                protected:
                    const Aliases &  m_aliases;

                protected:
                    std::string      m_name;
                    map_t            m_data;
                    Flags            m_types;

                public:
                    const std::string & name() const        { return m_name;    }
                    const Aliases &     aliases() const     { return m_aliases; }

                public:
                    Group(const Aliases & aliases, const std::string & name, Flags types)
                        : m_aliases(aliases), m_name(name), m_types(types)
                    {}

                public:
                    inline bool is_supported(const Identity & identity) const
                    {
                        Flags type = Flags(identity.flags() & OutputControl_Flag_Type_Mask);
                        return (identity.flags() & OutputControl_Flag_Valid) &&
                                Flags(m_types & type) == type;
                    }

                protected:
                    inline const Identity & internal_unset(const Identity & identity)
                    {
                        if(!is_supported(identity))
                            return Identity::Null;

                        m_data.erase(identity);
                        return identity;
                    }

                    inline const Identity & internal_set(const Identity & identity, const _Type & value)
                    {
                        if(!is_supported(identity))
                            return Identity::Null;

                        typename map_t::iterator x = m_data.find(identity);

                        if(identity.is_default())
                            if(x != m_data.end())
                                return x->second == value ? identity : Identity::Null;

                        if(x != m_data.end()) {
                            x->second = value;
                            return x->first;
                        }
                        else
                            return m_data.insert(std::make_pair(identity, value)).first->first;
                    }

                    inline const Identity & internal_get(const Identity & identity, _Type & value) const
                    {
                        if(!is_supported(identity))
                            return Identity::Null;

                        Flags flags = Flags((identity.flags() &
                                            OutputControl_Flag_Type_Mask) |
                                            OutputControl_Flag_Valid);

                        // Check 'OutputControl_Flag_All'
                        typename map_t::const_iterator
                            x = m_data.find(Identity(Flags(flags | OutputControl_Flag_All)));

                        if(x != m_data.end()) {
                            value = x->second;
                            return x->first;
                        }

                        // Check identity
                        x = m_data.find(identity);
                        if(x != m_data.end()) {
                            value = x->second;
                            return x->first;
                        }

                        return internal_get_default(identity, value);
                    }

                public:
                    const Identity & internal_get_default(const Identity & identity, _Type & value) const
                    {
                        if(!is_supported(identity))
                            return Identity::Null;

                        // Check 'OutputControl_Flag_All'
                        typename map_t::const_iterator
                            x = m_data.find(
                                    Identity(Flags((identity.flags() & OutputControl_Flag_Type_Mask) |
                                                    OutputControl_Flag_All)));

                        if(x != m_data.end()) {
                            value = x->second;
                            return x->first;
                        }

                        // Check 'OutputControl_Flag_Default'
                        x = m_data.find(
                                Identity(Flags((identity.flags() & OutputControl_Flag_Type_Mask) |
                                          OutputControl_Flag_Default)));

                        if(x != m_data.end()) {
                            value = x->second;
                            return x->first;
                        }

                        // Check 'OutputControl_Flag_SW'
                        x = m_data.find(
                                Identity(
                                    Flags(
                                        (identity.flags() & OutputControl_Flag_Type_Mask) |
                                        OutputControl_Flag_AppDefault)));

                        if(x == m_data.end())
                            return Identity::Null;

                        value = x->second;
                        return x->first;
                    }

                public:
                    // can be problem for return result by reference for unset method only
                    const Identity & unset(const Identity & identity)
                    {
                        if(!identity.is_generic() || identity.is_special())
                            return internal_unset(identity);
                        else {
                            typename Aliases::map_t::const_iterator x = m_aliases.data().find(identity.type());

                            if(x != m_aliases.data().end()) {
                                bool res = true;

                                for(typename Aliases::vec_t::const_iterator z = x->second.begin(); z != x->second.end(); ++z)
                                    res &= internal_unset(Identity(*z)).is_valid();

                                return res ? identity : Identity::Null;
                            }
                            else
                                return internal_unset(identity);
                        }
                    }

                    // UnSet(string) method CAN'T return Identity by reference, because in case multi-alias
                    // UnSet(Identity) will return received Identity and UnSet(string) will return value by reference
                    // that created on the stack.
                    inline const Identity unset(const std::string & key) {
                        return unset(Identity(key));
                    }

                public:
                    const Identity & set(const Identity & identity, const _Type & value)
                    {
                        if(!identity.is_generic() || identity.is_special())
                            return internal_set(identity, value);
                        else {
                            typename Aliases::map_t::const_iterator x = m_aliases.data().find(identity.type());

                            if(x != m_aliases.data().end()) {
                                bool res = true;

                                for(typename Aliases::vec_t::const_iterator z = x->second.begin(); z != x->second.end(); ++z)
                                    res &= internal_set(Identity(*z), value).is_valid();

                                return res ? identity : Identity::Null;
                            }
                            else
                                return internal_set(identity, value);
                        }
                    }

                    // Set(string) method CAN'T return Identity by reference, because in case multi-alias
                    // Set(Identity) will return received Identity and Set(string) will return value by reference
                    // that created on the stack.
                    inline const Identity set(const std::string & key, const _Type & value) {
                        return set(Identity(key), value);
                    }

                public:
                    const Identity & get(const Identity & identity, _Type & value) const
                    {
                        if(!identity.is_generic() || identity.is_special())
                            return internal_get(identity, value);

                        typename Aliases::map_t::const_iterator x = m_aliases.data().find(identity.type());

                        if(x != m_aliases.data().end()) {
                            if(x->second.empty())
                                return internal_get(identity, value);

                            if(x->second.size() == 1)
                                return internal_get(Identity(*x->second.begin()), value);
                        }

                        if(m_aliases.exist(identity.type()))
                            return Identity::Null;

                        return internal_get(identity, value);
                    }

                    // Get(string) method CAN return Identity by reference,
                    // because Get(Identity) not support multi-alias
                    // and every time return identity from map
                    inline const Identity & get(const std::string & key, _Type & value) {
                        return get(Identity(key), value);
                    }

                public:
                    void clear()
                    {
                        map_t new_data;

                        for(typename map_t::const_iterator x = m_data.begin(); x != m_data.end(); ++x)
                            if(x->first.flags() && OutputControl_Flag_AppDefault)
                                new_data[x->first] = x->second;

                        m_data.swap(new_data);
                    }

                public:
                    std::ostream & output(std::ostream & stream, const std::string & prefix = "")
                    {
                        stream << prefix << "OutputControl::Group \'" << name() << "\'" << std::endl;

                        m_aliases.output(stream, prefix + '\t');
                        stream << prefix << std::endl;

                        stream << prefix << '\t' << "Map:" << std::endl;
                        for(typename map_t::const_iterator x = m_data.begin(); x != m_data.end(); ++x)
                            stream << prefix << '\t' << '\t'
                                   << std::left << std::setw(15) << x->first.to_string() << std::right
                                   << " : " << x->second << std::endl;

                        stream << prefix << std::endl;
                        return stream;
                    }
        };

    public:
        class AppSettings {
           public:
               bool m_csv_enabled;
               bool m_file_enabled;

               std::string m_csv_path;
               std::string m_file_path;

               bool m_csv_to_file;

               bool m_csv_compressed;
               bool m_file_compressed;

               bool m_csv_binary;

               bool m_csv_in_summary;
               bool m_file_in_summary;

               std::string m_csv_filename;
               std::string m_file_filename;

           public:
               AppSettings();
               void init(const std::string & app_name);
        };

    public:
        static AppSettings app_settings;

    private:
        Aliases                     m_aliases;

    private:
        Group<bool>                 m_enabled;
        Group<std::string>          m_pathes;
        Group<bool>                 m_csv_to_file;
        Group<bool>                 m_compression;
        Group<bool>                 m_binary;
        Group<bool>                 m_in_summary;

    public:
        Group<bool> &               enabled()               { return m_enabled;     }
        const Group<bool> &         enabled() const         { return m_enabled;     }

        Group<std::string> &        pathes()                { return m_pathes;      }
        const Group<std::string> &  pathes() const          { return m_pathes;      }

        Group<bool> &               csv_to_file()           { return m_csv_to_file; }
        const Group<bool> &         csv_to_file() const     { return m_csv_to_file; }

        Group<bool> &               compression()           { return m_compression; }
        const Group<bool> &         compression() const     { return m_compression; }

        Group<bool> &               binary()                { return m_binary;      }
        const Group<bool> &         binary() const          { return m_binary;      }

        Group<bool> &               in_summary()            { return m_in_summary;  }
        const Group<bool> &         in_summary() const      { return m_in_summary;  }

        Aliases &                   aliases()               { return m_aliases;     }
        const Aliases &             aliases() const         { return m_aliases;     }

    private:
        OutputControl();

    public:
        static OutputControl & instance()
        {
            static OutputControl _instance;
            return _instance;
        }

    public:
        static std::ostream & output(std::ostream & stream, const std::string & prefix);


    public:
        class Properties
        {
            private:
                Identity            m_identity;
                bool                m_is_valid;
                bool                m_enabled;
                std::string         m_path;
                bool                m_csv_to_file;
                bool                m_compression;
                bool                m_binary;
                bool                m_in_summary;

            public:
                Identity            identity() const      { return m_identity;      }
                bool                is_valid() const      { return m_is_valid;      }
                bool                enabled() const       { return m_enabled;       }
                const std::string & path() const          { return m_path;          }
                bool                csv_to_file() const   { return m_csv_to_file;   }
                bool                compression() const   { return m_compression;   }
                bool                binary() const        { return m_binary;        }
                bool                in_summary() const    { return m_in_summary;    }

            public:
                Properties(const Identity & identity)
                    : m_identity(identity), m_is_valid(false), m_enabled(false),
                      m_csv_to_file(false), m_compression(false), m_binary(false), m_in_summary(false)
                {
                    init();
                }

                Properties(const std::string & text)
                    : m_identity(Identity(text)), m_is_valid(false), m_enabled(false),
                      m_csv_to_file(false), m_compression(false), m_binary(false), m_in_summary(false)
                {
                    init();
                }

            protected:
                void init();
                bool build_generic_path(const std::string & path);

            public:
                std::ostream & output(std::ostream & stream, const std::string & prefix);
        };

    public:
        static bool CreateFolder(const std::string & full_path);
};

#endif /* _OUTPUT_CONTROL_H_ */

