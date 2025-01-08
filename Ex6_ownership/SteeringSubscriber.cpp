#include "SteeringControl.h"
#include "SteeringControlPubSubTypes.h"

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
#include <atomic>
#include <memory>
#include <set>

using namespace eprosima::fastdds::dds;

static std::mutex print_mutex;

class SteeringListener : public DataReaderListener {
private:
    std::string controller_type_;
    int received_count_;
    std::mutex& print_mutex_;
    std::set<uint32_t>& active_strengths_;  // 현재 active한 controller들의 strength set
    uint32_t strength_;

public:
    SteeringListener(const std::string& controller_type, std::mutex& mutex, 
                    std::set<uint32_t>& active_strengths, uint32_t strength)
        : controller_type_(controller_type)
        , received_count_(0)
        , print_mutex_(mutex)
        , active_strengths_(active_strengths)
        , strength_(strength) {
    }

    void on_data_available(DataReader* reader) override {
        SteeringCommand command;
        SampleInfo info;
        
        while (reader->take_next_sample(&command, &info) == ReturnCode_t::RETCODE_OK) {
            if (info.valid_data) {
                // active_strengths가 비어있으면 데이터를 처리하지 않음
                if (active_strengths_.empty()) {
                    continue;
                }

                uint32_t controller_strength = getStrengthFromName(command.controller_name());
                
                // 현재 controller가 active 상태인지 확인
                if (active_strengths_.count(controller_strength) > 0) {
                    // active strength 중에서 현재 controller보다 높은 strength를 가진 것이 있는지 확인
                    bool is_highest_active = true;
                    for (auto strength : active_strengths_) {
                        if (strength > controller_strength) {
                            // 더 높은 strength가 있더라도, 해당 controller가 현재 publishing 중인지는 알 수 없음
                            // 따라서 현재 들어온 데이터를 처리
                            is_highest_active = true;
                        }
                    }

                    if (is_highest_active) {
                        std::lock_guard<std::mutex> lock(print_mutex_);
                        received_count_++;

                        auto timestamp = std::chrono::system_clock::time_point(
                            std::chrono::nanoseconds(command.timestamp()));
                        auto time_t = std::chrono::system_clock::to_time_t(timestamp);

                        std::cout << "\033[2J\033[H";  // Clear screen

                        std::cout << "=== Active Controllers ===\n";
                        for (auto strength : active_strengths_) {
                            std::cout << getControllerName(strength) << " (Strength: " << strength << ")\n";
                        }
                        std::cout << "\n";

                        std::cout << "=== Current Controller (" << command.controller_name() 
                                 << ", Strength: " << controller_strength << ") ===\n"
                                << "Time: " << std::put_time(std::localtime(&time_t), "%H:%M:%S") << "\n"
                                << "Steering Angle: " << std::fixed << std::setprecision(1) 
                                << command.steering_angle() << "°\n"
                                << "Vehicle Speed: " << command.vehicle_speed() << " km/h\n"
                                << "Control Reason: " << command.control_reason() << "\n"
                                << "Emergency Control: " << (command.emergency_control() ? "YES" : "No") << "\n"
                                << "Total messages received: " << received_count_ << "\n\n";

                        std::cout << "Enter command (1-3, s, q): ";
                        std::cout.flush();
                    }
                }
            }
        }
    }

private:
    uint32_t getStrengthFromName(const std::string& controller_name) {
        if (controller_name == "Manual Steering") return 10;
        if (controller_name == "ADAS Controller") return 20;
        if (controller_name == "Emergency Controller") return 30;
        return 0;
    }

    std::string getControllerName(uint32_t strength) {
        switch (strength) {
            case 10: return "Manual Control";
            case 20: return "ADAS Control";
            case 30: return "Emergency Control";
            default: return "Unknown";
        }
    }
};

class SteeringSubscriber {
private:
    DomainParticipant* participant_;
    Subscriber* subscriber_;
    Topic* topic_;
    DataReader* reader_;
    TypeSupport type_;
    std::unique_ptr<SteeringListener> listener_;
    std::atomic<bool> running_;
    std::set<uint32_t> active_strengths_;  // 현재 active한 controller들의 strength set

    struct ControllerInfo {
        std::string type;
        bool active;
        uint32_t strength;

        ControllerInfo(const std::string& t = "", uint32_t s = 0)
            : type(t), active(false), strength(s) {}
    };
    std::map<int, ControllerInfo> controllers_;

    void toggleController(int id) {
        auto& info = controllers_[id];
        info.active = !info.active;

        if (info.active) {
            active_strengths_.insert(info.strength);
            std::cout << "\nEnabled " << info.type 
                     << " (Strength: " << info.strength << ")" << std::endl;
        } else {
            active_strengths_.erase(info.strength);
            std::cout << "\nDisabled " << info.type 
                     << " (Strength: " << info.strength << ")" << std::endl;
        }
    }

public:
    SteeringSubscriber()
        : participant_(nullptr)
        , subscriber_(nullptr)
        , topic_(nullptr)
        , reader_(nullptr)
        , type_(new SteeringCommandPubSubType())
        , running_(true) {
        
        // 컨트롤러 정보 초기화
        controllers_[1] = ControllerInfo("Manual Control", 10);
        controllers_[2] = ControllerInfo("ADAS Control", 20);
        controllers_[3] = ControllerInfo("Emergency Control", 30);
    }

    ~SteeringSubscriber() {
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

    bool init() {
        DomainParticipantQos participantQos;
        participantQos.name("Steering_Subscriber");
        participant_ = DomainParticipantFactory::get_instance()->create_participant(0, participantQos);
        if (participant_ == nullptr) return false;

        type_.register_type(participant_);

        topic_ = participant_->create_topic(
            "SteeringControl",
            "SteeringCommand",
            TOPIC_QOS_DEFAULT);
        if (topic_ == nullptr) return false;

        subscriber_ = participant_->create_subscriber(SUBSCRIBER_QOS_DEFAULT);
        if (subscriber_ == nullptr) return false;

        DataReaderQos readerQos = DATAREADER_QOS_DEFAULT;
        readerQos.reliability().kind = RELIABLE_RELIABILITY_QOS;
        readerQos.ownership().kind = EXCLUSIVE_OWNERSHIP_QOS;

        // 기본 Manual control은 active로 설정
        active_strengths_.insert(controllers_[1].strength);
        controllers_[1].active = true;

        listener_.reset(new SteeringListener("SteeringControl", print_mutex, active_strengths_, 0));
        reader_ = subscriber_->create_datareader(
            topic_,
            readerQos,
            listener_.get());
        if (reader_ == nullptr) return false;

        return true;
    }

    void showStatus() {
        std::cout << "\nCurrent subscriptions:\n";
        for (const auto& pair : controllers_) {
            std::cout << pair.second.type << ": "
                     << (pair.second.active ? "Active" : "Inactive")
                     << " (Strength: " << pair.second.strength << ")"
                     << std::endl;
        }
    }

    void run() {
        std::cout << "\nAvailable commands:\n"
                << "1: Toggle Manual Control subscription\n"
                << "2: Toggle ADAS Control subscription\n"
                << "3: Toggle Emergency Control subscription\n"
                << "s: Show current subscriptions\n"
                << "q: Quit\n" << std::endl;

        showStatus();

        std::thread input_thread([this]() {
            char cmd;
            while (running_) {
                std::cin >> cmd;
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                
                if (cmd >= '1' && cmd <= '3') {
                    int id = cmd - '0';
                    toggleController(id);
                    showStatus();
                }
                else if (cmd == 's') {
                    showStatus();
                }
                else if (cmd == 'q') {
                    running_ = false;
                }

                std::cout << "\nEnter command (1-3, s, q): ";
            }
        });

        while (running_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        if (input_thread.joinable()) {
            input_thread.join();
        }
    }
};

int main() {
    try {
        SteeringSubscriber subscriber;
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