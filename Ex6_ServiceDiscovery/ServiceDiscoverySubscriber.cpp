// ServiceDiscoverySubscriber.cpp
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/DataReaderListener.hpp>
#include <fastdds/dds/subscriber/qos/DataReaderQos.hpp>
#include <fastdds/dds/subscriber/SampleInfo.hpp>

#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <mutex>
#include <map>
#include "ServiceDiscovery.h"
#include "ServiceDiscoveryPubSubTypes.h"

using namespace eprosima::fastdds::dds;

static std::mutex console_mutex;

class ServiceListener : public DataReaderListener {
private:
    std::map<std::string, ServiceInfo>& services_;
    std::mutex& mutex_;

public:
    ServiceListener(std::map<std::string, ServiceInfo>& services, std::mutex& mutex)
        : services_(services), mutex_(mutex) {}

    void on_data_available(DataReader* reader) override {
        ServiceInfo info;
        SampleInfo sample_info;
        
        while (reader->take_next_sample(&info, &sample_info) == ReturnCode_t::RETCODE_OK) {
            if (sample_info.valid_data) {
                std::lock_guard<std::mutex> lock(mutex_);
                services_[info.service_name()] = info;
                
                // 화면 갱신
                updateDisplay();
            }
        }
    }

    void updateDisplay() {
        std::lock_guard<std::mutex> lock(console_mutex);
        
        // 화면 지우기
        std::cout << "\033[2J\033[H";
        
        // 현재 시간 표시
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::cout << "Service Discovery Monitor - " 
                  << std::put_time(std::localtime(&time_t), "%H:%M:%S") << "\n"
                  << "======================================\n\n";

        // 서비스 목록 표시
        if (services_.empty()) {
            std::cout << "No services discovered yet...\n";
        } else {
            for (const auto& pair : services_) {
                const auto& service = pair.second;
                
                // 상태에 따른 색상 설정
                std::string color;
                if (service.status() == "ERROR") {
                    color = "\033[31m"; // 빨간색
                } else if (service.status() == "BUSY") {
                    color = "\033[33m"; // 노란색
                } else {
                    color = "\033[32m"; // 초록색
                }
                
                std::cout << "Service: " << service.service_name() << "\n"
                          << "  Type: " << service.service_type() << "\n"
                          << "  Endpoint: " << service.endpoint() << ":" << service.port() << "\n"
                          << "  Status: " << color << service.status() << "\033[0m\n"
                          << "  Health: " << (service.is_healthy() ? "✓" : "✗") << "\n"
                          << "  Capabilities: ";
                
                // 기능 목록 표시
                const auto& capabilities = service.capabilities();
                for (size_t i = 0; i < capabilities.size(); ++i) {
                    std::cout << capabilities[i];
                    if (i < capabilities.size() - 1) std::cout << ", ";
                }
                std::cout << "\n\n";
            }
        }
        
        std::cout << "\nCommands:\n"
                  << "l: List all services\n"
                  << "c: Clear inactive services\n"
                  << "q: Quit\n\n"
                  << "Enter command: ";
        std::cout.flush();
    }
};

class ServiceDiscoverySubscriber {
private:
    DomainParticipant* participant_;
    Subscriber* subscriber_;
    Topic* topic_;
    DataReader* reader_;
    TypeSupport type_;
    std::unique_ptr<ServiceListener> listener_;
    std::atomic<bool> running_;
    std::map<std::string, ServiceInfo> discovered_services_;
    std::mutex services_mutex_;

public:
    ServiceDiscoverySubscriber()
        : participant_(nullptr)
        , subscriber_(nullptr)
        , topic_(nullptr)
        , reader_(nullptr)
        , type_(new ServiceInfoPubSubType())
        , running_(true) {
    }

    bool init() {
        // Participant 설정
        DomainParticipantQos participantQos;
        participantQos.name("ServiceDiscovery_Subscriber");
        
        // SIMPLE 프로토콜 설정
        participantQos.wire_protocol().builtin.discovery_config.discoveryProtocol = 
            eprosima::fastrtps::rtps::DiscoveryProtocol_t::SIMPLE;
        
        participant_ = DomainParticipantFactory::get_instance()->create_participant(0, participantQos);
        if (participant_ == nullptr) return false;

        // Type 등록
        type_.register_type(participant_);

        // Topic 생성
        topic_ = participant_->create_topic(
            "ServiceDiscovery",
            "ServiceInfo",
            TOPIC_QOS_DEFAULT);
        if (topic_ == nullptr) return false;

        // Subscriber 생성
        subscriber_ = participant_->create_subscriber(SUBSCRIBER_QOS_DEFAULT);
        if (subscriber_ == nullptr) return false;

        // DataReader QoS 설정
        DataReaderQos readerQos = DATAREADER_QOS_DEFAULT;
        readerQos.reliability().kind = RELIABLE_RELIABILITY_QOS;
        readerQos.durability().kind = TRANSIENT_LOCAL_DURABILITY_QOS;

        // Listener 생성 및 DataReader 설정
        listener_.reset(new ServiceListener(discovered_services_, services_mutex_));
        reader_ = subscriber_->create_datareader(topic_, readerQos, listener_.get());
        if (reader_ == nullptr) return false;

        return true;
    }

    void run() {
        std::cout << "\nService Discovery Monitor Started\n"
                  << "================================\n"
                  << "Available commands:\n"
                  << "l: List all services\n"
                  << "c: Clear inactive services\n"
                  << "q: Quit\n" << std::endl;

        // 사용자 입력을 처리하는 스레드
        std::thread input_thread([this]() {
            char cmd;
            while (running_) {
                std::cin >> cmd;
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

                switch (cmd) {
                    case 'l':
                    case 'L': {
                        std::lock_guard<std::mutex> lock(services_mutex_);
                        listener_->updateDisplay();
                        break;
                    }
                    case 'c':
                    case 'C': {
                        std::lock_guard<std::mutex> lock(services_mutex_);
                        auto now = std::chrono::system_clock::now();
                        auto now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                            now.time_since_epoch()).count();

                        // 10초 이상 업데이트가 없는 서비스 제거
                        for (auto it = discovered_services_.begin(); it != discovered_services_.end();) {
                            if (now_ns - it->second.timestamp() > 10000000000) {
                                std::cout << "\nRemoving inactive service: " << it->first << std::endl;
                                it = discovered_services_.erase(it);
                            } else {
                                ++it;
                            }
                        }
                        listener_->updateDisplay();
                        break;
                    }
                    case 'q':
                    case 'Q':
                        running_ = false;
                        break;
                    default:
                        std::cout << "\nInvalid command. Please try again." << std::endl;
                        break;
                }
            }
        });

        // 메인 스레드는 running_ 상태 체크
        while (running_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // 정리
        if (input_thread.joinable()) {
            input_thread.join();
        }
    }

    ~ServiceDiscoverySubscriber() {
        if (participant_ != nullptr) {
            if (subscriber_ != nullptr) {
                if (reader_ != nullptr) {
                    subscriber_->delete_datareader(reader_);
                }
                participant_->delete_subscriber(subscriber_);
            }
            if (topic_ != nullptr) {
                participant_->delete_topic(topic_);
            }
            DomainParticipantFactory::get_instance()->delete_participant(participant_);
        }
    }
};

int main() {
    try {
        ServiceDiscoverySubscriber subscriber;
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