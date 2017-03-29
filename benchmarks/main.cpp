#include <benchmark/benchmark.h>
#include <iostream>
#include <boost/optional/optional.hpp>

namespace
{
    struct TestReporter : benchmark::BenchmarkReporter
    {
        boost::optional<bool> ok;
        benchmark::ConsoleReporter display;

        bool ReportContext(const Context &context) override
        {
            return display.ReportContext(context);
        }

        void ReportRuns(const std::vector<Run> &report) override
        {
            for (const Run &run : report)
            {
                if (run.benchmark_name == "MediumRequestAndResponseOverInProcessPipe/4k")
                {
                    // These values should be about right for the super slow
                    // processors on travis-ci.
                    const double required_bytes =
#ifdef NDEBUG
                        1200000
#else
                        19500
#endif
                        ;
                    if (run.bytes_per_second >= required_bytes)
                    {
                        if (run.bytes_per_second < (required_bytes * 2))
                        {
                            ok = true;
                        }
                        else
                        {
                            std::cerr << run.benchmark_name
                                      << " is much faster than required which "
                                         "is suspicious: "
                                      << run.bytes_per_second << " bytes/s (" << required_bytes
                                      << " required)\n";
                            ok = false;
                        }
                    }
                    else
                    {
                        std::cerr << run.benchmark_name << " is too slow: " << run.bytes_per_second
                                  << " bytes/s (" << required_bytes << " required)\n";
                        ok = false;
                    }
                }
            }
            return display.ReportRuns(report);
        }
    };
}

int main(int argc, char **argv)
{
    benchmark::Initialize(&argc, argv);
    TestReporter reporter;
    benchmark::RunSpecifiedBenchmarks(&reporter);
    if (!reporter.ok)
    {
        std::cerr << "At least one of the required test benchmarks did not run\n";
        return 1;
    }
    return (*reporter.ok ? 0 : 1);
}
