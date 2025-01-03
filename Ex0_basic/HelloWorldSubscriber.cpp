#include "HelloWorld.h"
#include "HelloWorldPubSubTypes.h"

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

using namespace eprosima::fastdds::dds;

class HelloWorldSubscriber {
private:
    DomainParticipant* participant_;
    Subscriber* subscriber_;
    Topic* topic_;
    DataReader* reader_;
    TypeSupport type_;

    class SubListener : public DataReaderListener {
    public:
        SubListener() = default;

        void on_data_available(DataReader* reader) override {
            HelloWorld sample;
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
    HelloWorldSubscriber() : participant_(nullptr), subscriber_(nullptr), topic_(nullptr), reader_(nullptr),
                            type_(new HelloWorldPubSubType()) {
    }

    bool init() {
        // Create participant
        DomainParticipantQos participantQos;
        participantQos.name("HelloWorld_Subscriber");
        participant_ = DomainParticipantFactory::get_instance()->create_participant(0, participantQos);
        if (participant_ == nullptr) return false;

        // Register type
        type_.register_type(participant_);

        // Create subscriber
        subscriber_ = participant_->create_subscriber(SUBSCRIBER_QOS_DEFAULT);
        if (subscriber_ == nullptr) return false;

        // Create topic
        topic_ = participant_->create_topic("HelloWorldTopic", "HelloWorld", TOPIC_QOS_DEFAULT);
        if (topic_ == nullptr) return false;

        // Create datareader
        reader_ = subscriber_->create_datareader(topic_, DATAREADER_QOS_DEFAULT, &listener_);
        if (reader_ == nullptr) return false;

        return true; 
    }

    void run() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
};

int main() {
    HelloWorldSubscriber* subscriber = new HelloWorldSubscriber();
    if (subscriber->init()) {
        subscriber->run();
    }
    return 0;
}