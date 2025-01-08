#include "HistoryTest.h"
#include "HistoryTestPubSubTypes.h"

#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/DataReaderListener.hpp>
#include <fastdds/dds/subscriber/qos/DataReaderQos.hpp>
#include <fastdds/dds/subscriber/SampleInfo.hpp>

#include <deque>
#include <mutex>
#include <iomanip>
#include <chrono>
#include <thread>
#include <iostream>

using namespace eprosima::fastdds::dds;

class HistoryListener : public DataReaderListener {
private:
    std::string topic_name_;
    std::deque<SensorData> history_;
    std::mutex mutex_;
    uint32_t total_samples_;
    std::atomic<size_t> display_limit_;

public:
    HistoryListener(const std::string& topic_name)
        : topic_name_(topic_name)
        , total_samples_(0)
        , display_limit_(5) {
    }

    void setDisplayLimit(size_t limit) {
        display_limit_ = limit;
    }

    void on_data_available(DataReader* reader) override {
        SensorData data;
        SampleInfo info;
        std::lock_guard<std::mutex> lock(mutex_);

        while (reader->take_next_sample(&data, &info) == ReturnCode_t::RETCODE_OK) {
            if (info.valid_data) {
                history_.push_back(data);
                total_samples_++;

                // Clear screen and move cursor to top
                std::cout << "\033[2J\033[H";
                
                // Print topic info
                std::cout << "=== " << topic_name_ << " History ===\n"
                         << "Total samples received: " << total_samples_ << "\n"
                         << "Current history size: " << history_.size() << "\n"
                         << "Display limit: " << display_limit_ << " samples\n\n";

                // Print table header
                std::cout << std::setw(6) << "Seq" 
                         << std::setw(10) << "Temp(°C)"
                         << std::setw(10) << "Hum(%)"
                         << std::setw(12) << "Press(hPa)"
                         << "  Time\n";
                std::cout << std::string(50, '-') << "\n";

                // 현재 모드에 따라 적절한 수의 샘플만 표시
                size_t start_idx = (history_.size() > display_limit_) ? 
                    history_.size() - display_limit_ : 0;
                
                for (size_t i = start_idx; i < history_.size(); ++i) {
                    const auto& sample = history_[i];
                    auto timestamp = std::chrono::system_clock::time_point(
                        std::chrono::nanoseconds(sample.timestamp()));
                    auto time_t = std::chrono::system_clock::to_time_t(timestamp);
                    
                    std::cout << std::setw(6) << sample.sequence_number()
                             << std::fixed << std::setprecision(1)
                             << std::setw(10) << sample.temperature()
                             << std::setw(10) << sample.humidity()
                             << std::setw(12) << sample.pressure()
                             << "  " << std::put_time(std::localtime(&time_t), "%H:%M:%S")
                             << std::endl;
                }
                std::cout.flush();
            }
        }
    }
};

class HistorySubscriber {
private:
    DomainParticipant* participant_;
    Subscriber* subscriber_;
    Topic* topic_;
    DataReader* reader_;
    TypeSupport type_;
    HistoryListener listener_;
    std::atomic<bool> running_;

    void setupKeepLastReader() {
        if (reader_ != nullptr) {
            subscriber_->delete_datareader(reader_);
        }

        DataReaderQos qos = DATAREADER_QOS_DEFAULT;
        qos.history().kind = KEEP_LAST_HISTORY_QOS;
        qos.history().depth = 5;
        qos.reliability().kind = RELIABLE_RELIABILITY_QOS;
        
        listener_.setDisplayLimit(5);
        reader_ = subscriber_->create_datareader(topic_, qos, &listener_);
        
        std::cout << "\nSwitched to KEEP_LAST mode (depth: 5)" << std::endl;
    }

    void setupKeepAllReader() {
        if (reader_ != nullptr) {
            subscriber_->delete_datareader(reader_);
        }

        DataReaderQos qos = DATAREADER_QOS_DEFAULT;
        qos.history().kind = KEEP_ALL_HISTORY_QOS;
        qos.resource_limits().max_samples = 30;
        qos.reliability().kind = RELIABLE_RELIABILITY_QOS;
        
        listener_.setDisplayLimit(30);
        reader_ = subscriber_->create_datareader(topic_, qos, &listener_);
        
        std::cout << "\nSwitched to KEEP_ALL mode (max samples: 30)" << std::endl;
    }
    
public:
    HistorySubscriber()
        : participant_(nullptr)
        , subscriber_(nullptr)
        , topic_(nullptr)
        , reader_(nullptr)
        , type_(new SensorDataPubSubType())
        , listener_("History QoS Test")
        , running_(true) {
    }

    ~HistorySubscriber() {
        if (subscriber_ != nullptr) {
            if (reader_ != nullptr)
                subscriber_->delete_datareader(reader_);
        }
        if (participant_ != nullptr) {
            if (topic_ != nullptr)
                participant_->delete_topic(topic_);
            if (subscriber_ != nullptr)
                participant_->delete_subscriber(subscriber_);
            DomainParticipantFactory::get_instance()->delete_participant(participant_);
        }
    }

    bool init() {
        DomainParticipantQos participantQos;
        participantQos.name("History_Subscriber");
        participant_ = DomainParticipantFactory::get_instance()->create_participant(0, participantQos);
        if (participant_ == nullptr) return false;

        type_.register_type(participant_);

        subscriber_ = participant_->create_subscriber(SUBSCRIBER_QOS_DEFAULT);
        if (subscriber_ == nullptr) return false;

        topic_ = participant_->create_topic("HistoryTopic", "SensorData", TOPIC_QOS_DEFAULT);
        if (topic_ == nullptr) return false;

        return true;
    }

    void run() {
        std::cout << "Select initial History QoS mode:\n"
                  << "1. KEEP_LAST mode (maintains last 5 samples)\n"
                  << "2. KEEP_ALL mode (maintains up to 30 samples)\n"
                  << "Enter mode (1 or 2): ";
        
        char mode;
        std::cin >> mode;
        std::cin.ignore();  // 버퍼 클리어

        // 초기 모드 설정
        if (mode == '1') {
            setupKeepLastReader();
        } else {
            setupKeepAllReader();
        }

        // 사용자 입력을 처리하는 스레드
        std::thread input_thread([this]() {
            std::cout << "\nSubscriber is running.\n"
                      << "Commands during runtime:\n"
                      << "1: Switch to KEEP_LAST mode\n"
                      << "2: Switch to KEEP_ALL mode\n"
                      << "q: Quit\n" << std::endl;

            char cmd;
            while (running_) {
                if (std::cin.get(cmd)) {
                    if (cmd == '1') {
                        setupKeepLastReader();
                    }
                    else if (cmd == '2') {
                        setupKeepAllReader();
                    }
                    else if (cmd == 'q') {
                        running_ = false;
                        break;
                    }
                }
            }
        });

        // 메인 스레드는 running_ 플래그가 false가 될 때까지 대기
        while (running_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        if (input_thread.joinable()) {
            input_thread.join();
        }
    }
};

int main(int argc, char** argv) {
    try {
        HistorySubscriber subscriber;
        if (subscriber.init()) {
            subscriber.run();
        }
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}