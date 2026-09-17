/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 */

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <random>
#include <string>
#include <sys/resource.h>
#include <unistd.h>
#include <vector>

namespace {
constexpr size_t PAGE_SIZE = 4096;
constexpr size_t FILE_SIZE = 512ULL * 1024ULL * 1024ULL;
constexpr size_t READ_SIZE = 128ULL * 1024ULL * 1024ULL;

struct Result {
    double mibPerSecond;
    long majorFaults;
    long minorFaults;
};

void Check(bool ok, const char *operation)
{
    if (!ok) {
        std::perror(operation);
        std::exit(EXIT_FAILURE);
    }
}

void PrepareFile(int fd)
{
    std::array<uint8_t, 1024 * 1024> buffer{};
    for (size_t i = 0; i < FILE_SIZE; i += buffer.size()) {
        Check(::pwrite(fd, buffer.data(), buffer.size(), static_cast<off_t>(i)) ==
                  static_cast<ssize_t>(buffer.size()),
              "pwrite");
    }
    Check(::fsync(fd) == 0, "fsync");
}

Result Run(int fd, const std::vector<off_t> &offsets, int advice)
{
    Check(::posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED) == 0, "posix_fadvise(DONTNEED)");
    Check(::posix_fadvise(fd, 0, 0, advice) == 0, "posix_fadvise(workload)");
    std::array<uint8_t, PAGE_SIZE> buffer{};
    rusage before{};
    rusage after{};
    Check(::getrusage(RUSAGE_SELF, &before) == 0, "getrusage(before)");
    auto begin = std::chrono::steady_clock::now();
    uint64_t checksum = 0;
    for (off_t offset : offsets) {
        Check(::pread(fd, buffer.data(), buffer.size(), offset) == static_cast<ssize_t>(buffer.size()), "pread");
        checksum += buffer[0];
    }
    auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
    Check(::getrusage(RUSAGE_SELF, &after) == 0, "getrusage(after)");
    if (checksum == UINT64_MAX) {
        std::fprintf(stderr, "unreachable checksum=%llu\n", static_cast<unsigned long long>(checksum));
    }
    return { static_cast<double>(offsets.size() * PAGE_SIZE) / (1024.0 * 1024.0) / elapsed,
             after.ru_majflt - before.ru_majflt, after.ru_minflt - before.ru_minflt };
}

void Report(const char *workload, const char *adviceName, int fd, const std::vector<off_t> &offsets, int advice)
{
    std::vector<double> throughput;
    long majorFaults = 0;
    long minorFaults = 0;
    for (int round = 0; round < 5; ++round) {
        auto result = Run(fd, offsets, advice);
        throughput.emplace_back(result.mibPerSecond);
        majorFaults += result.majorFaults;
        minorFaults += result.minorFaults;
    }
    std::sort(throughput.begin(), throughput.end());
    std::printf("[FADVISE] workload=%s advice=%s median_mib_s=%.1f major_faults=%ld minor_faults=%ld\n", workload,
                adviceName, throughput[throughput.size() / 2], majorFaults, minorFaults);
}
}  // namespace

int main(int argc, char **argv)
{
    std::string path = argc > 1 ? argv[1] : "/tmp/perf_glm_fadvise.data";
    int fd = ::open(path.c_str(), O_CREAT | O_TRUNC | O_RDWR, 0600);
    Check(fd >= 0, "open");
    PrepareFile(fd);

    const size_t readPages = READ_SIZE / PAGE_SIZE;
    const size_t filePages = FILE_SIZE / PAGE_SIZE;
    std::vector<off_t> sequential;
    std::vector<off_t> random;
    sequential.reserve(readPages);
    random.reserve(filePages);
    for (size_t page = 0; page < filePages; ++page) {
        random.emplace_back(static_cast<off_t>(page * PAGE_SIZE));
        if (page < readPages) {
            sequential.emplace_back(static_cast<off_t>(page * PAGE_SIZE));
        }
    }
    std::mt19937 generator(0x5a17U);
    std::shuffle(random.begin(), random.end(), generator);
    random.resize(readPages);

    Report("random", "normal", fd, random, POSIX_FADV_NORMAL);
    Report("random", "sequential", fd, random, POSIX_FADV_SEQUENTIAL);
    Report("random", "random", fd, random, POSIX_FADV_RANDOM);
    Report("sequential", "normal", fd, sequential, POSIX_FADV_NORMAL);
    Report("sequential", "sequential", fd, sequential, POSIX_FADV_SEQUENTIAL);
    Report("sequential", "random", fd, sequential, POSIX_FADV_RANDOM);

    Check(::close(fd) == 0, "close");
    Check(::unlink(path.c_str()) == 0, "unlink");
    return 0;
}
