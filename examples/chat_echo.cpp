#include "simple_rfcomm/simple_rfcomm.hpp"
#include <iostream>
#include <string>
#include <thread>
#include <atomic>

static constexpr std::string_view CHAT_UUID = "e3b0c442-98fc-1c14-9afe-4c9c2e0b5f13";

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage:\n"
                  << "  Server mode : chat_echo server [service_name]\n"
                  << "  Client mode : chat_echo client <target_bd_addr>\n";
        return 1;
    }

    std::string mode = argv[1];
    auto uuid = rfcomm::Uuid::parse(CHAT_UUID);

    std::unique_ptr<rfcomm::Socket> sock;

    if (mode == "server") {
        std::string sname = (argc > 2) ? argv[2] : "SimpleRFCOMM Chat";
        rfcomm::Server server;
        std::cout << "Starting server with SDP UUID " << uuid.to_string() << "...\n";
        if (!server.listen_with_sdp(uuid, sname)) {
            std::cerr << "Failed to start server!\n";
            return 1;
        }

        std::cout << "Waiting for peer to connect on channel " << static_cast<int>(server.channel()) << "...\n";
        sock = server.accept();
        if (!sock) {
            std::cerr << "Accept failed!\n";
            return 1;
        }
        std::cout << "Peer connected from " << sock->peer_address().to_string() << "!\n";
    } else if (mode == "client") {
        if (argc < 3) {
            std::cerr << "Please provide target Bluetooth MAC address!\n";
            return 1;
        }
        auto target = rfcomm::Address::parse(argv[2]);
        rfcomm::Client client;
        std::cout << "Discovering chat service on " << target.to_string() << " via SDP...\n";
        sock = client.connect_by_sdp(target, uuid);
        if (!sock) {
            std::cerr << "Failed to find or connect to chat service!\n";
            return 1;
        }
        std::cout << "Connected to chat server!\n";
    } else {
        std::cerr << "Unknown mode: " << mode << "\n";
        return 1;
    }

    std::cout << "Type messages and press Enter. Type 'quit' to exit.\n\n";

    std::atomic<bool> running{true};

    // Background reader thread
    std::thread reader([&sock, &running]() {
        std::vector<uint8_t> buffer(4096);
        while (running) {
            auto r = sock->recv(buffer);
            if (r <= 0) {
                if (running) {
                    std::cout << "\n[Peer disconnected]\n";
                    running = false;
                }
                break;
            }
            std::string msg(reinterpret_cast<char*>(buffer.data()), static_cast<size_t>(r));
            std::cout << "\n[Peer]: " << msg << "\n> " << std::flush;
        }
    });

    // Main thread sender
    std::string line;
    std::cout << "> " << std::flush;
    while (running && std::getline(std::cin, line)) {
        if (line == "quit") {
            running = false;
            break;
        }
        if (!sock->send_all(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(line.data()), line.size()))) {
            std::cerr << "[Send failed]\n";
            break;
        }
        std::cout << "> " << std::flush;
    }

    running = false;
    sock->close();
    if (reader.joinable()) {
        reader.join();
    }

    std::cout << "Chat finished.\n";
    return 0;
}
