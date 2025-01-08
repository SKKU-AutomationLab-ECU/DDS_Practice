#include "ReliabilityTest.h"
#include "ReliabilityTestPubSubTypes.h"

#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/DataReaderListener.hpp>
#include <fastdds/dds/subscriber/qos/DataReaderQos.hpp>
#include <fastdds/dds/subscriber/SampleInfo.hpp>

#include <map>
#include <set>
#include <mutex>
#include <iostream>

using namespace eprosima::fastdds::dds;

class ReliabilityListener : public DataReaderListener {
private:
    std::string topic_name_;
    std::map<uint32_t, bool> received_sequences_;
    std::set<uint32_t> missing_sequences_;
    std::mutex mutex_;
    uint32_t last_continuous_seq_;

public:
    ReliabilityListener(const std::string& topic_name) 
        : topic_name_(topic_name)
        , last_continuous_seq_(0) {
    }

    void on_data_available(DataReader* reader) override {
        TestData data;
        SampleInfo info;
        while (reader->take_next_sample(&data, &info) == ReturnCode_t::RETCODE_OK) {
            if (info.valid_data) {
                std::lock_guard<std::mutex> lock(mutex_);
                uint32_t seq = data.sequence_number();
                received_sequences_[seq] = data.is_critical();

                // 첫 메시지인 경우 초기화
                if (received_sequences_.size() == 1) {
                    last_continuous_seq_ = seq - 1;  // 현재 시퀀스 이전부터 시작
                }

                // Update continuous sequence
                while (received_sequences_.find(last_continuous_seq_ + 1) != received_sequences_.end()) {
                    last_continuous_seq_++;
                }

                // Find missing sequences
                missing_sequences_.clear();
                if (!received_sequences_.empty()) {
                    uint32_t first_seq = received_sequences_.begin()->first;
                    uint32_t last_seq = received_sequences_.rbegin()->first;
                    for (uint32_t i = first_seq; i <= last_seq; i++) {
                        if (received_sequences_.find(i) == received_sequences_.end()) {
                            missing_sequences_.insert(i);
                        }
                    }
                }

                // Print status
                std::cout << "\n=== " << topic_name_ << " Status ===\n"
                         << "Received message #" << seq 
                         << (data.is_critical() ? " (CRITICAL)" : "") << "\n"
                         << "Total messages received: " << received_sequences_.size() << "\n"
                         << "Missing sequences: ";

                if (missing_sequences_.empty()) {
                    std::cout << "None";
                } else {
                    for (auto missing : missing_sequences_) {
                        std::cout << missing << " ";
                    }
                }
                std::cout << std::endl;

                // Print critical messages status
                std::cout << "Critical messages status:" << std::endl;
                for (const auto& pair : received_sequences_) {
                    if (pair.second) {
                        std::cout << "Critical message #" << pair.first << " received" << std::endl;
                    }
                }
            }
        }
    }
};

class ReliabilitySubscriber {
private:
    DomainParticipant* participant_;
    Subscriber* subscriber_;
    Topic* reliable_topic_;
    Topic* best_effort_topic_;
    DataReader* reliable_reader_;
    DataReader* best_effort_reader_;
    TypeSupport type_;
    ReliabilityListener reliable_listener_;
    ReliabilityListener best_effort_listener_;

public:
    ReliabilitySubscriber()
        : participant_(nullptr)
        , subscriber_(nullptr)
        , reliable_topic_(nullptr)
        , best_effort_topic_(nullptr)
        , reliable_reader_(nullptr)
        , best_effort_reader_(nullptr)
        , type_(new TestDataPubSubType())
        , reliable_listener_("RELIABLE")
        , best_effort_listener_("BEST_EFFORT") {
    }

    ~ReliabilitySubscriber() {
        if (subscriber_ != nullptr) {
            if (reliable_reader_ != nullptr)
                subscriber_->delete_datareader(reliable_reader_);
            if (best_effort_reader_ != nullptr)
                subscriber_->delete_datareader(best_effort_reader_);
        }
        if (participant_ != nullptr) {
            if (reliable_topic_ != nullptr)
                participant_->delete_topic(reliable_topic_);
            if (best_effort_topic_ != nullptr)
                participant_->delete_topic(best_effort_topic_);
            if (subscriber_ != nullptr)
                participant_->delete_subscriber(subscriber_);
            DomainParticipantFactory::get_instance()->delete_participant(participant_);
        }
    }

    bool init() {
        DomainParticipantQos participantQos;
        participantQos.name("Reliability_Subscriber");
        participant_ = DomainParticipantFactory::get_instance()->create_participant(0, participantQos);
        if (participant_ == nullptr) return false;

        type_.register_type(participant_);

        subscriber_ = participant_->create_subscriber(SUBSCRIBER_QOS_DEFAULT);
        if (subscriber_ == nullptr) return false;

        // Create topics
        reliable_topic_ = participant_->create_topic("ReliableTopic", "TestData", TOPIC_QOS_DEFAULT);
        best_effort_topic_ = participant_->create_topic("BestEffortTopic", "TestData", TOPIC_QOS_DEFAULT);

        if (reliable_topic_ == nullptr || best_effort_topic_ == nullptr) return false;

        // Configure RELIABLE QoS
        DataReaderQos reliable_qos = DATAREADER_QOS_DEFAULT;
        reliable_qos.reliability().kind = RELIABLE_RELIABILITY_QOS;
        reliable_qos.history().kind = KEEP_ALL_HISTORY_QOS;
        reliable_reader_ = subscriber_->create_datareader(reliable_topic_, reliable_qos, &reliable_listener_);

        // Configure BEST_EFFORT QoS
        DataReaderQos best_effort_qos = DATAREADER_QOS_DEFAULT;
        best_effort_qos.reliability().kind = BEST_EFFORT_RELIABILITY_QOS;
        best_effort_qos.history().kind = KEEP_ALL_HISTORY_QOS;
        best_effort_reader_ = subscriber_->create_datareader(best_effort_topic_, best_effort_qos, &best_effort_listener_);

        if (reliable_reader_ == nullptr || best_effort_reader_ == nullptr) return false;

        return true;
    }

    void run() {
        std::cout << "Subscriber is running. Press Enter to stop." << std::endl;
        std::cin.ignore();
        std::cin.get();
    }
};

int main() {
    try {
        ReliabilitySubscriber subscriber;
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