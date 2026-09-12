#include "simple_rfcomm/simple_rfcomm.hpp"
#include <iostream>
#include <string>
#include <thread>
#include <chrono>

using namespace std::chrono_literals;

static constexpr std::string_view DEMO_UUID = "e3b0c442-98fc-1c14-9afe-4c9c2e0b5f14";

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage:\n"
                  << "  Server mode : serial_port_demo server\n"
                  << "  Client mode : serial_port_demo client <target_bd_addr>\n";
        return 1;
    }

    std::string mode = argv[1];
    auto uuid = rfcomm::Uuid::parse(DEMO_UUID);

    rfcomm::SerialPort serial;

    if (mode == "server") {
        rfcomm::Server server;
        std::cout << "[Server] Listening with SDP UUID " << uuid.to_string() << "...\n" << std::flush;
        if (!server.listen_with_sdp(uuid, "FTDI Serial Service")) {
            std::cerr << "[Server] Listen failed!\n" << std::flush;
            return 1;
        }

        std::cout << "[Server] Waiting for client on channel " << static_cast<int>(server.channel()) << "...\n" << std::flush;
        auto raw_sock = server.accept();
        if (!raw_sock) {
            std::cerr << "[Server] Accept failed!\n";
            return 1;
        }

        // Attach connected socket to FTDI-style SerialPort
        if (!serial.attach(std::move(raw_sock))) {
            std::cerr << "[Server] Failed to attach socket to SerialPort!\n";
            return 1;
        }

        std::cout << "[Server] Client connected: " << serial.peer_address().to_string() << "\n";
        serial.set_timeouts(5000ms, 5000ms);

        // Emulate an AT-command sensor or microcontroller
        while (serial.is_open()) {
            std::string cmd = serial.read_line('\n');
            if (cmd.empty()) {
                if (!serial.is_open()) break;
                continue;
            }

            std::cout << "[Server RX Line]: \"" << cmd << "\"\n";

            if (cmd == "AT") {
                serial.write_line("OK");
            } else if (cmd == "AT+INFO") {
                serial.write_line("+INFO: SimpleRFCOMM FTDI Virtual Device v1.0");
                serial.write_line("OK");
            } else if (cmd == "AT+PURGE") {
                serial.purge(rfcomm::PurgeFlags::All);
                serial.write_line("OK");
            } else if (cmd == "AT+QUIT") {
                serial.write_line("BYE");
                break;
            } else {
                serial.write_line("ERROR: UNKNOWN COMMAND");
            }
        }

        std::cout << "[Server] Session finished.\n";

    } else if (mode == "client") {
        if (argc < 3) {
            std::cerr << "Please provide target Bluetooth MAC address!\n";
            return 1;
        }

        std::string target_mac = argv[2];
        std::cout << "[Client] Opening SerialPort to " << target_mac << " via SDP...\n" << std::flush;

        // FTDI-style open_by_uuid with 10s timeout
        if (!serial.open_by_uuid(target_mac, DEMO_UUID, 10000ms)) {
            std::cerr << "[Client] Failed to open Bluetooth serial port!\n";
            return 1;
        }

        std::cout << "[Client] Connected to channel " << static_cast<int>(serial.peer_channel()) << "!\n";

        // Configure virtual serial settings (FTDI D2XX style)
        serial.set_baud_rate(rfcomm::BaudRate::Baud115200);
        serial.set_data_characteristics(rfcomm::DataBits::Bits8, rfcomm::StopBits::One, rfcomm::Parity::None);
        serial.set_timeouts(3000ms, 3000ms);

        // Send commands and read line responses
        std::vector<std::string> commands = {"AT", "AT+INFO", "AT+PURGE", "AT+QUIT"};

        for (const auto& cmd : commands) {
            std::cout << "\n[Client TX]: " << cmd << "\n";
            serial.write_line(cmd);

            // Wait a brief moment and check queue status
            std::this_thread::sleep_for(100ms);
            size_t available = serial.get_rx_queue_status();
            std::cout << "[Client Status] Bytes waiting in RX queue: " << available << "\n";

            // Read lines until OK or BYE
            while (true) {
                std::string resp = serial.read_line('\n', 2000ms);
                if (resp.empty()) break;
                std::cout << "[Client RX]: " << resp << "\n";
                if (resp == "OK" || resp == "BYE" || resp.starts_with("ERROR")) {
                    break;
                }
            }
        }

        serial.close();
        std::cout << "\n[Client] Port closed successfully.\n";
    }

    return 0;
}
