// Sony SRG-300H UDP power inquiry, v0.0.14. Standard C++17 + OS sockets only.
// Protocol: Sony A-ES6-100-12(1), printed pp.29-32 and 39.
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
using Socket = SOCKET;
constexpr Socket InvalidSocket = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
using Socket = int;
constexpr Socket InvalidSocket = -1;
#endif
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <thread>

using Bytes = std::vector<unsigned char>;
using Clock = std::chrono::steady_clock;
std::ofstream trace;
void log(const std::string &message)
{
    std::cout << message << std::endl;
    if (trace) trace << message << std::endl;
}
std::string hex(const Bytes &bytes)
{
    std::ostringstream out;
    for (size_t i = 0; i < bytes.size(); ++i) {
        if (i) out << ' ';
        out << std::uppercase << std::hex << std::setfill('0') << std::setw(2) << unsigned(bytes[i]);
    }
    return out.str();
}
int socketError()
{
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}
struct Network {
    Network() {
#ifdef _WIN32
        WSADATA data{};
        const int result = WSAStartup(MAKEWORD(2, 2), &data);
        if (result) throw std::runtime_error("WSAStartup failed: " + std::to_string(result));
#endif
    }
    ~Network() {
#ifdef _WIN32
        WSACleanup();
#endif
    }
};
struct UdpSocket {
    Socket value = InvalidSocket;
    UdpSocket() : value(socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) {
        if (value == InvalidSocket) throw std::runtime_error("UDP socket creation failed");
    }
    ~UdpSocket() {
#ifdef _WIN32
        closesocket(value);
#else
        close(value);
#endif
    }
    UdpSocket(const UdpSocket &) = delete;
    UdpSocket &operator=(const UdpSocket &) = delete;
};
Bytes packet(unsigned type, std::uint32_t sequence, const Bytes &payload)
{
    Bytes result{static_cast<unsigned char>(type >> 8), static_cast<unsigned char>(type),
                 0, static_cast<unsigned char>(payload.size()),
                 static_cast<unsigned char>(sequence >> 24), static_cast<unsigned char>(sequence >> 16),
                 static_cast<unsigned char>(sequence >> 8), static_cast<unsigned char>(sequence)};
    result.insert(result.end(), payload.begin(), payload.end());
    return result;
}
void sendPacket(Socket socket, const sockaddr_in &camera, const Bytes &bytes)
{
    log("TX  " + hex(bytes));
    const int sent = static_cast<int>(sendto(socket, reinterpret_cast<const char *>(bytes.data()),
                                          static_cast<int>(bytes.size()), 0,
                                          reinterpret_cast<const sockaddr *>(&camera), sizeof(camera)));
    if (sent != static_cast<int>(bytes.size()))
        throw std::runtime_error("UDP send failed; OS error " + std::to_string(socketError()));
}
// Fixed absolute deadline: malformed or unrelated replies cannot extend the wait.
Bytes receiveReply(Socket socket, const sockaddr_in &camera, unsigned expectedType, std::uint32_t sequence,
                   bool matchSequence, int timeoutMs)
{
    const auto deadline = Clock::now() + std::chrono::milliseconds(timeoutMs);
    for (;;) {
        const auto remaining = std::chrono::duration_cast<std::chrono::microseconds>(deadline - Clock::now()).count();
        if (remaining <= 0) throw std::runtime_error("Timed out waiting for a valid reply");
        fd_set ready;
        FD_ZERO(&ready);
        FD_SET(socket, &ready);
        timeval timeout{};
        timeout.tv_sec = static_cast<long>(remaining / 1000000);
        timeout.tv_usec = static_cast<long>(remaining % 1000000);
#ifdef _WIN32
        const int result = select(0, &ready, nullptr, nullptr, &timeout);
#else
        const int result = select(socket + 1, &ready, nullptr, nullptr, &timeout);
#endif
        if (result == 0) throw std::runtime_error("Timed out waiting for a valid reply");
        if (result < 0) throw std::runtime_error("UDP wait failed; OS error " + std::to_string(socketError()));
        char buffer[2048];
        sockaddr_in sender{};
#ifdef _WIN32
        int senderLength = sizeof(sender);
#else
        socklen_t senderLength = sizeof(sender);
#endif
        const int length = static_cast<int>(recvfrom(socket, buffer, sizeof(buffer), 0,
            reinterpret_cast<sockaddr *>(&sender), &senderLength));
        if (length < 0) throw std::runtime_error("UDP receive failed; OS error " + std::to_string(socketError()));
        Bytes bytes(buffer, buffer + length);
        char senderAddress[INET_ADDRSTRLEN]{};
        inet_ntop(AF_INET, &sender.sin_addr, senderAddress, sizeof(senderAddress));
        log("RX from " + std::string(senderAddress) + ":" + std::to_string(ntohs(sender.sin_port)) + "  " + hex(bytes));
        if (sender.sin_addr.s_addr != camera.sin_addr.s_addr) {
            log("    Ignored: sender IP is not the selected camera"); continue;
        }
        if (bytes.size() < 9) { log("    Ignored: shorter than Sony header plus payload"); continue; }
        const unsigned type = (unsigned(bytes[0]) << 8) | bytes[1];
        const unsigned count = (unsigned(bytes[2]) << 8) | bytes[3];
        const std::uint32_t receivedSequence = (std::uint32_t(bytes[4]) << 24) |
            (std::uint32_t(bytes[5]) << 16) | (std::uint32_t(bytes[6]) << 8) | bytes[7];
        if (count < 1 || count > 16 || bytes.size() != 8 + count) {
            log("    Ignored: invalid payload length"); continue;
        }
        // RESET has no meaningful sequence number; inquiry replies must echo it.
        if (matchSequence && receivedSequence != sequence) {
            log("    Ignored: sequence mismatch (received " + std::to_string(receivedSequence) + ")"); continue;
        }
        const Bytes payload(bytes.begin() + 8, bytes.end());
        if (type == 0x0200 && payload.size() == 2 && payload[0] == 0x0F)
            throw std::runtime_error("Sony control ERROR: " + hex(payload));
        if (type != expectedType) { log("    Ignored: unexpected payload type"); continue; }
        return payload;
    }
}
std::string hexWord(unsigned value)
{
    std::ostringstream out;
    out << "0x" << std::uppercase << std::hex << std::setw(4) << std::setfill('0') << value;
    return out.str();
}
void validateInquiry(const Bytes &payload, size_t expected, const std::string &label)
{
    if (payload.size() == 4 && payload[0] == 0x90 && (payload[1] & 0xF0) == 0x60 && payload[3] == 0xFF)
        throw std::runtime_error(label + " VISCA error: " + hex(payload));
    if (payload.size() != expected || payload[0] != 0x90 || payload[1] != 0x50 || payload.back() != 0xFF)
        throw std::runtime_error(label + " unexpected reply: " + hex(payload));
}
unsigned nibbleWord(const Bytes &payload, size_t first)
{
    unsigned word = 0;
    for (size_t i = first; i < first + 4; ++i) {
        if (payload.at(i) > 0x0F) throw std::runtime_error("Invalid position nibble in reply: " + hex(payload));
        word = (word << 4) | payload[i];
    }
    return word;
}
int signedWord(unsigned word) { return word >= 0x8000 ? static_cast<int>(word) - 65536 : static_cast<int>(word); }

int number(const std::string &text, int low, int high)
{
    size_t end = 0;
    int result;
    try { result = std::stoi(text, &end); }
    catch (const std::exception &) { throw std::runtime_error("Invalid numeric argument: " + text); }
    if (end != text.size() || result < low || result > high) throw std::runtime_error("Invalid numeric argument: " + text);
    return result;
}
int main(int argc, char **argv)
{
    if (argc > 1 && std::string(argv[1]) == "--help") {
        std::cout << "SonyUdpProbe [IPv4=192.168.0.100] [port=52381] [timeout-ms=3000] [local-port=52381] [inquiry-sequence=1] [mode=compare|positions|identity|zoom-test|compare-seq2|power-seq2|spaced-zoom]\n";
        return 0;
    }
    try {
        if (argc > 7) throw std::runtime_error("Too many arguments; use --help");
        const std::string host = argc > 1 ? argv[1] : "192.168.0.100";
        const int port = argc > 2 ? number(argv[2], 1, 65535) : 52381;
        const int timeoutMs = argc > 3 ? number(argv[3], 50, 30000) : 3000;
        const int localPort = argc > 4 ? number(argv[4], 1, 65535) : 52381;
        std::uint32_t sequence = argc > 5 ? static_cast<std::uint32_t>(number(argv[5], 0, 2147483647)) : 1;
        const std::string mode = argc > 6 ? argv[6] : "zoom-test";
        if (mode != "compare" && mode != "positions" && mode != "identity" && mode != "zoom-test" && mode != "compare-seq2" && mode != "power-seq2" && mode != "spaced-zoom")
            throw std::runtime_error("Invalid mode: use compare, positions, identity, zoom-test, compare-seq2, power-seq2 or spaced-zoom");
        std::time_t now = std::time(nullptr);
        std::tm local{};
#ifdef _WIN32
        localtime_s(&local, &now);
#else
        localtime_r(&now, &local);
#endif
        std::ostringstream filename;
        filename << "SonyUdpProbe-" << std::put_time(&local, "%Y%m%d-%H%M%S") << ".log";
        trace.open(filename.str(), std::ios::app);
        if (!trace) throw std::runtime_error("Cannot create trace log in current folder");
        log("Sony SRG-300H UDP inquiry probe v0.0.14");
        log("Test mode: " + mode);
        log("Trace file: " + filename.str());
        std::ostringstream timestamp;
        timestamp << std::put_time(&local, "%Y-%m-%d %H:%M:%S");
        log("Local time: " + timestamp.str());
        log("Target: " + host + ":" + std::to_string(port) + " UDP; camera address 1");
        if (mode == "zoom-test")
            log("MOVEMENT TEST: low-speed telephoto for 250 ms, then STOP. Leaves zoom at the new position.");
        else log("Protocol sequence RESET, then selected INQUIRIES only. No camera power-state or movement changes.");
        Network network;
        UdpSocket socket;
        sockaddr_in camera{};
        camera.sin_family = AF_INET;
        camera.sin_port = htons(static_cast<unsigned short>(port));
        if (inet_pton(AF_INET, host.c_str(), &camera.sin_addr) != 1)
            throw std::runtime_error("Enter a numeric IPv4 address");
        // A separate socket discovers the route. The receiving socket remains
        // unconnected so a different camera reply source port is visible.
        UdpSocket route;
        if (connect(route.value, reinterpret_cast<const sockaddr *>(&camera), sizeof(camera)) != 0)
            throw std::runtime_error("Cannot select UDP peer; OS error " + std::to_string(socketError()));
        sockaddr_in endpoint{};
#ifdef _WIN32
        int endpointLength = sizeof(endpoint);
#else
        socklen_t endpointLength = sizeof(endpoint);
#endif
        if (getsockname(route.value, reinterpret_cast<sockaddr *>(&endpoint), &endpointLength) != 0)
            throw std::runtime_error("Cannot identify local route; OS error " + std::to_string(socketError()));
        endpoint.sin_port = htons(static_cast<unsigned short>(localPort));
#ifdef _WIN32
        // Never silently share the receiver with another controller/process.
        const BOOL exclusive = TRUE;
        if (setsockopt(socket.value, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                       reinterpret_cast<const char *>(&exclusive), sizeof(exclusive)) != 0)
            throw std::runtime_error("Cannot set exclusive UDP binding");
#endif
        if (bind(socket.value, reinterpret_cast<const sockaddr *>(&endpoint), sizeof(endpoint)) != 0)
            throw std::runtime_error("Cannot bind local UDP port " + std::to_string(localPort) +
                "; OS error " + std::to_string(socketError()) + ". Close other probe/controller instances using that port.");
        if (getsockname(socket.value, reinterpret_cast<sockaddr *>(&endpoint), &endpointLength) != 0)
            throw std::runtime_error("Cannot identify local endpoint");
        char address[INET_ADDRSTRLEN]{};
        inet_ntop(AF_INET, &endpoint.sin_addr, address, sizeof(address));
        log("Local endpoint: " + std::string(address) + ":" + std::to_string(ntohs(endpoint.sin_port)));
        log("Listening for replies from camera IP on ANY source port.");
        if (mode == "spaced-zoom") {
            unsigned passed = 0;
            for (unsigned zoomType : {0x0110u, 0x0100u}) {
                const std::string label = zoomType == 0x0110 ? "Zoom header 01 10" : "Zoom header 01 00";
                log("--- " + label + ": power 1, zoom 2, power 3, zoom 4 ---");
                try {
                    sendPacket(socket.value, camera, packet(0x0200, 0, {0x01}));
                    const Bytes ack = receiveReply(socket.value, camera, 0x0201, 0, false, timeoutMs);
                    if (ack != Bytes{0x01}) throw std::runtime_error("Unexpected reset ACK: " + hex(ack));
                } catch (const std::exception &error) {
                    log("FAIL: reset for " + label + " - " + error.what());
                    continue;
                }
                for (std::uint32_t seq : {1u,2u,3u,4u}) {
                    log("Waiting 100 ms before next inquiry.");
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    const bool powerQuery = (seq % 2) == 1;
                    const std::string name = powerQuery ? "Power" : label;
                    log(name + ", sequence " + std::to_string(seq));
                    try {
                        sendPacket(socket.value, camera, packet(powerQuery ? 0x0110 : zoomType, seq,
                            powerQuery ? Bytes{0x81,0x09,0x04,0x00,0xFF} : Bytes{0x81,0x09,0x04,0x47,0xFF}));
                        const Bytes response = receiveReply(socket.value, camera, 0x0111, seq, true, timeoutMs);
                        validateInquiry(response, powerQuery ? 4 : 7, name);
                        if (powerQuery) {
                            if (response[2] != 0x02 && response[2] != 0x03)
                                throw std::runtime_error("Unknown power state: " + hex(response));
                            log("PASS: Power sequence " + std::to_string(seq) + " = " +
                                (response[2] == 0x02 ? "ON" : "STANDBY"));
                        } else {
                            const unsigned zoom = nibbleWord(response, 2);
                            log("PASS: " + name + " sequence " + std::to_string(seq) +
                                "; position=" + hexWord(zoom) + " (raw " + std::to_string(zoom) + ")");
                        }
                        ++passed;
                    } catch (const std::exception &error) {
                        log("FAIL: " + name + " sequence " + std::to_string(seq) + " - " + error.what());
                        log("Continuing with next sequence after 100 ms; no intervening reset.");
                    }
                }
            }
            log("SUMMARY: " + std::to_string(passed) + "/8 spaced inquiries answered and decoded.");
            log("No movement commands sent. Trace saved: " + filename.str());
            return passed == 8 ? 0 : (passed ? 2 : 1);
        }
        if (mode == "power-seq2") {
            log("CONTROL TEST: one reset, then identical power inquiries at sequences 1 and 2.");
            sendPacket(socket.value, camera, packet(0x0200, 0, {0x01}));
            const Bytes ack = receiveReply(socket.value, camera, 0x0201, 0, false, timeoutMs);
            if (ack != Bytes{0x01}) throw std::runtime_error("Unexpected reset ACK: " + hex(ack));
            log("Reset acknowledged; waiting 100 ms before next command.");
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            for (std::uint32_t seq : {1u, 2u}) {
                if (seq == 2) {
                    log("First power reply validated; waiting 100 ms before sequence 2.");
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
                log("Power inquiry, sequence " + std::to_string(seq));
                sendPacket(socket.value, camera, packet(0x0110, seq, {0x81,0x09,0x04,0x00,0xFF}));
                const Bytes response = receiveReply(socket.value, camera, 0x0111, seq, true, timeoutMs);
                validateInquiry(response, 4, "Power");
                if (response[2] != 0x02 && response[2] != 0x03)
                    throw std::runtime_error("Unknown power state: " + hex(response));
                log("PASS: power sequence " + std::to_string(seq) + " = " +
                    (response[2] == 0x02 ? "ON" : "STANDBY"));
            }
            log("PASS: both power inquiries answered, including the second request without another reset.");
            log("No movement commands sent. Trace saved: " + filename.str());
            return 0;
        }
        if (mode == "compare-seq2") {
            unsigned passed = 0;
            for (unsigned type : {0x0110u, 0x0100u}) {
                const std::string label = type == 0x0110 ? "Zoom 01 10 / sequence 2" : "Zoom 01 00 / sequence 2";
                log("--- Test: " + label + " ---");
                try {
                    sendPacket(socket.value, camera, packet(0x0200, 0, {0x01}));
                    const Bytes ack = receiveReply(socket.value, camera, 0x0201, 0, false, timeoutMs);
                    if (ack != Bytes{0x01}) throw std::runtime_error("Unexpected reset ACK: " + hex(ack));
                    log("Reset acknowledged; waiting 100 ms before next command.");
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    sendPacket(socket.value, camera, packet(0x0110, 1, {0x81,0x09,0x04,0x00,0xFF}));
                    const Bytes power = receiveReply(socket.value, camera, 0x0111, 1, true, timeoutMs);
                    validateInquiry(power, 4, "Power");
                    if (power != Bytes{0x90,0x50,0x02,0xFF})
                        throw std::runtime_error("Camera is not reporting power ON; zoom test skipped");
                    log("Power ON received at sequence 1; sending zoom at sequence 2.");
                    sendPacket(socket.value, camera, packet(type, 2, {0x81,0x09,0x04,0x47,0xFF}));
                    const Bytes response = receiveReply(socket.value, camera, 0x0111, 2, true, timeoutMs);
                    validateInquiry(response, 7, "Zoom position");
                    const unsigned zoom = nibbleWord(response, 2);
                    log("PASS: " + label + "; position=" + hexWord(zoom) + " (raw " + std::to_string(zoom) + ")");
                    ++passed;
                } catch (const std::exception &error) {
                    log("FAIL: " + label + " - " + error.what());
                }
            }
            log("SUMMARY: " + std::to_string(passed) + "/2 sequence-2 zoom inquiries answered and decoded.");
            log("No movement commands sent. Trace saved: " + filename.str());
            return passed == 2 ? 0 : (passed ? 2 : 1);
        }
        if (mode == "compare") {
            struct Case { const char *name; unsigned type; Bytes query; size_t size; int decode; };
            const std::vector<Case> cases{
                {"Power / 01 10", 0x0110, {0x81,0x09,0x04,0x00,0xFF}, 4, 0},
                {"Zoom / 01 10", 0x0110, {0x81,0x09,0x04,0x47,0xFF}, 7, 1},
                {"Pan-tilt / 01 10", 0x0110, {0x81,0x09,0x06,0x12,0xFF}, 11, 2},
                {"Bitfocus lens block / 01 10", 0x0110, {0x81,0x09,0x7E,0x7E,0x00,0xFF}, 16, 1},
                {"Python-style zoom / 01 00", 0x0100, {0x81,0x09,0x04,0x47,0xFF}, 7, 1},
                {"Python-style pan-tilt / 01 00", 0x0100, {0x81,0x09,0x06,0x12,0xFF}, 11, 2},
            };
            std::vector<std::string> outcomes;
            size_t passed = 0;
            for (const auto &test : cases) {
                log(std::string("--- Test: ") + test.name + " ---");
                try {
                    log("Fresh sequence-counter reset; await ACK before inquiry.");
                    sendPacket(socket.value, camera, packet(0x0200, 0, {0x01}));
                    const Bytes ack = receiveReply(socket.value, camera, 0x0201, 0, false, timeoutMs);
                    if (ack != Bytes{0x01}) throw std::runtime_error("Unexpected reset ACK: " + hex(ack));
                    log("Reset acknowledged; waiting 100 ms before next command.");
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    log("Inquiry sequence " + std::to_string(sequence) + "; no other inquiry precedes this one.");
                    sendPacket(socket.value, camera, packet(test.type, sequence, test.query));
                    const Bytes response = receiveReply(socket.value, camera, 0x0111, sequence, true, timeoutMs);
                    validateInquiry(response, test.size, test.name);
                    if (test.decode == 0) {
                        if (response[2] == 0x02) log("Power: ON");
                        else if (response[2] == 0x03) log("Power: STANDBY");
                        else throw std::runtime_error("Unknown power state: " + hex(response));
                    } else if (test.decode == 1) {
                        const unsigned zoom = nibbleWord(response, 2);
                        log("Zoom: " + hexWord(zoom) + " (raw " + std::to_string(zoom) + ")");
                    } else {
                        const unsigned pan = nibbleWord(response, 2), tilt = nibbleWord(response, 6);
                        log("Pan: " + hexWord(pan) + "; tilt: " + hexWord(tilt));
                        log("Signed counts: pan=" + std::to_string(signedWord(pan)) + "; tilt=" +
                            std::to_string(signedWord(tilt)) + " (not degrees)");
                    }
                    ++passed;
                    outcomes.push_back(std::string("PASS: ") + test.name);
                } catch (const std::exception &error) {
                    outcomes.push_back(std::string("FAIL: ") + test.name + " - " + error.what());
                    log(outcomes.back());
                    log("Next test starts with its own protocol reset; no retry of this request.");
                }
            }
            log("=== COMPARISON SUMMARY ===");
            for (const auto &outcome : outcomes) log(outcome);
            log(std::to_string(passed) + "/" + std::to_string(cases.size()) + " inquiries answered and decoded.");
            log("No camera movements, power-state changes or preset writes were requested.");
            log("Trace saved: " + filename.str());
            return passed == cases.size() ? 0 : (passed ? 2 : 1);
        }
        log("Step 1: reset protocol sequence counter (not camera settings)");
        sendPacket(socket.value, camera, packet(0x0200, 0, {0x01}));
        const Bytes reset = receiveReply(socket.value, camera, 0x0201, 0, false, timeoutMs);
        if (reset != Bytes{0x01}) throw std::runtime_error("Unexpected RESET acknowledgement: " + hex(reset));
                    log("Reset acknowledged; waiting 100 ms before next command.");
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
        log("Sequence reset acknowledged.");
        if (mode == "identity") {
        log("Step 2: model/ROM inquiry (Sony device-setting type 01 20), sequence " + std::to_string(sequence));
        sendPacket(socket.value, camera, packet(0x0120, sequence, {0x81, 0x09, 0x00, 0x02, 0xFF}));
        const Bytes identity = receiveReply(socket.value, camera, 0x0111, sequence, true, timeoutMs);
        validateInquiry(identity, 10, "Model/ROM");
        const unsigned vendor = (unsigned(identity[2]) << 8) | identity[3];
        const unsigned model = (unsigned(identity[4]) << 8) | identity[5];
        const unsigned rom = (unsigned(identity[6]) << 8) | identity[7];
        log("PASS: vendor=" + hexWord(vendor) + (vendor == 1 ? " (Sony)" : " (unrecognized vendor)") +
            "; model=" + hexWord(model) + (vendor == 1 && model == 0x0513 ? " (SRG-300H)" : " (unrecognized model)") +
            "; ROM revision=" + hexWord(rom) + "; maximum VISCA command sockets=" + std::to_string(identity[8]));
        log("ROM is reported as raw hexadecimal; no assumed dotted firmware version.");
            log("PASS: separate identification inquiry completed.");
            log("Trace saved: " + filename.str());
            return 0;
        }
        log("Step 2: power inquiry, sequence " + std::to_string(sequence));
        sendPacket(socket.value, camera, packet(0x0110, sequence, {0x81, 0x09, 0x04, 0x00, 0xFF}));
        const Bytes power = receiveReply(socket.value, camera, 0x0111, sequence, true, timeoutMs);
        validateInquiry(power, 4, "Power");
        const bool on = power == Bytes{0x90, 0x50, 0x02, 0xFF};
        if (on) log("PASS: camera power is ON.");
        else if (power == Bytes{0x90, 0x50, 0x03, 0xFF}) log("PASS: camera power is STANDBY.");
        else throw std::runtime_error("Unexpected power state: " + hex(power));

        if (!on) {
            log("PARTIAL: power received; position inquiries skipped because camera is in standby.");
            log("Trace saved: " + filename.str());
            return 2;
        }

        if (mode == "zoom-test") {
            auto readZoom = [&]() {
                log("Waiting 100 ms before zoom inquiry.");
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                ++sequence;
                log("Zoom inquiry using outer header 01 10, sequence " + std::to_string(sequence));
                sendPacket(socket.value, camera, packet(0x0110, sequence, {0x81,0x09,0x04,0x47,0xFF}));
                const Bytes value = receiveReply(socket.value, camera, 0x0111, sequence, true, timeoutMs);
                validateInquiry(value, 7, "Zoom position");
                return nibbleWord(value, 2);
            };
            const unsigned before = readZoom();
            log("BEFORE zoom: " + hexWord(before) + " (raw " + std::to_string(before) + ")");
            if (before >= 0x4000) {
                log("INCONCLUSIVE: already at/above optical telephoto limit; no movement requested.");
                return 2;
            }
            auto stop = [&]() {
                ++sequence;
                log("Sending zoom STOP, sequence " + std::to_string(sequence));
                sendPacket(socket.value, camera, packet(0x0100, sequence, {0x81,0x01,0x04,0x07,0x00,0xFF}));
            };
            try {
                log("Waiting 100 ms before TELE command.");
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                ++sequence;
                log("Low-speed TELE for 250 ms; STOP will be sent without waiting for a drive reply.");
                sendPacket(socket.value, camera, packet(0x0100, sequence, {0x81,0x01,0x04,0x07,0x20,0xFF}));
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
                stop();
                const auto deadline = Clock::now() + std::chrono::milliseconds(timeoutMs);
                for (;;) {
                    const int left = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now()).count());
                    if (left <= 0) throw std::runtime_error("Zoom STOP completion timed out");
                    const Bytes response = receiveReply(socket.value, camera, 0x0111, sequence, true, left);
                    if (response.size() == 3 && response[0] == 0x90 && response[2] == 0xFF) {
                        if (response[1] == 0x51 || response[1] == 0x52) break;
                        if (response[1] == 0x41 || response[1] == 0x42) { log("STOP acknowledged; awaiting completion."); continue; }
                    }
                    throw std::runtime_error("Unexpected zoom STOP response: " + hex(response));
                }
                log("Zoom STOP completion received.");
            } catch (...) {
                log("Movement/STOP failed; attempting one additional STOP before exiting.");
                try { stop(); } catch (const std::exception &error) { log(std::string("Additional STOP failed: ") + error.what()); }
                throw;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            const unsigned after = readZoom();
            log("AFTER zoom: " + hexWord(after) + " (raw " + std::to_string(after) + ")");
            if (after > before) log("PASS: zoom position increased after TELE; 01 10 inquiry returned changing position feedback.");
            else log("INCONCLUSIVE: zoom did not increase; inspect raw packets and camera movement.");
            log("Zoom remains at the new position. No presets, pan/tilt or power settings changed.");
            log("Trace saved: " + filename.str());
            return after > before ? 0 : 2;
        }

        ++sequence;
        log("Step 3: zoom-position inquiry, sequence " + std::to_string(sequence));
        sendPacket(socket.value, camera, packet(0x0110, sequence, {0x81, 0x09, 0x04, 0x47, 0xFF}));
        const Bytes zoomReply = receiveReply(socket.value, camera, 0x0111, sequence, true, timeoutMs);
        validateInquiry(zoomReply, 7, "Zoom position");
        const unsigned zoom = nibbleWord(zoomReply, 2);
        log("PASS: zoom position=" + hexWord(zoom) + " (raw " + std::to_string(zoom) + ")");

        ++sequence;
        log("Step 4: pan/tilt-position inquiry, sequence " + std::to_string(sequence));
        sendPacket(socket.value, camera, packet(0x0110, sequence, {0x81, 0x09, 0x06, 0x12, 0xFF}));
        const Bytes position = receiveReply(socket.value, camera, 0x0111, sequence, true, timeoutMs);
        validateInquiry(position, 11, "Pan/tilt position");
        const unsigned pan = nibbleWord(position, 2);
        const unsigned tilt = nibbleWord(position, 6);
        log("PASS: pan=" + hexWord(pan) + "; tilt=" + hexWord(tilt));
        log("Signed 16-bit position counts: pan=" + std::to_string(signedWord(pan)) +
            "; tilt=" + std::to_string(signedWord(tilt)) + " (not degrees)");
        log("PASS: power, zoom and pan/tilt inquiries completed; no camera movement or setting changes requested.");
        log("Trace saved: " + filename.str());
        return 0;
    } catch (const std::exception &error) {
        log(std::string("FAIL: ") + error.what());
        log("Keep the raw trace and Wireshark capture for diagnosis. A successful UDP send alone does not prove camera communication.");
        return 1;
    }
}
