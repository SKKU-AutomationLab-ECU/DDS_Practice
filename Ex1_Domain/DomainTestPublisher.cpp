#include "DomainTest.h"
#include "DomainTestPubSubTypes.h"

#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/DataWriterListener.hpp>

#include <thread>
#include <chrono>
#include <iostream>

using namespace eprosima::fastdds::dds;

class DomainTestPublisher {
private:
    DomainTest message_;
    DomainParticipant* participant_;
    Publisher* publisher_;
    Topic* topic_;
    DataWriter* writer_;
    TypeSupport type_;
    uint32_t domain_id_;

public:
    DomainTestPublisher(uint32_t domain_id) 
        : participant_(nullptr)
        , publisher_(nullptr)
        , topic_(nullptr)
        , writer_(nullptr)
        , type_(new DomainTestPubSubType())
        , domain_id_(domain_id) {
    }

    bool init() {
        // Create participant with specified domain ID
        DomainParticipantQos participantQos;
        participantQos.name("DomainTest_Publisher");

        participant_ = DomainParticipantFactory::get_instance()->create_participant(domain_id_, participantQos);
        if (participant_ == nullptr) return false;

        // Register type
        type_.register_type(participant_);

        // Create publisher
        publisher_ = participant_->create_publisher(PUBLISHER_QOS_DEFAULT);
        if (publisher_ == nullptr) return false;

        // Create topic
        topic_ = participant_->create_topic("DomainTestTopic", "DomainTest", TOPIC_QOS_DEFAULT);
        if (topic_ == nullptr) return false;

        // Create datawriter
        writer_ = publisher_->create_datawriter(topic_, DATAWRITER_QOS_DEFAULT);
        if (writer_ == nullptr) return false;

        std::cout << "Publisher initialized on domain ID: " << domain_id_ << std::endl;
        return true;
    }

    bool publish() {
        static uint32_t index = 0;
        message_.index(index);
        message_.message("Domain " + std::to_string(domain_id_) + " Test Counter: " + std::to_string(index));
        writer_->write(&message_);
        index++;
        return true;
    }

    void run() {
        while (true) {
            publish();
            std::this_thread::sleep_for(std::chrono::seconds(1));
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

    DomainTestPublisher* publisher = new DomainTestPublisher(domain_id);
    if (publisher->init()) {
        publisher->run();
    }
    return 0;
}