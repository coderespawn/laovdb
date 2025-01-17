// Copyright Contributors to the OpenVDB Project
// SPDX-License-Identifier: MPL-2.0

#include <openvdb_ax/compiler/Compiler.h>

#include <openvdb/openvdb.h>
#include <openvdb/points/PointDataGrid.h>
#include <openvdb/util/CpuTimer.h>
#include <openvdb/util/logging.h>

#include <cppunit/CompilerOutputter.h>
#include <cppunit/TestFailure.h>
#include <cppunit/TestListener.h>
#include <cppunit/TestResult.h>
#include <cppunit/TestResultCollector.h>
#include <cppunit/TextTestProgressListener.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>

#include <algorithm> // for std::shuffle()
#include <cmath> // for std::round()
#include <cstdlib> // for EXIT_SUCCESS
#include <cstring> // for strrchr()
#include <exception>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>


/// @note  Global unit test flag enabled with -g which symbolises the integration
///        tests to auto-generate their AX tests. Any previous tests will be
///        overwritten.
int sGenerateAX = false;


namespace {

using StringVec = std::vector<std::string>;


void
usage(const char* progName, std::ostream& ostrm)
{
    ostrm <<
"Usage: " << progName << " [options]\n" <<
"Which: runs OpenVDB AX library unit tests\n" <<
"Options:\n" <<
"    -f file   read whitespace-separated names of tests to be run\n" <<
"              from the given file (\"#\" comments are supported)\n" <<
"    -l        list all available tests\n" <<
"    -shuffle  run tests in random order\n" <<
"    -t test   specific suite or test to run, e.g., \"-t TestGrid\"\n" <<
"              or \"-t TestGrid::testGetGrid\" (default: run all tests)\n" <<
"    -v        verbose output\n" <<
"    -g        As well as testing, auto-generate any integration tests\n";
#ifdef OPENVDB_USE_LOG4CPLUS
    ostrm <<
"\n" <<
"    -error    log fatal and non-fatal errors (default: log only fatal errors)\n" <<
"    -warn     log warnings and errors\n" <<
"    -info     log info messages, warnings and errors\n" <<
"    -debug    log debugging messages, info messages, warnings and errors\n";
#endif
}


void
getTestNames(StringVec& nameVec, const CppUnit::Test* test)
{
    if (test) {
        const int numChildren = test->getChildTestCount();
        if (numChildren == 0) {
            nameVec.push_back(test->getName());
        } else {
            for (int i = 0; i < test->getChildTestCount(); ++i) {
                getTestNames(nameVec, test->getChildTestAt(i));
            }
        }
    }
}


/// Listener that prints the name, elapsed time, and error status of each test
class TimedTestProgressListener: public CppUnit::TestListener
{
public:
    void startTest(CppUnit::Test* test) override
    {
        mFailed = false;
        std::cout << test->getName() << std::flush;
        mTimer.start();
    }

    void addFailure(const CppUnit::TestFailure& failure) override
    {
        std::cout << " : " << (failure.isError() ? "error" : "assertion");
        mFailed  = true;
    }

    void endTest(CppUnit::Test*) override
    {
        if (!mFailed) {
            // Print elapsed time only for successful tests.
            const double msec = std::round(mTimer.milliseconds());
            if (msec > 1.0) {
                laovdb::util::printTime(std::cout, msec, " : OK (", ")",
                    /*width=*/0, /*precision=*/(msec > 1000.0 ? 1 : 0), /*verbose=*/0);
            } else {
                std::cout << " : OK (<1ms)";
            }
        }
        std::cout << std::endl;
    }

private:
    laovdb::util::CpuTimer mTimer;
    bool mFailed = false;
};


int
run(int argc, char* argv[])
{
    const char* progName = argv[0];
    if (const char* ptr = ::strrchr(progName, '/')) progName = ptr + 1;

    bool shuffle = false, verbose = false;
    StringVec tests;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-l") {
            StringVec allTests;
            const CppUnit::Test* tests = CppUnit::TestFactoryRegistry::getRegistry().makeTest();
            getTestNames(allTests, tests);
            delete tests;
            for (const auto& name: allTests) { std::cout << name << "\n"; }
            return EXIT_SUCCESS;
        } else if (arg == "-shuffle") {
            shuffle = true;
        } else if (arg == "-v") {
            verbose = true;
        } else if (arg == "-g") {
            sGenerateAX = true;
        } else if (arg == "-t") {
            if (i + 1 < argc) {
                ++i;
                tests.push_back(argv[i]);
            } else {
                OPENVDB_LOG_FATAL("missing test name after \"-t\"");
                usage(progName, std::cerr);
                return EXIT_FAILURE;
            }
        } else if (arg == "-f") {
            if (i + 1 < argc) {
                ++i;
                std::ifstream file{argv[i]};
                if (file.fail()) {
                    OPENVDB_LOG_FATAL("unable to read file " << argv[i]);
                    return EXIT_FAILURE;
                }
                while (file) {
                    // Read a whitespace-separated string from the file.
                    std::string test;
                    file >> test;
                    if (!test.empty()) {
                        if (test[0] != '#') {
                            tests.push_back(test);
                        } else {
                            // If the string starts with a comment symbol ("#"),
                            // skip it and jump to the end of the line.
                            while (file) { if (file.get() == '\n') break; }
                        }
                    }
                }
            } else {
                OPENVDB_LOG_FATAL("missing filename after \"-f\"");
                usage(progName, std::cerr);
                return EXIT_FAILURE;
            }
        } else if (arg == "-h" || arg == "-help" || arg == "--help") {
            usage(progName, std::cout);
            return EXIT_SUCCESS;
        } else {
            OPENVDB_LOG_FATAL("unrecognized option \"" << arg << "\"");
            usage(progName, std::cerr);
            return EXIT_FAILURE;
        }
    }

    try {
        CppUnit::TestFactoryRegistry& registry =
            CppUnit::TestFactoryRegistry::getRegistry();

        auto* root = registry.makeTest();
        if (!root) {
            throw std::runtime_error(
                "CppUnit test registry was not initialized properly");
        }

        if (!shuffle) {
            if (tests.empty()) tests.push_back("");
        } else {
            // Get the names of all selected tests and their children.
            StringVec allTests;
            if (tests.empty()) {
                getTestNames(allTests, root);
            } else {
                for (const auto& name: tests) {
                    getTestNames(allTests, root->findTest(name));
                }
            }
            // Randomly shuffle the list of names.
            std::random_device randDev;
            std::mt19937 generator(randDev());
            std::shuffle(allTests.begin(), allTests.end(), generator);
            tests.swap(allTests);
        }

        CppUnit::TestRunner runner;
        runner.addTest(root);

        CppUnit::TestResult controller;

        CppUnit::TestResultCollector result;
        controller.addListener(&result);

        CppUnit::TextTestProgressListener progress;
        TimedTestProgressListener vProgress;
        if (verbose) {
            controller.addListener(&vProgress);
        } else {
            controller.addListener(&progress);
        }

        for (size_t i = 0; i < tests.size(); ++i) {
            runner.run(controller, tests[i]);
        }

        CppUnit::CompilerOutputter outputter(&result, std::cerr);
        outputter.write();

        return result.wasSuccessful() ? EXIT_SUCCESS : EXIT_FAILURE;

    } catch (std::exception& e) {
        OPENVDB_LOG_FATAL(e.what());
        return EXIT_FAILURE;
    }
}

} // anonymous namespace

template <typename T>
static inline void registerType()
{
    if (!laovdb::points::TypedAttributeArray<T>::isRegistered())
        laovdb::points::TypedAttributeArray<T>::registerType();
}

int
main(int argc, char *argv[])
{
    laovdb::initialize();
    laovdb::ax::initialize();
    laovdb::logging::initialize(argc, argv);

    // Also intialize Vec2/4 point attributes

    registerType<laovdb::math::Vec2<int32_t>>();
    registerType<laovdb::math::Vec2<float>>();
    registerType<laovdb::math::Vec2<double>>();
    registerType<laovdb::math::Vec4<int32_t>>();
    registerType<laovdb::math::Vec4<float>>();
    registerType<laovdb::math::Vec4<double>>();

    auto value = run(argc, argv);

    laovdb::ax::uninitialize();
    laovdb::uninitialize();

    return value;
}

