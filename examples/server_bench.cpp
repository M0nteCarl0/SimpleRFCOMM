#include "simple_rfcomm/simple_rfcomm.hpp"
#include <iostream>
#include <vector>
#include <chrono>
#include <string>

// Default benchmark service UUID: e3b0c442-98fc-1c14-9afe-4c9c2e0b5f12
static constexpr std::string_view DEFAULT_BENCH_UUID = "e3b0c442-98fc-1c14-9afe-4c9c2e0b5f12";

int main(int argc, char* argv[]) {
    std::string uuid_str = std::string(DEFAULT_BENCH_UUID);
    std::string service_name = "SimpleRFCOMM HighSpeed Bench";
    uint8_t channel = 0; // 0 = auto-assign
    bool echo_mode = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--uuid" && i + 1 < argc) {
            uuid_str = argv[++i];
        } else if (arg == "--channel" && i + 1 < argc) {
            channel = static_cast<uint8_t>(std::stoi(argv[++i]));
        } else if (arg == "--name" && i + 1 < argc) {
            service_name = argv[++i];
        } else if (arg == "--echo") {
            echo_mode = true;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: server_bench [options]\n"
                      << "  --uuid <uuid>        Service UUID (default: " << DEFAULT_BENCH_UUID << ")\n"
                      << "  --channel <ch>       Listen channel 1..30 (0 for auto-assign, default: 0)\n"
                      << "  --name <str>         Service name in SDP\n"
                      << "  --echo               Echo received data back to client\n";
            return 0;
        }
    }

    auto uuid = rfcomm::Uuid::parse(uuid_str);

    std::cout << "========================================================\n"
              << "       SimpleRFCOMM Benchmark Server (C++20)            \n"
              << "========================================================\n"
              << "Service Name : " << service_name << "\n"
              << "Service UUID : " << uuid.to_string() << "\n"
              << "Mode         : " << (echo_mode ? "Echo mode" : "Throughput Sink mode") << "\n";

    rfcomm::Server server;
    if (!server.listen_with_sdp(uuid, service_name, channel)) {
        std::cerr << "[ERROR] Failed to start server and publish SDP record!\n";
        return 1;
    }

    std::cout << "Server successfully listening on RFCOMM channel " << static_cast<int>(server.channel())
              << " with SDP published.\n"
              << "Waiting for client connection...\n" << std::flush;

    while (true) {
        auto sock = server.accept();
        if (!sock) {
            std::cerr << "[ERROR] Accept failed or server closed.\n";
            break;
        }

        std::cout << "\n[+] Client connected from " << sock->peer_address().to_string()
                  << " (peer channel " << static_cast<int>(sock->peer_channel()) << ")\n";

        std::vector<uint8_t> buffer(32 * 1024);
        size_t total_bytes = 0;
        auto start_time = std::chrono::steady_clock::now();

        while (true) {
            auto r = sock->recv(buffer);
            if (r <= 0) {
                break; // Client closed connection or error
            }

            total_bytes += static_cast<size_t>(r);

            if (echo_mode) {
                if (!sock->send_all(std::span<const uint8_t>(buffer.data(), static_cast<size_t>(r)))) {
                    std::cerr << "[!] Failed to echo data back\n";
                    break;
                }
            }
        }

        auto end_time = std::chrono::steady_clock::now();
        std::chrono::duration<double> duration = end_time - start_time;
        double secs = duration.count();
        if (secs <= 0.0001) secs = 0.0001;

        double kb = static_cast<double>(total_bytes) / 1024.0;
        double mb = kb / 1024.0;
        double kb_s = kb / secs;
        double mbps = (kb_s * 8.0) / 1000.0;

        std::cout << "[-] Client disconnected.\n"
                  << "---------------- Benchmark Results ----------------\n"
                  << "Total Received : " << total_bytes << " bytes (" << mb << " MB)\n"
                  << "Elapsed Time   : " << secs << " s\n"
                  << "Throughput     : " << kb_s << " KB/s (" << mbps << " Mbps)\n"
                  << "---------------------------------------------------\n"
                  << "Waiting for next connection (Press Ctrl+C to stop)...\n" << std::flush;
    }

    return 0;
}
