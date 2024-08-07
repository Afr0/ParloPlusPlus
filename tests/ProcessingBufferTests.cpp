#include "pch.h"
#include <gtest/gtest.h>
#include <vector>
#include "Parlo.h"

class ProcessingBufferTests : public ::testing::Test {
protected:
    void SetUp() override {
        //Code here will be called immediately after the constructor (right before each test).
    }

    void TearDown() override {
        //Code here will be called immediately after each test (right before the destructor).
    }

    //Helper function to simulate the OnProcessedPacket event
    void simulateOnProcessedPacket(Parlo::ProcessingBuffer& buffer, const std::vector<uint8_t>& data) {
        buffer.setOnPacketProcessedHandler([this](const Parlo::Packet&) {
            eventFired = true;
            });
        buffer.addData(data);
    }

    std::atomic<bool> eventFired{ false };
};

/* Test for adding data to the processing buffer. */
TEST_F(ProcessingBufferTests, TestAddingData) {
    std::cout << "Testing adding data to ProcessingBuffer..." << std::endl;;
    uint16_t length = 9;

    //Convert uint16_t length to two bytes in little-endian format
    uint8_t lengthLow = static_cast<uint8_t>(length & 0xFF);
    uint8_t lengthHigh = static_cast<uint8_t>((length >> 8) & 0xFF);

    Parlo::ProcessingBuffer processingBuffer;
    std::vector<uint8_t> data = { 1, 2, lengthLow, lengthHigh };
    processingBuffer.addData(data);
    ASSERT_EQ(processingBuffer.bufferCount(), 4);

    try {
        ASSERT_EQ(data[0], processingBuffer[0]);
        ASSERT_EQ(data[1], processingBuffer[1]);
        ASSERT_EQ(data[2], processingBuffer[2]);
        ASSERT_EQ(data[3], processingBuffer[3]);

        std::cout << "Passed test!" << std::endl;
    }
    catch (const std::out_of_range& e) {
        FAIL() << "Index out of range: " << e.what();
    }
}

/*Test for adding too much data to the processing buffer.*/
TEST_F(ProcessingBufferTests, TestAddingTooMuchData) {
    std::cout << "Testing adding too much data to ProcessingBuffer..." << std::endl;;

    Parlo::ProcessingBuffer processingBuffer;
    std::vector<uint8_t> data(Parlo::MAX_PACKET_SIZE + 1, 0);

    EXPECT_THROW(processingBuffer.addData(data), std::overflow_error);

    std::cout << "Passed test!..." << std::endl;;
}

/*Test for processing packet and firing event.*/
TEST_F(ProcessingBufferTests, TestProcessingPacket) {
    Parlo::ProcessingBuffer processingBuffer;
    uint16_t length = 9;

    //Convert uint16_t length to two bytes in little-endian format
    uint8_t lengthLow = static_cast<uint8_t>(length & 0xFF);
    uint8_t lengthHigh = static_cast<uint8_t>((length >> 8) & 0xFF);

    std::vector<uint8_t> data = { 1, 0, lengthLow, lengthHigh, 5, 6, 7 };

    std::cout << "Testing packet processing..." << std::endl;;

    simulateOnProcessedPacket(processingBuffer, data);

    EXPECT_FALSE(eventFired.load());

    processingBuffer.addData({ 8, 9, 10 });

    //Poll for eventFired to become true
    auto start = std::chrono::steady_clock::now();
    while (!eventFired.load() && std::chrono::steady_clock::now() - start < std::chrono::seconds(1))
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

    EXPECT_TRUE(eventFired.load());

    std::cout << "Passed test!" << std::endl;;
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
