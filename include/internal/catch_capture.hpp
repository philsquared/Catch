/*
 *  Created by Phil on 18/10/2010.
 *  Copyright 2010 Two Blue Cubes Ltd. All rights reserved.
 *
 *  Distributed under the Boost Software License, Version 1.0. (See accompanying
 *  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 */
#ifndef TWOBLUECUBES_CATCH_CAPTURE_HPP_INCLUDED
#define TWOBLUECUBES_CATCH_CAPTURE_HPP_INCLUDED

#include "catch_expression_decomposer.hpp"
#include "catch_expressionresult_builder.h"
#include "catch_message.h"
#include "catch_interfaces_capture.h"
#include "catch_debugger.hpp"
#include "catch_context.h"
#include "catch_common.h"
#include "catch_tostring.hpp"
#include "catch_interfaces_reporter.h"
#include "internal/catch_compiler_capabilities.h"

#include <ostream>

namespace Catch {

    inline IResultCapture& getResultCapture() {
        return getCurrentContext().getResultCapture();
    }

    template<typename MatcherT>
    ExpressionResultBuilder expressionResultBuilderFromMatcher( MatcherT const& matcher,
                                                                std::string const& matcherCallAsString ) {
        std::string matcherAsString = matcher.toString();
        if( matcherAsString == "{?}" )
            matcherAsString = matcherCallAsString;
        return ExpressionResultBuilder()
            .setRhs( matcherAsString )
            .setOp( "matches" );
    }

    template<typename MatcherT, typename ArgT>
    ExpressionResultBuilder expressionResultBuilderFromMatcher( MatcherT const& matcher,
                                                                ArgT const& arg,
                                                                std::string const& matcherCallAsString ) {
        return expressionResultBuilderFromMatcher( matcher, matcherCallAsString )
            .setLhs( Catch::toString( arg ) )
            .setResultType( matcher.match( arg ) );
    }

    template<typename MatcherT, typename ArgT>
    ExpressionResultBuilder expressionResultBuilderFromMatcher( MatcherT const& matcher,
                                                                ArgT* arg,
                                                                std::string const& matcherCallAsString ) {
        return expressionResultBuilderFromMatcher( matcher, matcherCallAsString )
            .setLhs( Catch::toString( arg ) )
            .setResultType( matcher.match( arg ) );
    }

#if defined(INTERNAL_CATCH_VS_MANAGED)
    // TestFailureException not defined for CLR
#else // detect CLR
struct TestFailureException{};
#endif

} // end namespace Catch

///////////////////////////////////////////////////////////////////////////////
#define INTERNAL_CATCH_ASSERTIONINFO_NAME INTERNAL_CATCH_UNIQUE_NAME( __assertionInfo )

#if !defined(INTERNAL_CATCH_VS_MANAGED) && !defined(INTERNAL_CATCH_VS_NATIVE)

    // normal Catch
    #define INTERNAL_CATCH_TEST_FAILURE_EXCEPTION const Catch::TestFailureException&
    #define INTERNAL_CATCH_TEST_THROW_FAILURE throw Catch::TestFailureException();

#else   // VS integration

#if defined(INTERNAL_CATCH_VS_MANAGED)

    #define INTERNAL_CATCH_TEST_THROW_FAILURE \
    { \
        Catch::IResultCapture& cap = Catch::getResultCapture(); \
        const Catch::AssertionResult* r = cap.getLastResult(); \
        std::stringstream _sf; \
        _sf << r->getExpressionInMacro().c_str() << ", " << r->getMessage().c_str(); \
        std::string fail = _sf.str(); \
        Assert::Fail(Catch::convert_string_to_managed(fail)); \
    }

    #define INTERNAL_CATCH_TEST_FAILURE_EXCEPTION AssertFailedException^
#else

#if defined(INTERNAL_CATCH_VS_NATIVE)
    #define INTERNAL_CATCH_TEST_THROW_FAILURE \
    { \
        Catch::IResultCapture& cap = Catch::getResultCapture(); \
        const Catch::AssertionResult* r = cap.getLastResult(); \
        std::wstringstream _s; \
        _s << r->getSourceInfo().file.c_str(); \
        std::wstring ws = _s.str(); \
        std::string testName = cap.getCurrentTestName(); \
        __LineInfo li(ws.c_str(), testName.c_str(), r->getSourceInfo().line); \
        std::wstringstream _sf; \
        _sf << r->getExpandedExpression().c_str() << ", " << r->getMessage().c_str(); \
        std::wstring ws2 = _sf.str(); \
        Assert::Fail(ws2.c_str(), &li); \
    }

    #define INTERNAL_CATCH_TEST_FAILURE_EXCEPTION const Catch::TestFailureException&

#endif // INTERNAL_CATCH_VS_MANAGED

#endif // detect CLR

#endif   // VS integration

///////////////////////////////////////////////////////////////////////////////
#define INTERNAL_CATCH_ACCEPT_EXPR( evaluatedExpr, resultDisposition, originalExpr ) \
    if( Catch::ResultAction::Value internal_catch_action = Catch::getResultCapture().acceptExpression( evaluatedExpr, INTERNAL_CATCH_ASSERTIONINFO_NAME )  ) { \
        if( internal_catch_action & Catch::ResultAction::Debug ) BreakIntoDebugger(); \
        if( internal_catch_action & Catch::ResultAction::Abort ) { INTERNAL_CATCH_TEST_THROW_FAILURE } \
        if( !Catch::shouldContinueOnFailure( resultDisposition ) ) { INTERNAL_CATCH_TEST_THROW_FAILURE } \
        Catch::isTrue( false && originalExpr ); \
    }

///////////////////////////////////////////////////////////////////////////////
#define INTERNAL_CATCH_ACCEPT_INFO( expr, macroName, resultDisposition ) \
    Catch::AssertionInfo INTERNAL_CATCH_ASSERTIONINFO_NAME( macroName, CATCH_INTERNAL_LINEINFO, expr, resultDisposition );

///////////////////////////////////////////////////////////////////////////////
#define INTERNAL_CATCH_TEST( expr, resultDisposition, macroName ) \
    do { \
        INTERNAL_CATCH_ACCEPT_INFO( #expr, macroName, resultDisposition ); \
        try { \
            INTERNAL_CATCH_ACCEPT_EXPR( ( Catch::ExpressionDecomposer()->*expr ).endExpression( resultDisposition ), resultDisposition, expr ); \
        } catch( INTERNAL_CATCH_TEST_FAILURE_EXCEPTION ) { \
            throw; \
        } catch( ... ) { \
            INTERNAL_CATCH_ACCEPT_EXPR( Catch::ExpressionResultBuilder( Catch::ResultWas::ThrewException ) << Catch::translateActiveException(), \
                resultDisposition | Catch::ResultDisposition::ContinueOnFailure, expr ); \
        } \
    } while( Catch::isTrue( false ) )

///////////////////////////////////////////////////////////////////////////////
#define INTERNAL_CATCH_IF( expr, resultDisposition, macroName ) \
    INTERNAL_CATCH_TEST( expr, resultDisposition, macroName ); \
    if( Catch::getResultCapture().getLastResult()->succeeded() )

///////////////////////////////////////////////////////////////////////////////
#define INTERNAL_CATCH_ELSE( expr, resultDisposition, macroName ) \
    INTERNAL_CATCH_TEST( expr, resultDisposition, macroName ); \
    if( !Catch::getResultCapture().getLastResult()->succeeded() )

///////////////////////////////////////////////////////////////////////////////
#define INTERNAL_CATCH_NO_THROW( expr, resultDisposition, macroName ) \
    do { \
        INTERNAL_CATCH_ACCEPT_INFO( #expr, macroName, resultDisposition ); \
        try { \
            expr; \
            INTERNAL_CATCH_ACCEPT_EXPR( Catch::ExpressionResultBuilder( Catch::ResultWas::Ok ), resultDisposition, false ); \
        } \
        catch( ... ) { \
            INTERNAL_CATCH_ACCEPT_EXPR( Catch::ExpressionResultBuilder( Catch::ResultWas::ThrewException ) << Catch::translateActiveException(), resultDisposition, false ); \
        } \
} while( Catch::isTrue( false ) )

///////////////////////////////////////////////////////////////////////////////
#define INTERNAL_CATCH_THROWS_IMPL( expr, exceptionType, resultDisposition ) \
    try { \
        if( Catch::getCurrentContext().getConfig()->allowThrows() ) { \
            expr; \
            INTERNAL_CATCH_ACCEPT_EXPR( Catch::ExpressionResultBuilder( Catch::ResultWas::DidntThrowException ), resultDisposition, false ); \
        } \
    } \
    catch( INTERNAL_CATCH_TEST_FAILURE_EXCEPTION ) { \
        throw; \
    } \
    catch( exceptionType ) { \
        INTERNAL_CATCH_ACCEPT_EXPR( Catch::ExpressionResultBuilder( Catch::ResultWas::Ok ), resultDisposition, false ); \
    }

///////////////////////////////////////////////////////////////////////////////
#define INTERNAL_CATCH_THROWS( expr, exceptionType, resultDisposition, macroName ) \
    do { \
        INTERNAL_CATCH_ACCEPT_INFO( #expr, macroName, resultDisposition ); \
        INTERNAL_CATCH_THROWS_IMPL( expr, exceptionType, resultDisposition ) \
    } while( Catch::isTrue( false ) )

///////////////////////////////////////////////////////////////////////////////
#define INTERNAL_CATCH_THROWS_AS( expr, exceptionType, resultDisposition, macroName ) \
    do { \
        INTERNAL_CATCH_ACCEPT_INFO( #expr, macroName, resultDisposition ); \
        INTERNAL_CATCH_THROWS_IMPL( expr, exceptionType, resultDisposition ) \
        catch( ... ) { \
            INTERNAL_CATCH_ACCEPT_EXPR( ( Catch::ExpressionResultBuilder( Catch::ResultWas::ThrewException ) << Catch::translateActiveException() ), \
                resultDisposition | Catch::ResultDisposition::ContinueOnFailure, false ); \
        } \
    } while( Catch::isTrue( false ) )


///////////////////////////////////////////////////////////////////////////////
#define INTERNAL_CATCH_MSG( log, messageType, resultDisposition, macroName ) \
    do { \
        INTERNAL_CATCH_ACCEPT_INFO( "", macroName, resultDisposition ); \
        INTERNAL_CATCH_ACCEPT_EXPR( Catch::ExpressionResultBuilder( messageType ) << log, resultDisposition, true ) \
    } while( Catch::isTrue( false ) )

///////////////////////////////////////////////////////////////////////////////
#define INTERNAL_CATCH_INFO( log, macroName ) \
    Catch::ScopedMessage INTERNAL_CATCH_UNIQUE_NAME( scopedMessage ) = Catch::MessageBuilder( macroName, CATCH_INTERNAL_LINEINFO, Catch::ResultWas::Info ) << log;


///////////////////////////////////////////////////////////////////////////////
#define INTERNAL_CHECK_THAT( arg, matcher, resultDisposition, macroName ) \
    do { \
        INTERNAL_CATCH_ACCEPT_INFO( #arg " " #matcher, macroName, resultDisposition ); \
        try { \
            INTERNAL_CATCH_ACCEPT_EXPR( ( Catch::expressionResultBuilderFromMatcher( ::Catch::Matchers::matcher, arg, #matcher ) ), resultDisposition, false ); \
        } catch( INTERNAL_CATCH_TEST_FAILURE_EXCEPTION ) { \
            throw; \
        } catch( ... ) { \
            INTERNAL_CATCH_ACCEPT_EXPR( ( Catch::ExpressionResultBuilder( Catch::ResultWas::ThrewException ) << Catch::translateActiveException() ), \
                resultDisposition | Catch::ResultDisposition::ContinueOnFailure, false ); \
        } \
    } while( Catch::isTrue( false ) )

#endif // TWOBLUECUBES_CATCH_CAPTURE_HPP_INCLUDED
