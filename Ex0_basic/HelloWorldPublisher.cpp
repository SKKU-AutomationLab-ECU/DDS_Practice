#include "HelloWorld.h"
#include "HelloWorldPubSubTypes.h"

#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/DataWriterListener.hpp>

#include <thread>
#include <chrono>

using namespace eprosima::fastdds::dds;

class HelloWorldPublisher {
private:
    HelloWorld hello_;
    DomainParticipant* participant_;
    Publisher* publisher_;
    Topic* topic_;
    DataWriter* writer_;
    TypeSupport type_;

public:
    HelloWorldPublisher() : participant_(nullptr), publisher_(nullptr), topic_(nullptr), writer_(nullptr), 
                           type_(new HelloWorldPubSubType()) {
    }

    bool init() {
        // Create participant
        DomainParticipantQos participantQos;
        participantQos.name("HelloWorld_Publisher");

        participant_ = DomainParticipantFactory::get_instance()->create_participant(0, participantQos);
        if (participant_ == nullptr) return false;

        // Register type
        type_.register_type(participant_);

        // Create publisher
        publisher_ = participant_->create_publisher(PUBLISHER_QOS_DEFAULT);
        if (publisher_ == nullptr) return false;

        // Create topic
        topic_ = participant_->create_topic("HelloWorldTopic", "HelloWorld", TOPIC_QOS_DEFAULT);
        if (topic_ == nullptr) return false;

        // Create datawriter
        writer_ = publisher_->create_datawriter(topic_, DATAWRITER_QOS_DEFAULT);
        if (writer_ == nullptr) return false;

        return true;
    }

    bool publish() {
        static uint32_t index = 0;
        hello_.index(index);
        hello_.message("Pub/sub Test Counter: " + std::to_string(index));
        writer_->write(&hello_);
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

int main() {
    HelloWorldPublisher* publisher = new HelloWorldPublisher();
    if (publisher->init()) {
        publisher->run();
    }
    return 0;
}