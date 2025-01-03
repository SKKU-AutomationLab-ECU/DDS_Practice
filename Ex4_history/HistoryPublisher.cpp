#include "HistoryTest.h"
#include "HistoryTestPubSubTypes.h"

#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/qos/DataWriterQos.hpp>

#include <chrono>
#include <thread>
#include <iostream>
#include <random>
#include <iomanip>
#include <atomic>

using namespace eprosima::fastdds::dds;

class HistoryPublisher {
private:
    DomainParticipant* participant_;
    Publisher* publisher_;
    Topic* topic_;
    DataWriter* writer_;
    TypeSupport type_;
    SensorData data_;
    uint32_t sequence_number_;
    std::random_device rd_;
    std::mt19937 gen_;
    std::uniform_real_distribution<> temp_dist_;
    std::uniform_real_distribution<> humidity_dist_;
    std::uniform_real_distribution<> pressure_dist_;
    std::atomic<bool> burst_mode_;
    std::atomic<bool> running_;

public:
    HistoryPublisher() 
        : participant_(nullptr)
        , publisher_(nullptr)
        , topic_(nullptr)
        , writer_(nullptr)
        , type_(new SensorDataPubSubType())
        , sequence_number_(0)
        , gen_(rd_())
        , temp_dist_(20.0, 30.0)
        , humidity_dist_(40.0, 60.0)
        , pressure_dist_(995.0, 1015.0)
        , burst_mode_(false)
        , running_(true) {
    }

    ~HistoryPublisher() {
        if (publisher_ != nullptr) {
            if (writer_ != nullptr)
                publisher_->delete_datawriter(writer_);
        }
        if (participant_ != nullptr) {
            if (topic_ != nullptr)
                participant_->delete_topic(topic_);
            if (publisher_ != nullptr)
                participant_->delete_publisher(publisher_);
            DomainParticipantFactory::get_instance()->delete_participant(participant_);
        }
    }

    bool init() {
        DomainParticipantQos participantQos;
        participantQos.name("History_Publisher");
        participant_ = DomainParticipantFactory::get_instance()->create_participant(0, participantQos);
        if (participant_ == nullptr) return false;

        type_.register_type(participant_);

        publisher_ = participant_->create_publisher(PUBLISHER_QOS_DEFAULT);
        if (publisher_ == nullptr) return false;

        // Create single topic
        topic_ = participant_->create_topic("HistoryTopic", "SensorData", TOPIC_QOS_DEFAULT);
        if (topic_ == nullptr) return false;

        // Configure DataWriter QoS
        DataWriterQos writerQos = DATAWRITER_QOS_DEFAULT;
        writerQos.reliability().kind = RELIABLE_RELIABILITY_QOS;
        writer_ = publisher_->create_datawriter(topic_, writerQos);
        if (writer_ == nullptr) return false;

        return true;
    }

    bool publish() {
        

        data_.timestamp(std::chrono::system_clock::now().time_since_epoch().count());
        data_.sequence_number(sequence_number_);
        data_.temperature(temp_dist_(gen_));
        data_.humidity(humidity_dist_(gen_));
        data_.pressure(pressure_dist_(gen_));

        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);

        writer_->write(&data_);

        std::cout << std::put_time(std::localtime(&time_t), "%H:%M:%S") 
                  << " Published: Seq=" << sequence_number_
                  << " Temp=" << std::fixed << std::setprecision(1) << data_.temperature()
                  << "°C Humidity=" << data_.humidity()
                  << "% Pressure=" << data_.pressure() << "hPa"
                  << (burst_mode_ ? " [BURST MODE]" : "")
                  << std::endl;
        sequence_number_++;
        return true;
    }

    void run() {
        std::cout << "Publisher running. Commands:\n"
                  << "1. Normal mode: 1 sample per second\n"
                  << "2. Burst mode: 10 samples per second\n"
                  << "q. Quit\n"
                  << "Enter command: ";

        std::thread input_thread([this]() {
            char cmd;
            while (running_) {
                std::cin.get(cmd);
                if (cmd == '1') {
                    burst_mode_ = false;
                    std::cout << "Switched to Normal mode" << std::endl;
                }
                else if (cmd == '2') {
                    burst_mode_ = true;
                    std::cout << "Switched to Burst mode" << std::endl;
                }
                else if (cmd == 'q') {
                    running_ = false;
                }
            }
        });

        while (running_) {
            if (burst_mode_) {
                // Burst mode: 10 samples per second
                for (int i = 0; i < 10 && burst_mode_ && running_; ++i) {
                    publish();
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            } else {
                // Normal mode: 1 sample per second
                publish();
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }

        if (input_thread.joinable()) {
            input_thread.join();
        }
    }
};

int main() {
    try {
        HistoryPublisher publisher;
        if (publisher.init()) {
            publisher.run();
        }
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}