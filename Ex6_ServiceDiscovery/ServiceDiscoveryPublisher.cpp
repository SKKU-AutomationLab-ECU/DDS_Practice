#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/qos/DataWriterQos.hpp>

#include <chrono>
#include <thread>
#include <iostream>
#include <iomanip>
#include <atomic>
#include <signal.h>
#include "ServiceDiscovery.h"
#include "ServiceDiscoveryPubSubTypes.h"

using namespace eprosima::fastdds::dds;

class ServiceDiscoveryPublisher {
private:
    std::vector<std::string> getCapabilities(const std::string& service_type) {
        std::vector<std::string> capabilities;
        
        // 공통 기능
        capabilities.push_back("health_check");
        capabilities.push_back("basic_discovery");
        
        // 서비스 타입별 특화 기능
        if (service_type == "REST") {
            capabilities.push_back("CRUD");
            capabilities.push_back("JSON");
            capabilities.push_back("HTTP/1.1");
            capabilities.push_back("RESTful");
        } 
        else if (service_type == "gRPC") {
            capabilities.push_back("streaming");
            capabilities.push_back("protobuf");
            capabilities.push_back("HTTP/2");
            capabilities.push_back("bidirectional");
        }
        else if (service_type == "WebSocket") {
            capabilities.push_back("real-time");
            capabilities.push_back("bi-directional");
            capabilities.push_back("persistent-connection");
            capabilities.push_back("push-notifications");
        }
        else if (service_type == "GraphQL") {
            capabilities.push_back("query");
            capabilities.push_back("mutation");
            capabilities.push_back("subscription");
            capabilities.push_back("schema-introspection");
        }
        else if (service_type == "SOAP") {
            capabilities.push_back("XML");
            capabilities.push_back("WSDL");
            capabilities.push_back("enterprise");
            capabilities.push_back("security");
        }
        else {
            capabilities.push_back("custom-protocol");
            capabilities.push_back("extensible");
        }
        
        return capabilities;
    }


    DomainParticipant* participant_;
    Publisher* publisher_;
    Topic* topic_;
    DataWriter* writer_;
    TypeSupport type_;
    ServiceInfo service_info_;
    std::atomic<bool> running_;
    std::string service_type_;

public:
    ServiceDiscoveryPublisher(const std::string& service_name, 
                            const std::string& service_type,
                            const std::string& endpoint,
                            unsigned long port)
        : participant_(nullptr)
        , publisher_(nullptr)
        , topic_(nullptr)
        , writer_(nullptr)
        , type_(new ServiceInfoPubSubType())
        , running_(true)
        , service_type_(service_type) {
        
        // 서비스 정보 초기화
        service_info_.service_name(service_name);
        service_info_.service_type(service_type);
        service_info_.endpoint(endpoint);
        service_info_.port(port);
        service_info_.is_healthy(true);
        
        // 서비스 타입별 기능 목록 설정
        std::vector<std::string> capabilities = getCapabilities(service_type);
        service_info_.capabilities(capabilities);
    }

    bool init() {
        // Participant 설정
        DomainParticipantQos participantQos;
        participantQos.name("ServiceDiscovery_Publisher_" + service_info_.service_name());
        
        // SIMPLE 프로토콜로 설정
        participantQos.wire_protocol().builtin.discovery_config.discoveryProtocol = 
            eprosima::fastrtps::rtps::DiscoveryProtocol_t::SIMPLE;
        
        // Lease Duration 설정
        participantQos.wire_protocol().builtin.discovery_config.leaseDuration_announcementperiod =
            eprosima::fastrtps::Duration_t(5, 0);

        participant_ = DomainParticipantFactory::get_instance()->create_participant(0, participantQos);
        if (participant_ == nullptr) {
            std::cerr << "Failed to create participant" << std::endl;
            return false;
        }

        // Type 등록
        type_.register_type(participant_);

        // Topic 생성
        topic_ = participant_->create_topic(
            "ServiceDiscovery",
            "ServiceInfo",
            TOPIC_QOS_DEFAULT);
        if (topic_ == nullptr) {
            std::cerr << "Failed to create topic" << std::endl;
            return false;
        }

        // Publisher 생성
        publisher_ = participant_->create_publisher(PUBLISHER_QOS_DEFAULT);
        if (publisher_ == nullptr) {
            std::cerr << "Failed to create publisher" << std::endl;
            return false;
        }

        // DataWriter QoS 설정
        DataWriterQos writerQos = DATAWRITER_QOS_DEFAULT;
        writerQos.reliability().kind = RELIABLE_RELIABILITY_QOS;
        writerQos.durability().kind = TRANSIENT_LOCAL_DURABILITY_QOS;

        writer_ = publisher_->create_datawriter(topic_, writerQos);
        if (writer_ == nullptr) {
            std::cerr << "Failed to create writer" << std::endl;
            return false;
        }

        return true;
    }

    void publish() {
        service_info_.timestamp(std::chrono::system_clock::now().time_since_epoch().count());
        
        // 랜덤하게 상태 변경 (데모용)
        static int count = 0;
        count++;
        if (count % 10 == 0) {
            service_info_.status("BUSY");
            service_info_.is_healthy(true);
        } else if (count % 15 == 0) {
            service_info_.status("ERROR");
            service_info_.is_healthy(false);
        } else {
            service_info_.status("ACTIVE");
            service_info_.is_healthy(true);
        }

        if (writer_->write(&service_info_)) {
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            
            std::cout << std::put_time(std::localtime(&time_t), "%H:%M:%S") 
                      << " [" << service_info_.service_name() << "] Published:"
                      << " Type=" << service_info_.service_type()
                      << " Status=" << service_info_.status()
                      << " Healthy=" << (service_info_.is_healthy() ? "Yes" : "No")
                      << std::endl;
        }
    }

    void run() {
        std::cout << "Service Discovery Publisher started: " << service_info_.service_name() 
                  << "\nEndpoint: " << service_info_.endpoint() << ":" << service_info_.port()
                  << "\nPress Ctrl+C to stop." << std::endl;

        while (running_) {
            publish();
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    void stop() {
        running_ = false;
    }
};

static ServiceDiscoveryPublisher* publisher_ptr = nullptr;

void signal_handler(int signum) {
    std::cout << "\nStopping publisher..." << std::endl;
    if (publisher_ptr != nullptr) {
        publisher_ptr->stop();
    }
}

int main(int argc, char** argv) {
    if (argc != 5) {
        std::cout << "Usage: " << argv[0] 
                  << " <service_name> <service_type> <endpoint> <port>\n"
                  << "Example: " << argv[0] 
                  << " UserService REST localhost 8080" << std::endl;
        return 1;
    }

    signal(SIGINT, signal_handler);

    try {
        ServiceDiscoveryPublisher publisher(argv[1], argv[2], argv[3], std::stoul(argv[4]));
        publisher_ptr = &publisher;

        if (publisher.init()) {
            publisher.run();
        }
        
        publisher_ptr = nullptr;
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}