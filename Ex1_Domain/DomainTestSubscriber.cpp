#include "DomainTest.h"
#include "DomainTestPubSubTypes.h"

#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/DataReaderListener.hpp>
#include <fastdds/dds/subscriber/qos/DataReaderQos.hpp>
#include <fastdds/dds/subscriber/SampleInfo.hpp>

#include <thread>
#include <chrono>
#include <iostream>

using namespace eprosima::fastdds::dds;

class DomainTestSubscriber {
private:
    DomainParticipant* participant_;
    Subscriber* subscriber_;
    Topic* topic_;
    DataReader* reader_;
    TypeSupport type_;
    uint32_t domain_id_;

    class SubListener : public DataReaderListener {
    public:
        SubListener() = default;

        void on_data_available(DataReader* reader) override {
            DomainTest sample;
            SampleInfo info;
            if (reader->take_next_sample(&sample, &info) == ReturnCode_t::RETCODE_OK) {
                if (info.valid_data) {
                    std::cout << "Message received: " << sample.message() << std::endl;
                    std::cout << "Index: " << sample.index() << std::endl;
                }
            }
        }
    } listener_;

public:
    DomainTestSubscriber(uint32_t domain_id) 
        : participant_(nullptr)
        , subscriber_(nullptr)
        , topic_(nullptr)
        , reader_(nullptr)
        , type_(new DomainTestPubSubType())
        , domain_id_(domain_id) {
    }

    bool init() {
        // Create participant with specified domain ID
        DomainParticipantQos participantQos;
        participantQos.name("DomainTest_Subscriber");
        participant_ = DomainParticipantFactory::get_instance()->create_participant(domain_id_, participantQos);
        if (participant_ == nullptr) return false;

        // Register type
        type_.register_type(participant_);

        // Create subscriber
        subscriber_ = participant_->create_subscriber(SUBSCRIBER_QOS_DEFAULT);
        if (subscriber_ == nullptr) return false;

        // Create topic
        topic_ = participant_->create_topic("DomainTestTopic", "DomainTest", TOPIC_QOS_DEFAULT);
        if (topic_ == nullptr) return false;

        // Create datareader
        reader_ = subscriber_->create_datareader(topic_, DATAREADER_QOS_DEFAULT, &listener_);
        if (reader_ == nullptr) return false;

        std::cout << "Subscriber initialized on domain ID: " << domain_id_ << std::endl;
        return true; 
    }

    void run() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
};

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <domain_id>" << std::endl;
        std::cout << "domain_id must be a single digit (0-9)" << std::endl;
        return 1;
    }

    // Parse domain ID
    int domain_id = argv[1][0] - '0';
    if (domain_id < 0 || domain_id > 9) {
        std::cout << "Error: domain_id must be a single digit (0-9)" << std::endl;
        return 1;
    }

    DomainTestSubscriber* subscriber = new DomainTestSubscriber(domain_id);
    if (subscriber->init()) {
        subscriber->run();
    }
    return 0;
}