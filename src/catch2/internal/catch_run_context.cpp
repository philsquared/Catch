
//              Copyright Catch2 Authors
// Distributed under the Boost Software License, Version 1.0.
//   (See accompanying file LICENSE.txt or copy at
//        https://www.boost.org/LICENSE_1_0.txt)

// SPDX-License-Identifier: BSL-1.0
#include <catch2/internal/catch_run_context.hpp>

#include <catch2/catch_user_config.hpp>
#include <catch2/generators/catch_generators_throw.hpp>
#include <catch2/interfaces/catch_interfaces_config.hpp>
#include <catch2/interfaces/catch_interfaces_generatortracker.hpp>
#include <catch2/interfaces/catch_interfaces_registry_hub.hpp>
#include <catch2/interfaces/catch_interfaces_reporter.hpp>
#include <catch2/internal/catch_compiler_capabilities.hpp>
#include <catch2/internal/catch_context.hpp>
#include <catch2/internal/catch_enforce.hpp>
#include <catch2/internal/catch_fatal_condition_handler.hpp>
#include <catch2/internal/catch_random_number_generator.hpp>
#include <catch2/catch_timer.hpp>
#include <catch2/internal/catch_output_redirect.hpp>
#include <catch2/internal/catch_assertion_handler.hpp>
#include <catch2/internal/catch_path_filter.hpp>
#include <catch2/internal/catch_test_failure_exception.hpp>
#include <catch2/internal/catch_thread_local.hpp>
#include <catch2/internal/catch_unreachable.hpp>
#include <catch2/internal/catch_result_type.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cassert>
#include <algorithm>

namespace Catch {

    namespace Generators {
        namespace {
            struct GeneratorTracker final : TestCaseTracking::TrackerBase,
                                      IGeneratorTracker {
                GeneratorBasePtr m_generator;
                // Filtered generator has moved to specific index due to
                // a filter, it needs special handling of `countedNext()`
                bool m_isFiltered = false;

                GeneratorTracker(
                    TestCaseTracking::NameAndLocation&& nameAndLocation,
                    TrackerContext& ctx,
                    ITracker* parent,
                    GeneratorBasePtr&& generator ):
                    TrackerBase( CATCH_MOVE( nameAndLocation ), ctx, parent ),
                    m_generator( CATCH_MOVE( generator ) ) {
                    assert( m_generator &&
                            "Cannot create tracker without generator" );

                    // Handle potential filter and move forward here...
                    // Old style filters do not affect generators at all
                    if (m_newStyleFilters && m_allTrackerDepth < m_filterRef->size()) {
                        auto const& filter =
                            ( *m_filterRef )[m_allTrackerDepth];
                        // Generator cannot be un-entered the way a section
                        // can be, so the tracker has to throw for a wrong
                        // filter to stop the execution flow.
                        if (filter.type == PathFilter::For::Section) {
                            // We want the semantics of `SKIP()`, but we inline it
                            // to avoid issues with conditionally prefixed macros
                            INTERNAL_CATCH_MSG(
                                "SKIP",
                                Catch::ResultWas::ExplicitSkip,
                                Catch::ResultDisposition::Normal,
                                "" );
                            Catch::Detail::Unreachable();
                        }
                        // '*' is the wildcard for "all elements in generator"
                        // used for filtering sections below the generator, but
                        // not the generator itself.
                        if ( filter.filter != "*" ) {
                            m_isFiltered = true;
                            // TBD: We assume that the filter was validated as
                            //      number during parsing. We should pass it
                            //      as number from the CLI parser.
                            size_t targetIndex = std::stoul( filter.filter );
                            m_generator->skipToNthElement( targetIndex );
                        }
                    }
                }

                static GeneratorTracker*
                acquire( TrackerContext& ctx,
                         TestCaseTracking::NameAndLocationRef const&
                             nameAndLocation ) {
                    GeneratorTracker* tracker;

                    ITracker& currentTracker = ctx.currentTracker();
                    // Under specific circumstances, the generator we want
                    // to acquire is also the current tracker. If this is
                    // the case, we have to avoid looking through current
                    // tracker's children, and instead return the current
                    // tracker.
                    // A case where this check is important is e.g.
                    //     for (int i = 0; i < 5; ++i) {
                    //         int n = GENERATE(1, 2);
                    //     }
                    //
                    // without it, the code above creates 5 nested generators.
                    if ( currentTracker.nameAndLocation() == nameAndLocation ) {
                        auto thisTracker = currentTracker.parent()->findChild(
                            nameAndLocation );
                        assert( thisTracker );
                        assert( thisTracker->isGeneratorTracker() );
                        tracker = static_cast<GeneratorTracker*>( thisTracker );
                    } else if ( ITracker* childTracker =
                                    currentTracker.findChild(
                                        nameAndLocation ) ) {
                        assert( childTracker );
                        assert( childTracker->isGeneratorTracker() );
                        tracker =
                            static_cast<GeneratorTracker*>( childTracker );
                    } else {
                        return nullptr;
                    }

                    if ( !tracker->isComplete() ) { tracker->open(); }

                    return tracker;
                }

                // TrackerBase interface
                bool isGeneratorTracker() const override { return true; }
                void close() override {
                    TrackerBase::close();
                    // If a generator has a child (it is followed by a section)
                    // and none of its children have started, then we must wait
                    // until later to start consuming its values.
                    // This catches cases where `GENERATE` is placed between two
                    // `SECTION`s.
                    // **The check for m_children.empty cannot be removed**.
                    // doing so would break `GENERATE` _not_ followed by
                    // `SECTION`s.
                    const bool should_wait_for_child = [&]() {
                        // No children -> nobody to wait for
                        if ( m_children.empty() ) { return false; }
                        // If at least one child started executing, don't wait
                        if ( std::find_if(
                                 m_children.begin(),
                                 m_children.end(),
                                 []( TestCaseTracking::ITrackerPtr const&
                                         tracker ) {
                                     return tracker->hasStarted();
                                 } ) != m_children.end() ) {
                            return false;
                        }

                        // No children have started. We need to check if they
                        // _can_ start, and thus we should wait for them, or
                        // they cannot start (due to filters), and we shouldn't
                        // wait for them

                        // No filters left -> no restrictions on running sections
                        size_t childDepth = 1 + (m_newStyleFilters ? m_allTrackerDepth : m_sectionOnlyDepth);
                        if ( childDepth >= m_filterRef->size() ) {
                            return true;
                        }

                        // If we are using the new style filters, we need to check
                        // whether the successive filter is for section or a generator.
                        if ( m_newStyleFilters
                            && (*m_filterRef)[childDepth].type != PathFilter::For::Section ) {
                            return false;
                        }
                        // Look for any child section that could match the remaining filters
                        for ( auto const& child : m_children ) {
                            if ( child->isSectionTracker() &&
                                 static_cast<SectionTracker const&>( *child )
                                         .trimmedName() == StringRef((*m_filterRef)[childDepth].filter) ) {
                                return true;
                            }
                        }
                        return false;
                    }();

                    // This check is a bit tricky, because m_generator->next()
                    // has a side-effect, where it consumes generator's current
                    // value, but we do not want to invoke the side-effect if
                    // this generator is still waiting for any child to start.
                    assert( m_generator && "Tracker without generator" );
                    if ( should_wait_for_child
                        ||  ( m_runState == CompletedSuccessfully
                            && !m_isFiltered // filtered generators cannot meaningfully move forward, as they would get past the filter
                            && m_generator->countedNext() ) ) {
                        m_children.clear();
                        m_runState = Executing;
                    }
                }

                // IGeneratorTracker interface
                auto getGenerator() const -> GeneratorBasePtr const& override {
                    return m_generator;
                }

                bool isFilteredImpl() const override { return m_isFiltered; }
            };
        } // namespace
    }

    namespace Detail {
        // Assertions are owned by the thread that is executing them.
        // This allows for lock-free progress in common cases where we
        // do not need to send the assertion events to the reporter.
        // This also implies that messages are owned by their respective
        // threads, and should not be shared across different threads.
        //
        // This implies that various pieces of metadata referring to last
        // assertion result/source location/message handling, etc
        // should also be thread local. For now we just use naked globals
        // below, in the future we will want to allocate piece of memory
        // from heap, to avoid consuming too much thread-local storage.
        //
        // Note that we also don't want non-trivial the thread-local variables
        // below be initialized for every thread, only for those that touch
        // Catch2. To make this work with both GCC/Clang and MSVC, we have to
        // make them thread-local magic statics. (Class-level statics have the
        // desired semantics on GCC, but not on MSVC).

        // This is used for the "if" part of CHECKED_IF/CHECKED_ELSE
        static CATCH_INTERNAL_THREAD_LOCAL bool g_lastAssertionPassed = false;

        // This is the source location for last encountered macro. It is
        // used to provide the users with more precise location of error
        // when an unexpected exception/fatal error happens.
        static CATCH_INTERNAL_THREAD_LOCAL SourceLineInfo
            g_lastKnownLineInfo( "DummyLocation", static_cast<size_t>( -1 ) );

        // Should we clear message scopes before sending off the messages to
        // reporter? Set in `assertionPassedFastPath` to avoid doing the full
        // clear there for performance reasons.
        static CATCH_INTERNAL_THREAD_LOCAL bool g_clearMessageScopes = false;


        // Holds the data for both scoped and unscoped messages together,
        // to avoid issues where their lifetimes start in wrong order,
        // and then are destroyed in wrong order.
        class MessageHolder {
            // The actual message vector passed to the reporters
            std::vector<MessageInfo> messages;
            // IDs of messages from UNSCOPED_X macros, which we have to
            // remove manually.
            std::vector<unsigned int> unscoped_ids;

        public:
            // We do not need to special-case the unscoped messages when
            // we only keep around the raw msg ids.
            ~MessageHolder() = default;

            void addUnscopedMessage( MessageInfo&& info ) {
                repairUnscopedMessageInvariant();
                unscoped_ids.push_back( info.sequence );
                messages.push_back( CATCH_MOVE( info ) );
            }

            void addUnscopedMessage(MessageBuilder&& builder) {
                MessageInfo info( CATCH_MOVE( builder.m_info ) );
                info.message = builder.m_stream.str();
                addUnscopedMessage( CATCH_MOVE( info ) );
            }

            void addScopedMessage(MessageInfo&& info) {
                messages.push_back( CATCH_MOVE( info ) );
            }

            std::vector<MessageInfo> const& getMessages() const {
                return messages;
            }

            void removeMessage( unsigned int messageId ) {
                // Note: On average, it would probably be better to look for
                //       the message backwards. However, we do not expect to have
                //       to  deal with more messages than low single digits, so
                //       the improvement is tiny, and we would have to hand-write
                //       the loop to avoid terrible codegen of reverse iterators
                //       in debug mode.
                auto iter =
                    std::find_if( messages.begin(),
                                  messages.end(),
                                  [messageId]( MessageInfo const& msg ) {
                                      return msg.sequence == messageId;
                                  } );
                assert( iter != messages.end() &&
                        "Trying to remove non-existent message." );
                messages.erase( iter );
            }

            void removeUnscopedMessages() {
                for ( const auto messageId : unscoped_ids ) {
                    removeMessage( messageId );
                }
                unscoped_ids.clear();
                g_clearMessageScopes = false;
            }

            void repairUnscopedMessageInvariant() {
                if ( g_clearMessageScopes ) { removeUnscopedMessages(); }
                g_clearMessageScopes = false;
            }
        };

        CATCH_INTERNAL_START_WARNINGS_SUPPRESSION
        CATCH_INTERNAL_SUPPRESS_GLOBALS_WARNINGS
        static MessageHolder& g_messageHolder() {
            static CATCH_INTERNAL_THREAD_LOCAL MessageHolder value;
            return value;
        }
        CATCH_INTERNAL_STOP_WARNINGS_SUPPRESSION


        void pushScopedMessage( MessageInfo&& message ) {
            Detail::g_messageHolder().addScopedMessage(  CATCH_MOVE( message ) );
        }

        void popScopedMessage( unsigned int messageId ) {
            Detail::g_messageHolder().removeMessage( messageId );
        }

        void emplaceUnscopedMessage( MessageBuilder&& builder ) {
            Detail::g_messageHolder().addUnscopedMessage( CATCH_MOVE( builder ) );
        }

        void addUnscopedMessage( MessageInfo&& message ) {
            Detail::g_messageHolder().addUnscopedMessage( CATCH_MOVE( message ) );
        }

        bool lastAssertionPassed() { return Detail::g_lastAssertionPassed; }

    } // namespace Detail

    RunContext::RunContext( IConfig const* _config,
                            IEventListenerPtr&& reporter,
                            std::size_t testCaseCount )
    :   m_runInfo( _config->name(), testCaseCount ),
        m_config(_config),
        m_reporter(CATCH_MOVE(reporter)),
        m_outputRedirect( makeOutputRedirect( m_reporter->getPreferences().shouldRedirectStdOut ) ),
        m_abortAfterXFailedAssertions( m_config->abortAfter() ),
        m_reportAssertionStarting( m_reporter->getPreferences().shouldReportAllAssertionStarts ),
        m_includeSuccessfulResults( m_config->includeSuccessfulResults() || m_reporter->getPreferences().shouldReportAllAssertions ),
        m_shouldDebugBreak( m_config->shouldDebugBreak() )
    {
        getCurrentMutableContext().setResultCapture( this );
        m_reporter->testRunStarting(m_runInfo);

        // TODO: HACK!
        //       We need to make sure the underlying cache is initialized
        //       while we are guaranteed to be running in a single thread,
        //       because the initialization is not thread-safe.
        ReusableStringStream rss;
        (void)rss;
    }

    RunContext::~RunContext() {
        updateTotalsFromAtomics();
        m_reporter->testRunEnded(TestRunStats(m_runInfo, m_totals, aborting()));
    }

    Totals RunContext::runTest(TestCaseHandle const& testCase) {
        updateTotalsFromAtomics();
        const Totals prevTotals = m_totals;

        auto const& testInfo = testCase.getTestCaseInfo();
        m_reporter->testCaseStarting(testInfo);
        testCase.prepareTestCase();
        m_activeTestCase = &testCase;


        ITracker& rootTracker = m_trackerContext.startRun();
        assert(rootTracker.isSectionTracker());
        rootTracker.setFilters( &m_config->getPathFilters(),
                                m_config->useNewFilterBehaviour() );

        // We intentionally only seed the internal RNG once per test case,
        // before it is first invoked. The reason for that is a complex
        // interplay of generator/section implementation details and the
        // Random*Generator types.
        //
        // The issue boils down to us needing to seed the Random*Generators
        // with different seed each, so that they return different sequences
        // of random numbers. We do this by giving them a number from the
        // shared RNG instance as their seed.
        //
        // However, this runs into an issue if the reseeding happens each
        // time the test case is entered (as opposed to first time only),
        // because multiple generators could get the same seed, e.g. in
        // ```cpp
        // TEST_CASE() {
        //     auto i = GENERATE(take(10, random(0, 100));
        //     SECTION("A") {
        //         auto j = GENERATE(take(10, random(0, 100));
        //     }
        //     SECTION("B") {
        //         auto k = GENERATE(take(10, random(0, 100));
        //     }
        // }
        // ```
        // `i` and `j` would properly return values from different sequences,
        // but `i` and `k` would return the same sequence, because their seed
        // would be the same.
        // (The reason their seeds would be the same is that the generator
        //  for k would be initialized when the test case is entered the second
        //  time, after the shared RNG instance was reset to the same value
        //  it had when the generator for i was initialized.)
        seedRng( *m_config );

        uint64_t testRuns = 0;
        std::string redirectedCout;
        std::string redirectedCerr;
        do {
            m_trackerContext.startCycle();
            m_testCaseTracker = &SectionTracker::acquire(m_trackerContext, TestCaseTracking::NameAndLocationRef(testInfo.name, testInfo.lineInfo));

            m_reporter->testCasePartialStarting(testInfo, testRuns);

            updateTotalsFromAtomics();
            const auto beforeRunTotals = m_totals;
            runCurrentTest();
            std::string oneRunCout = m_outputRedirect->getStdout();
            std::string oneRunCerr = m_outputRedirect->getStderr();
            m_outputRedirect->clearBuffers();
            redirectedCout += oneRunCout;
            redirectedCerr += oneRunCerr;

            updateTotalsFromAtomics();
            const auto singleRunTotals = m_totals.delta(beforeRunTotals);
            auto statsForOneRun = TestCaseStats(testInfo, singleRunTotals, CATCH_MOVE(oneRunCout), CATCH_MOVE(oneRunCerr), aborting());
            m_reporter->testCasePartialEnded(statsForOneRun, testRuns);

            ++testRuns;
        } while (!m_testCaseTracker->isSuccessfullyCompleted() && !aborting());

        Totals deltaTotals = m_totals.delta(prevTotals);
        if (testInfo.expectedToFail() && deltaTotals.testCases.passed > 0) {
            deltaTotals.assertions.failed++;
            deltaTotals.testCases.passed--;
            deltaTotals.testCases.failed++;
        }
        m_totals.testCases += deltaTotals.testCases;
        testCase.tearDownTestCase();
        m_reporter->testCaseEnded(TestCaseStats(testInfo,
                                  deltaTotals,
                                  CATCH_MOVE(redirectedCout),
                                  CATCH_MOVE(redirectedCerr),
                                  aborting()));

        m_activeTestCase = nullptr;
        m_testCaseTracker = nullptr;

        return deltaTotals;
    }


    void RunContext::assertionEnded(AssertionResult&& result) {
        Detail::g_lastKnownLineInfo = result.m_info.lineInfo;
        if (result.getResultType() == ResultWas::Ok) {
            m_atomicAssertionCount.passed++;
            Detail::g_lastAssertionPassed = true;
        } else if (result.getResultType() == ResultWas::ExplicitSkip) {
            m_atomicAssertionCount.skipped++;
            Detail::g_lastAssertionPassed = true;
        } else if (!result.succeeded()) {
            Detail::g_lastAssertionPassed = false;
            if (result.isOk()) {}
            else if( m_activeTestCase->getTestCaseInfo().okToFail() ) { // Read from a shared state established before the threads could start, this is fine
                m_atomicAssertionCount.failedButOk++;
            } else {
                m_atomicAssertionCount.failed++;
            }
        } else {
            Detail::g_lastAssertionPassed = true;
        }

        auto& msgHolder = Detail::g_messageHolder();
        msgHolder.repairUnscopedMessageInvariant();

        // From here, we are touching shared state and need mutex.
        Detail::LockGuard lock( m_assertionMutex );
        {
            auto _ = scopedDeactivate( *m_outputRedirect );
            updateTotalsFromAtomics();
            m_reporter->assertionEnded( AssertionStats( result, msgHolder.getMessages(), m_totals ) );
        }

        if ( result.getResultType() != ResultWas::Warning ) {
            msgHolder.removeUnscopedMessages();
        }

        // Reset working state. assertion info will be reset after
        // populateReaction is run if it is needed
        m_lastResult = CATCH_MOVE( result );
    }

    void RunContext::notifyAssertionStarted( AssertionInfo const& info ) {
        if (m_reportAssertionStarting) {
            Detail::LockGuard lock( m_assertionMutex );
            auto _ = scopedDeactivate( *m_outputRedirect );
            m_reporter->assertionStarting( info );
        }
    }

    bool RunContext::sectionStarted( StringRef sectionName,
                                     SourceLineInfo const& sectionLineInfo,
                                     Counts& assertions ) {
        ITracker& sectionTracker =
            SectionTracker::acquire( m_trackerContext,
                                     TestCaseTracking::NameAndLocationRef(
                                         sectionName, sectionLineInfo ) );

        if (!sectionTracker.isOpen())
            return false;
        m_activeSections.push_back(&sectionTracker);

        SectionInfo sectionInfo( sectionLineInfo, static_cast<std::string>(sectionName) );
        Detail::g_lastKnownLineInfo = sectionLineInfo;

        {
            auto _ = scopedDeactivate( *m_outputRedirect );
            m_reporter->sectionStarting( sectionInfo );
        }

        updateTotalsFromAtomics();
        assertions = m_totals.assertions;

        return true;
    }
    IGeneratorTracker*
    RunContext::acquireGeneratorTracker( StringRef generatorName,
                                         SourceLineInfo const& lineInfo ) {
        auto* tracker = Generators::GeneratorTracker::acquire(
            m_trackerContext,
            TestCaseTracking::NameAndLocationRef(
                 generatorName, lineInfo ) );
        Detail::g_lastKnownLineInfo = lineInfo;
        return tracker;
    }

    IGeneratorTracker* RunContext::createGeneratorTracker(
        StringRef generatorName,
        SourceLineInfo lineInfo,
        Generators::GeneratorBasePtr&& generator ) {

        auto nameAndLoc = TestCaseTracking::NameAndLocation( static_cast<std::string>( generatorName ), lineInfo );
        auto& currentTracker = m_trackerContext.currentTracker();
        assert(
            currentTracker.nameAndLocation() != nameAndLoc &&
            "Trying to create tracker for a generator that already has one" );

        auto newTracker =
            Catch::Detail::make_unique<Generators::GeneratorTracker>(
                CATCH_MOVE( nameAndLoc ),
                m_trackerContext,
                &currentTracker,
                CATCH_MOVE( generator ) );

        // The warning shouldn't fire if the generator is infinite, **but** filtered down.
        if ( m_config->warnAboutInfiniteGenerators() &&
             !newTracker->m_generator->isFinite() &&
             !newTracker->isFiltered() ) {
            // We want the semantics of `FAIL()`, but we inline it
            // to avoid issues with conditionally prefixed macros
            INTERNAL_CATCH_MSG( "FAIL",
                                Catch::ResultWas::ExplicitFailure,
                                Catch::ResultDisposition::Normal,
                                "GENERATE() would run infinitely" );
        }

        auto returnPtr = newTracker.get();
        currentTracker.addChild( CATCH_MOVE( newTracker ) );

        returnPtr->open();
        return returnPtr;
    }

    bool RunContext::testForMissingAssertions(Counts& assertions) {
        if (assertions.total() != 0)
            return false;
        if (!m_config->warnAboutMissingAssertions())
            return false;
        if (m_trackerContext.currentTracker().hasChildren())
            return false;
        m_atomicAssertionCount.failed++;
        assertions.failed++;
        return true;
    }

    void RunContext::sectionEnded(SectionEndInfo&& endInfo) {
        updateTotalsFromAtomics();
        Counts assertions = m_totals.assertions - endInfo.prevAssertions;
        bool missingAssertions = testForMissingAssertions(assertions);

        if (!m_activeSections.empty()) {
            m_activeSections.back()->close();
            m_activeSections.pop_back();
        }

        {
            auto _ = scopedDeactivate( *m_outputRedirect );
            m_reporter->sectionEnded(
                SectionStats( CATCH_MOVE( endInfo.sectionInfo ),
                              assertions,
                              endInfo.durationInSeconds,
                              missingAssertions ) );
        }
    }

    void RunContext::sectionEndedEarly(SectionEndInfo&& endInfo) {
        if ( m_unfinishedSections.empty() ) {
            m_activeSections.back()->fail();
        } else {
            m_activeSections.back()->close();
        }
        m_activeSections.pop_back();

        m_unfinishedSections.push_back(CATCH_MOVE(endInfo));
    }

    void RunContext::benchmarkPreparing( StringRef name ) {
        auto _ = scopedDeactivate( *m_outputRedirect );
        m_reporter->benchmarkPreparing( name );
    }
    void RunContext::benchmarkStarting( BenchmarkInfo const& info ) {
        auto _ = scopedDeactivate( *m_outputRedirect );
        m_reporter->benchmarkStarting( info );
    }
    void RunContext::benchmarkEnded( BenchmarkStats<> const& stats ) {
        auto _ = scopedDeactivate( *m_outputRedirect );
        m_reporter->benchmarkEnded( stats );
    }
    void RunContext::benchmarkFailed( StringRef error ) {
        auto _ = scopedDeactivate( *m_outputRedirect );
        m_reporter->benchmarkFailed( error );
    }

    std::string RunContext::getCurrentTestName() const {
        return m_activeTestCase
            ? m_activeTestCase->getTestCaseInfo().name
            : std::string();
    }

    const AssertionResult * RunContext::getLastResult() const {
        // m_lastResult is updated inside the assertion slow-path, under
        // a mutex, so the read needs to happen under mutex as well.

        // TBD: The last result only makes sense if it is a thread-local
        //      thing, because the answer is different per thread, like
        //      last line info, whether last assertion passed, and so on.
        //
        //      However, the last result was also never updated in the
        //      assertion fast path, so it was always somewhat broken,
        //      and since IResultCapture::getLastResult is deprecated,
        //      we will leave it as is, until it is finally removed.
        Detail::LockGuard _( m_assertionMutex );
        return &*m_lastResult;
    }

    void RunContext::exceptionEarlyReported() {
        m_shouldReportUnexpected = false;
    }

    void RunContext::handleFatalErrorCondition( StringRef message ) {
        // We lock only when touching the reporters directly, to avoid
        // deadlocks when we call into other functions that also want
        // to lock the mutex before touching reporters.
        //
        // This does mean that we allow other threads to run while handling
        // a fatal error, but this is all a best effort attempt anyway.
        {
            Detail::LockGuard lock( m_assertionMutex );
            // TODO: scoped deactivate here? Just give up and do best effort?
            //       the deactivation can break things further, OTOH so can the
            //       capture
            auto _ = scopedDeactivate( *m_outputRedirect );

            // First notify reporter that bad things happened
            m_reporter->fatalErrorEncountered( message );
        }

        // Don't rebuild the result -- the stringification itself can cause more fatal errors
        // Instead, fake a result data.
        AssertionResultData tempResult( ResultWas::FatalErrorCondition, { false } );
        tempResult.message = static_cast<std::string>(message);
        AssertionResult result( makeDummyAssertionInfo(),
                                CATCH_MOVE( tempResult ) );

        assertionEnded(CATCH_MOVE(result) );


        // At this point we touch sections/test cases from this thread
        // to try and end them. Technically that is not supported when
        // using multiple threads, but the worst thing that can happen
        // is that the process aborts harder :-D
        Detail::LockGuard lock( m_assertionMutex );

        // Best effort cleanup for sections that have not been destructed yet
        // Since this is a fatal error, we have not had and won't have the opportunity to destruct them properly
        while (!m_activeSections.empty()) {
            auto const& nl = m_activeSections.back()->nameAndLocation();
            SectionEndInfo endInfo{ SectionInfo(nl.location, nl.name), {}, 0.0 };
            sectionEndedEarly(CATCH_MOVE(endInfo));
        }
        handleUnfinishedSections();

        // Recreate section for test case (as we will lose the one that was in scope)
        auto const& testCaseInfo = m_activeTestCase->getTestCaseInfo();
        SectionInfo testCaseSection(testCaseInfo.lineInfo, testCaseInfo.name);

        Counts assertions;
        assertions.failed = 1;
        SectionStats testCaseSectionStats(CATCH_MOVE(testCaseSection), assertions, 0, false);
        m_reporter->sectionEnded( testCaseSectionStats );

        auto const& testInfo = m_activeTestCase->getTestCaseInfo();

        Totals deltaTotals;
        deltaTotals.testCases.failed = 1;
        deltaTotals.assertions.failed = 1;
        m_reporter->testCaseEnded(TestCaseStats(testInfo,
                                  deltaTotals,
                                  std::string(),
                                  std::string(),
                                  false));
        m_totals.testCases.failed++;
        updateTotalsFromAtomics();
        m_reporter->testRunEnded(TestRunStats(m_runInfo, m_totals, false));
    }

    void RunContext::assertionPassedFastPath(SourceLineInfo lineInfo) {
        // We want to save the line info for better experience with unexpected assertions
        Detail::g_lastKnownLineInfo = lineInfo;
        ++m_atomicAssertionCount.passed;
        Detail::g_lastAssertionPassed = true;
        Detail::g_clearMessageScopes = true;
    }

    void RunContext::updateTotalsFromAtomics() {
        m_totals.assertions = Counts{
            m_atomicAssertionCount.passed,
            m_atomicAssertionCount.failed,
            m_atomicAssertionCount.failedButOk,
            m_atomicAssertionCount.skipped,
        };
    }

    bool RunContext::aborting() const {
        return m_atomicAssertionCount.failed >= m_abortAfterXFailedAssertions;
    }

    void RunContext::runCurrentTest() {
        auto const& testCaseInfo = m_activeTestCase->getTestCaseInfo();
        SectionInfo testCaseSection(testCaseInfo.lineInfo, testCaseInfo.name);
        m_reporter->sectionStarting(testCaseSection);
        updateTotalsFromAtomics();
        Counts prevAssertions = m_totals.assertions;
        double duration = 0;
        m_shouldReportUnexpected = true;
        Detail::g_lastKnownLineInfo = testCaseInfo.lineInfo;

        Timer timer;
        CATCH_TRY {
            {
                auto _ = scopedActivate( *m_outputRedirect );
                timer.start();
                invokeActiveTestCase();
            }
            duration = timer.getElapsedSeconds();
        } CATCH_CATCH_ANON (TestFailureException&) {
            // This just means the test was aborted due to failure
        } CATCH_CATCH_ANON (TestSkipException&) {
            // This just means the test was explicitly skipped
        } CATCH_CATCH_ALL {
            // Under CATCH_CONFIG_FAST_COMPILE, unexpected exceptions under REQUIRE assertions
            // are reported without translation at the point of origin.
            if ( m_shouldReportUnexpected ) {
                AssertionReaction dummyReaction;
                handleUnexpectedInflightException( makeDummyAssertionInfo(),
                                                   translateActiveException(),
                                                   dummyReaction );
            }
        }
        updateTotalsFromAtomics();
        Counts assertions = m_totals.assertions - prevAssertions;
        bool missingAssertions = testForMissingAssertions(assertions);

        m_testCaseTracker->close();
        handleUnfinishedSections();
        auto& msgHolder = Detail::g_messageHolder();
        msgHolder.removeUnscopedMessages();
        assert( msgHolder.getMessages().empty() &&
                "There should be no leftover messages after the test ends" );

        SectionStats testCaseSectionStats(CATCH_MOVE(testCaseSection), assertions, duration, missingAssertions);
        m_reporter->sectionEnded(testCaseSectionStats);
    }

    void RunContext::invokeActiveTestCase() {
        // We need to engage a handler for signals/structured exceptions
        // before running the tests themselves, or the binary can crash
        // without failed test being reported.
        FatalConditionHandlerGuard _(&m_fatalConditionhandler);
        // We keep having issue where some compilers warn about an unused
        // variable, even though the type has non-trivial constructor and
        // destructor. This is annoying and ugly, but it makes them stfu.
        (void)_;

        m_activeTestCase->invoke();
    }

    void RunContext::handleUnfinishedSections() {
        // If sections ended prematurely due to an exception we stored their
        // infos here so we can tear them down outside the unwind process.
        for ( auto it = m_unfinishedSections.rbegin(),
                   itEnd = m_unfinishedSections.rend();
              it != itEnd;
              ++it ) {
            sectionEnded( CATCH_MOVE( *it ) );
        }
        m_unfinishedSections.clear();
    }

    void RunContext::handleExpr(
        AssertionInfo const& info,
        ITransientExpression const& expr,
        AssertionReaction& reaction
    ) {
        bool negated = isFalseTest( info.resultDisposition );
        bool result = expr.getResult() != negated;

        if( result ) {
            if (!m_includeSuccessfulResults) {
                assertionPassedFastPath(info.lineInfo);
            }
            else {
                reportExpr(info, ResultWas::Ok, &expr, negated);
            }
        }
        else {
            reportExpr(info, ResultWas::ExpressionFailed, &expr, negated );
            populateReaction(
                reaction, info.resultDisposition & ResultDisposition::Normal );
        }
    }
    void RunContext::reportExpr(
            AssertionInfo const &info,
            ResultWas::OfType resultType,
            ITransientExpression const *expr,
            bool negated ) {

        Detail::g_lastKnownLineInfo = info.lineInfo;
        AssertionResultData data( resultType, LazyExpression( negated ) );

        AssertionResult assertionResult{ info, CATCH_MOVE( data ) };
        assertionResult.m_resultData.lazyExpression.m_transientExpression = expr;

        assertionEnded( CATCH_MOVE(assertionResult) );
    }

    void RunContext::handleMessage(
            AssertionInfo const& info,
            ResultWas::OfType resultType,
            std::string&& message,
            AssertionReaction& reaction
    ) {
        Detail::g_lastKnownLineInfo = info.lineInfo;

        AssertionResultData data( resultType, LazyExpression( false ) );
        data.message = CATCH_MOVE( message );
        AssertionResult assertionResult{ info,
                                         CATCH_MOVE( data ) };

        const auto isOk = assertionResult.isOk();
        assertionEnded( CATCH_MOVE(assertionResult) );
        if ( !isOk ) {
            populateReaction(
                reaction, info.resultDisposition & ResultDisposition::Normal );
        } else if ( resultType == ResultWas::ExplicitSkip ) {
            // TODO: Need to handle this explicitly, as ExplicitSkip is
            // considered "OK"
            reaction.shouldSkip = true;
        }
    }

    void RunContext::handleUnexpectedExceptionNotThrown(
            AssertionInfo const& info,
            AssertionReaction& reaction
    ) {
        handleNonExpr(info, Catch::ResultWas::DidntThrowException, reaction);
    }

    void RunContext::handleUnexpectedInflightException(
            AssertionInfo const& info,
            std::string&& message,
            AssertionReaction& reaction
    ) {
        Detail::g_lastKnownLineInfo = info.lineInfo;

        AssertionResultData data( ResultWas::ThrewException, LazyExpression( false ) );
        data.message = CATCH_MOVE(message);
        AssertionResult assertionResult{ info, CATCH_MOVE(data) };
        assertionEnded( CATCH_MOVE(assertionResult) );
        populateReaction( reaction,
                          info.resultDisposition & ResultDisposition::Normal );
    }

    void RunContext::populateReaction( AssertionReaction& reaction,
                                       bool has_normal_disposition ) const {
        reaction.shouldDebugBreak = m_shouldDebugBreak;
        reaction.shouldThrow = aborting() || has_normal_disposition;
    }

    AssertionInfo RunContext::makeDummyAssertionInfo() {
        const bool testCaseJustStarted =
            Detail::g_lastKnownLineInfo ==
            m_activeTestCase->getTestCaseInfo().lineInfo;

        return AssertionInfo{
            testCaseJustStarted ? "TEST_CASE"_sr : StringRef(),
            Detail::g_lastKnownLineInfo,
            testCaseJustStarted ? StringRef() : "{Unknown expression after the reported line}"_sr,
            ResultDisposition::Normal
        };
    }

    void RunContext::handleIncomplete(
            AssertionInfo const& info
    ) {
        using namespace std::string_literals;
        Detail::g_lastKnownLineInfo = info.lineInfo;

        AssertionResultData data( ResultWas::ThrewException, LazyExpression( false ) );
        data.message = "Exception translation was disabled by CATCH_CONFIG_FAST_COMPILE"s;
        AssertionResult assertionResult{ info, CATCH_MOVE( data ) };
        assertionEnded( CATCH_MOVE(assertionResult) );
    }

    void RunContext::handleNonExpr(
            AssertionInfo const &info,
            ResultWas::OfType resultType,
            AssertionReaction &reaction
    ) {
        AssertionResultData data( resultType, LazyExpression( false ) );
        AssertionResult assertionResult{ info, CATCH_MOVE( data ) };

        const auto isOk = assertionResult.isOk();
        if ( isOk && !m_includeSuccessfulResults ) {
            assertionPassedFastPath( info.lineInfo );
            return;
        }

        assertionEnded( CATCH_MOVE(assertionResult) );
        if ( !isOk ) {
            populateReaction(
                reaction, info.resultDisposition & ResultDisposition::Normal );
        }
    }

    void seedRng(IConfig const& config) {
        sharedRng().seed(config.rngSeed());
    }

    unsigned int rngSeed() {
        return getCurrentContext().getConfig()->rngSeed();
    }

}
