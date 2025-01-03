#include "SteeringControl.h"
#include "SteeringControlPubSubTypes.h"

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
#include <random>
#include <atomic>
#include <signal.h>

using namespace eprosima::fastdds::dds;

enum class ControllerType {
    MANUAL,     // 기본 수동 조향
    ADAS,       // ADAS 시스템
    EMERGENCY   // 긴급 제어 시스템
};

class SteeringPublisher {
private:
    DomainParticipant* participant_;
    Publisher* publisher_;
    Topic* topic_;
    DataWriter* writer_;
    TypeSupport type_;
    SteeringCommand command_;
    std::atomic<bool> running_;
    ControllerType controller_type_;
    uint32_t ownership_strength_;
    std::random_device rd_;
    std::mt19937 gen_;
    std::uniform_real_distribution<> angle_dist_;
    std::uniform_real_distribution<> speed_dist_;

public:
    SteeringPublisher(ControllerType type)
        : participant_(nullptr)
        , publisher_(nullptr)
        , topic_(nullptr)
        , writer_(nullptr)
        , type_(new SteeringCommandPubSubType())
        , running_(true)
        , controller_type_(type)
        , gen_(rd_())
        , angle_dist_(-30.0, 30.0)     // ±30도 범위
        , speed_dist_(0.0, 120.0) {    // 0~120 km/h
        
        // 컨트롤러 타입별 설정
        switch(type) {
            case ControllerType::MANUAL:
                ownership_strength_ = 10;
                command_.controller_name("Manual Steering");
                break;
            case ControllerType::ADAS:
                ownership_strength_ = 20;
                command_.controller_name("ADAS Controller");
                break;
            case ControllerType::EMERGENCY:
                ownership_strength_ = 30;
                command_.controller_name("Emergency Controller");
                break;
        }
    }

    bool init() {
        // Participant 설정
        DomainParticipantQos participantQos;
        participantQos.name("Steering_Publisher_" + command_.controller_name());
        participant_ = DomainParticipantFactory::get_instance()->create_participant(0, participantQos);
        if (participant_ == nullptr) return false;

        // Type 등록
        type_.register_type(participant_);

        // Publisher 생성
        publisher_ = participant_->create_publisher(PUBLISHER_QOS_DEFAULT);
        if (publisher_ == nullptr) return false;

        // 단일 토픽 생성
        topic_ = participant_->create_topic(
            "SteeringControl",  // 모든 컨트롤러가 같은 토픽 사용
            "SteeringCommand", 
            TOPIC_QOS_DEFAULT);
        if (topic_ == nullptr) return false;

        // DataWriter QoS 설정
        DataWriterQos writerQos = DATAWRITER_QOS_DEFAULT;
        writerQos.reliability().kind = RELIABLE_RELIABILITY_QOS;
        writerQos.ownership().kind = EXCLUSIVE_OWNERSHIP_QOS;
        writerQos.ownership_strength().value = ownership_strength_;

        writer_ = publisher_->create_datawriter(topic_, writerQos);
        if (writer_ == nullptr) return false;

        return true;
    }

    void generateCommand() {
        command_.timestamp(std::chrono::system_clock::now().time_since_epoch().count());
        command_.steering_angle(angle_dist_(gen_));
        command_.vehicle_speed(speed_dist_(gen_));
        command_.steering_torque(0.0);  // 실제로는 토크 계산 필요
        
        switch(controller_type_) {
            case ControllerType::MANUAL:
                command_.control_reason("Regular driving");
                command_.emergency_control(false);
                break;
            case ControllerType::ADAS:
                command_.control_reason("Lane keeping assist");
                command_.emergency_control(false);
                break;
            case ControllerType::EMERGENCY:
                command_.control_reason("Collision avoidance");
                command_.emergency_control(true);
                break;
        }
    }

    void publish() {
        generateCommand();
        writer_->write(&command_);

        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        
        std::cout << std::put_time(std::localtime(&time_t), "%H:%M:%S") 
                  << " [" << command_.controller_name() << "] Published:"
                  << " Angle=" << std::fixed << std::setprecision(1) 
                  << command_.steering_angle() << "°"
                  << " Speed=" << command_.vehicle_speed() << "km/h"
                  << " Reason: " << command_.control_reason()
                  << " (Strength: " << ownership_strength_ << ")"
                  << std::endl;
    }

    void run() {
        std::cout << "Publisher started: " << command_.controller_name() << "\n"
                  << "Press Ctrl+C to stop." << std::endl;

        while (running_) {
            publish();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    void stop() {
        running_ = false;
    }
};

void signal_handler(int signum) {
    std::cout << "\nStopping publisher..." << std::endl;
    exit(signum);
}

int main(int argc, char** argv) {
    signal(SIGINT, signal_handler);

    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <controller_type>\n"
                  << "  1: Manual Steering\n"
                  << "  2: ADAS Controller\n"
                  << "  3: Emergency Controller\n";
        return 1;
    }

    ControllerType type;
    switch(std::stoi(argv[1])) {
        case 1: type = ControllerType::MANUAL; break;
        case 2: type = ControllerType::ADAS; break;
        case 3: type = ControllerType::EMERGENCY; break;
        default:
            std::cout << "Invalid controller type\n";
            return 1;
    }

    try {
        SteeringPublisher publisher(type);
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