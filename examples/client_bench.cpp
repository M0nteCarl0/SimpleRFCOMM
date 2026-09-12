#include "simple_rfcomm/simple_rfcomm.hpp"
#include <iostream>
#include <vector>
#include <chrono>
#include <string>
#include <numeric>

static constexpr std::string_view DEFAULT_BENCH_UUID = "e3b0c442-98fc-1c14-9afe-4c9c2e0b5f12";

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: client_bench <target_bd_addr> [options]\n"
                  << "  target_bd_addr       Target Bluetooth address (e.g. 9C:C7:D3:F6:E6:A8)\n"
                  << "Options:\n"
                  << "  --uuid <uuid>        Service UUID to search via SDP (default: " << DEFAULT_BENCH_UUID << ")\n"
                  << "  --channel <ch>       Direct channel number (skips SDP)\n"
                  << "  --size <MB>          Total data size in Megabytes (default: 2.0)\n"
                  << "  --chunk <KB>         Buffer chunk size in Kilobytes (default: 16)\n"
                  << "  --echo               Echo latency test mode (send chunk, await reply)\n";
        return 1;
    }

    std::string target_str = argv[1];
    std::string uuid_str = std::string(DEFAULT_BENCH_UUID);
    int direct_channel = 0;
    double size_mb = 2.0;
    size_t chunk_kb = 16;
    bool echo_mode = false;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--uuid" && i + 1 < argc) {
            uuid_str = argv[++i];
        } else if (arg == "--channel" && i + 1 < argc) {
            direct_channel = std::stoi(argv[++i]);
        } else if (arg == "--size" && i + 1 < argc) {
            size_mb = std::stod(argv[++i]);
        } else if (arg == "--chunk" && i + 1 < argc) {
            chunk_kb = std::stoul(argv[++i]);
        } else if (arg == "--echo") {
            echo_mode = true;
        }
    }

    auto target_addr = rfcomm::Address::parse(target_str);
    auto uuid = rfcomm::Uuid::parse(uuid_str);

    std::cout << "========================================================\n"
              << "       SimpleRFCOMM Benchmark Client (C++20)            \n"
              << "========================================================\n"
              << "Target Device : " << target_addr.to_string() << "\n";

    rfcomm::Client client;
    std::unique_ptr<rfcomm::Socket> sock;

    if (direct_channel > 0) {
        std::cout << "Connection    : Direct to channel " << direct_channel << "\n";
        sock = client.connect(target_addr, static_cast<uint8_t>(direct_channel));
    } else {
        std::cout << "Connection    : SDP Discovery for UUID " << uuid.to_string() << "...\n";
        auto start_sdp = std::chrono::steady_clock::now();
        sock = client.connect_by_sdp(target_addr, uuid);
        auto end_sdp = std::chrono::steady_clock::now();
        std::chrono::duration<double, std::milli> sdp_ms = end_sdp - start_sdp;
        std::cout << "SDP Resolution: " << (sock ? "Discovered" : "Failed") 
                  << " (took " << sdp_ms.count() << " ms)\n";
    }

    if (!sock) {
        std::cerr << "[ERROR] Could not connect to target device!\n";
        return 1;
    }

    std::cout << "[+] Connected! Target channel: " << static_cast<int>(sock->peer_channel()) << "\n";

    size_t chunk_bytes = chunk_kb * 1024;
    size_t total_bytes_to_send = static_cast<size_t>(size_mb * 1024.0 * 1024.0);
    std::vector<uint8_t> tx_buffer(chunk_bytes);
    // Fill buffer with pseudo-random pattern
    for (size_t i = 0; i < tx_buffer.size(); ++i) {
        tx_buffer[i] = static_cast<uint8_t>((i * 73 + 19) & 0xFF);
    }

    std::cout << "Mode          : " << (echo_mode ? "Round-Trip Echo & Latency" : "Streaming Throughput") << "\n"
              << "Total Size    : " << size_mb << " MB (" << total_bytes_to_send << " bytes)\n"
              << "Chunk Size    : " << chunk_kb << " KB\n"
              << "Starting benchmark...\n";

    size_t total_sent = 0;
    std::vector<double> rtt_samples;
    std::vector<uint8_t> rx_buffer;
    if (echo_mode) {
        rx_buffer.resize(chunk_bytes);
    }

    auto start_time = std::chrono::steady_clock::now();

    while (total_sent < total_bytes_to_send) {
        size_t current_chunk = std::min(chunk_bytes, total_bytes_to_send - total_sent);
        auto chunk_span = std::span<const uint8_t>(tx_buffer.data(), current_chunk);

        auto chunk_start = std::chrono::steady_clock::now();

        if (!sock->send_all(chunk_span)) {
            std::cerr << "[ERROR] send_all failed at " << total_sent << " bytes\n";
            break;
        }

        total_sent += current_chunk;

        if (echo_mode) {
            auto rx_span = std::span<uint8_t>(rx_buffer.data(), current_chunk);
            if (!sock->recv_all(rx_span)) {
                std::cerr << "[ERROR] recv_all failed at " << total_sent << " bytes\n";
                break;
            }
            auto chunk_end = std::chrono::steady_clock::now();
            std::chrono::duration<double, std::milli> rtt = chunk_end - chunk_start;
            rtt_samples.push_back(rtt.count());
        }
    }

    auto end_time = std::chrono::steady_clock::now();
    std::chrono::duration<double> duration = end_time - start_time;
    double secs = duration.count();
    if (secs <= 0.0001) secs = 0.0001;

    sock->close();

    double kb = static_cast<double>(total_sent) / 1024.0;
    double mb = kb / 1024.0;
    double kb_s = kb / secs;
    double mbps = (kb_s * 8.0) / 1000.0;

    std::cout << "\n================ Benchmark Complete ================\n"
              << "Transferred  : " << total_sent << " bytes (" << mb << " MB)\n"
              << "Elapsed Time : " << secs << " seconds\n"
              << "Speed        : " << kb_s << " KB/s (" << mbps << " Mbps)\n";

    if (echo_mode && !rtt_samples.empty()) {
        double sum_rtt = std::accumulate(rtt_samples.begin(), rtt_samples.end(), 0.0);
        double avg_rtt = sum_rtt / static_cast<double>(rtt_samples.size());
        std::cout << "Avg RTT      : " << avg_rtt << " ms per " << chunk_kb << " KB chunk\n";
    }

    std::cout << "====================================================\n";

    return 0;
}
