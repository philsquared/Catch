
//              Copyright Catch2 Authors
// Distributed under the Boost Software License, Version 1.0.
//   (See accompanying file LICENSE.txt or copy at
//        https://www.boost.org/LICENSE_1_0.txt)

// SPDX-License-Identifier: BSL-1.0
#include <catch2/internal/catch_enforce.hpp>
#include <catch2/internal/catch_jsonwriter.hpp>
#include <catch2/internal/catch_polyfills.hpp>

#include <cmath>
#include <locale>

namespace Catch {

    namespace {
        struct EscapeLUT {
            bool escape[256] = {};
            constexpr EscapeLUT() {
                for ( unsigned int i = 0; i < 0x20; ++i ) {
                    escape[i] = true;
                }
                escape[static_cast<unsigned char>( '"' )] = true;
                escape[static_cast<unsigned char>( '\\' )] = true;
            }
        };
        static constexpr EscapeLUT escapeLUT{};

        static constexpr bool needsEscape( char c ) {
            return escapeLUT.escape[static_cast<unsigned char>( c )];
        }

        static void writeEscape( std::ostream& os, char c ) {
            if ( c == '"' ) {
                os << "\\\""_sr;
            } else if ( c == '\\' ) {
                os << "\\\\"_sr;
            } else if ( c == '\b' ) {
                os << "\\b"_sr;
            } else if ( c == '\f' ) {
                os << "\\f"_sr;
            } else if ( c == '\n' ) {
                os << "\\n"_sr;
            } else if ( c == '\r' ) {
                os << "\\r"_sr;
            } else if ( c == '\t' ) {
                os << "\\t"_sr;
            } else {
                auto const value = static_cast<unsigned char>( c );
                static constexpr char hex[] = "0123456789abcdef";
                char const escaped[] = {
                    '\\', 'u', '0', '0', hex[value >> 4], hex[value & 0xf] };
                os.write( escaped, sizeof( escaped ) );
            }
        }
    } // namespace

    void JsonUtils::indent( std::ostream& os, std::uint64_t level ) {
        for ( std::uint64_t i = 0; i < level; ++i ) {
            os << "  ";
        }
    }
    void JsonUtils::appendCommaNewline( std::ostream& os,
                                        bool& should_comma,
                                        std::uint64_t level ) {
        if ( should_comma ) { os << ','; }
        should_comma = true;
        os << '\n';
        indent( os, level );
    }

    JsonObjectWriter::JsonObjectWriter( std::ostream& os ):
        JsonObjectWriter{ os, 0 } {}

    JsonObjectWriter::JsonObjectWriter( std::ostream& os,
                                        std::uint64_t indent_level ):
        m_os{ os }, m_indent_level{ indent_level } {
        m_os << '{';
    }
    JsonObjectWriter::JsonObjectWriter( JsonObjectWriter&& source ) noexcept:
        m_os{ source.m_os },
        m_indent_level{ source.m_indent_level },
        m_should_comma{ source.m_should_comma },
        m_active{ source.m_active } {
        source.m_active = false;
    }

    JsonObjectWriter::~JsonObjectWriter() {
        if ( !m_active ) { return; }

        m_os << '\n';
        JsonUtils::indent( m_os, m_indent_level );
        m_os << '}';
    }

    JsonValueWriter JsonObjectWriter::write( StringRef key ) {
        JsonUtils::appendCommaNewline(
            m_os, m_should_comma, m_indent_level + 1 );

        m_os << '"' << key << "\": ";
        return JsonValueWriter{ m_os, m_indent_level + 1 };
    }

    JsonArrayWriter::JsonArrayWriter( std::ostream& os ):
        JsonArrayWriter{ os, 0 } {}
    JsonArrayWriter::JsonArrayWriter( std::ostream& os,
                                      std::uint64_t indent_level ):
        m_os{ os }, m_indent_level{ indent_level } {
        m_os << '[';
    }
    JsonArrayWriter::JsonArrayWriter( JsonArrayWriter&& source ) noexcept:
        m_os{ source.m_os },
        m_indent_level{ source.m_indent_level },
        m_should_comma{ source.m_should_comma },
        m_active{ source.m_active } {
        source.m_active = false;
    }
    JsonArrayWriter::~JsonArrayWriter() {
        if ( !m_active ) { return; }

        m_os << '\n';
        JsonUtils::indent( m_os, m_indent_level );
        m_os << ']';
    }

    JsonObjectWriter JsonArrayWriter::writeObject() {
        JsonUtils::appendCommaNewline(
            m_os, m_should_comma, m_indent_level + 1 );
        return JsonObjectWriter{ m_os, m_indent_level + 1 };
    }

    JsonArrayWriter JsonArrayWriter::writeArray() {
        JsonUtils::appendCommaNewline(
            m_os, m_should_comma, m_indent_level + 1 );
        return JsonArrayWriter{ m_os, m_indent_level + 1 };
    }

    JsonArrayWriter& JsonArrayWriter::write( bool value ) {
        return writeImpl( value );
    }

    JsonValueWriter::JsonValueWriter( std::ostream& os ):
        JsonValueWriter{ os, 0 } {}

    JsonValueWriter::JsonValueWriter( std::ostream& os,
                                      std::uint64_t indent_level ):
        m_os{ os }, m_indent_level{ indent_level } {
        // We use C locale so that writing of numerical values is locale-independent.
        m_sstream.imbue( std::locale::classic() );
    }

    JsonObjectWriter JsonValueWriter::writeObject() && {
        return JsonObjectWriter{ m_os, m_indent_level };
    }

    JsonArrayWriter JsonValueWriter::writeArray() && {
        return JsonArrayWriter{ m_os, m_indent_level };
    }

    void JsonValueWriter::write( Catch::StringRef value ) && {
        writeImpl( value, true );
    }

    void JsonValueWriter::write( float value ) && {
        writeFloatingPoint( value );
    }

    void JsonValueWriter::write( double value ) && {
        writeFloatingPoint( value );
    }

    void JsonValueWriter::write( bool value ) && {
        writeImpl( value ? "true"_sr : "false"_sr, false );
    }

    template <typename T>
    void JsonValueWriter::writeFloatingPoint( T value ) {
        if ( Catch::isnan( value ) ) {
            writeImpl( "NaN"_sr, false );
        } else if ( std::isinf( value ) ) {
            writeImpl( value < 0 ? "-Infinity"_sr : "Infinity"_sr, false );
        } else {
            m_sstream << value;
            writeImpl( m_sstream.str(), false );
        }
    }

    void JsonValueWriter::writeImpl( Catch::StringRef value, bool quote ) {
        // Escaping only makes sense for actual strings, which are passed
        // with `quote == true`. Non-quoted values are things like bools
        // and numbers, which cannot create inputs that need escaping.
        if ( !quote ) {
            m_os << value;
        } else {
            m_os << '"';
            size_t current_start = 0;
            for ( size_t i = 0; i < value.size(); ++i ) {
                if ( needsEscape( value[i] ) ) {
                    if ( current_start < i ) {
                        m_os
                            << value.substr( current_start, i - current_start );
                    }
                    writeEscape( m_os, value[i] );
                    current_start = i + 1;
                }
            }
            if ( current_start < value.size() ) {
                m_os << value.substr( current_start,
                                      value.size() - current_start );
            }
            m_os << '"';
        }
    }

} // namespace Catch
