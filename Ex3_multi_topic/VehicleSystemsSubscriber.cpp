#include "VehicleSystems.h"
#include "VehicleSystemsPubSubTypes.h"

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
#include <iomanip>
#include <map>

using namespace eprosima::fastdds::dds;

class VehicleSystemsSubscriber {
private:
    // DDS Entities
    DomainParticipant* participant_;
    Subscriber* subscriber_;

    struct TopicReader {
        Topic* topic;
        DataReader* reader;
        TypeSupport type;
    };

    std::map<std::string, TopicReader> topic_readers_;

    // Listeners for each system
    class PowertrainListener : public DataReaderListener {
    public:
        void on_data_available(DataReader* reader) override {
            PowertrainData data;
            SampleInfo info;
            while (reader->take_next_sample(&data, &info) == ReturnCode_t::RETCODE_OK) {
                if (info.valid_data) {
                    std::cout << "\033[2J\033[H";  // Clear screen
                    std::cout << "=== Powertrain Data ===\n";
                    std::cout << "Engine RPM: " << data.engine_rpm() << "\n";
                    std::cout << "Engine Temperature: " << data.engine_temperature() << "°C\n";
                    std::cout << "Engine Load: " << data.engine_load() << "%\n";
                    std::cout << "Transmission Temperature: " << data.transmission_temp() << "°C\n";
                    std::cout << "Current Gear: " << data.current_gear() << "\n";
                    if (!data.dtc_codes().empty()) {
                        std::cout << "DTC Codes:\n";
                        for (const auto& code : data.dtc_codes()) {
                            std::cout << "  " << code << "\n";
                        }
                    }
                }
            }
        }
    } powertrain_listener_;

    class ChassisListener : public DataReaderListener {
    public:
        void on_data_available(DataReader* reader) override {
            ChassisData data;
            SampleInfo info;
            while (reader->take_next_sample(&data, &info) == ReturnCode_t::RETCODE_OK) {
                if (info.valid_data) {
                    std::cout << "\n=== Chassis Data ===\n";
                    std::cout << "Brake Pressure: " << data.brake_pressure() << " bar\n";
                    std::cout << "Steering Angle: " << data.steering_angle() << "°\n";
                    std::cout << "Suspension Height (FL,FR,RL,RR): ";
                    for (int i = 0; i < 4; i++) {
                        std::cout << data.suspension_height()[i] << "mm ";
                    }
                    std::cout << "\nWheel Speed (FL,FR,RL,RR): ";
                    for (int i = 0; i < 4; i++) {
                        std::cout << data.wheel_speed()[i] << "km/h ";
                    }
                    std::cout << "\nABS Active: " << (data.abs_active() ? "YES" : "NO") << "\n";
                    std::cout << "Traction Control: " << (data.traction_control_active() ? "ON" : "OFF") << "\n";
                }
            }
        }
    } chassis_listener_;

    class BatteryListener : public DataReaderListener {
    public:
        void on_data_available(DataReader* reader) override {
            BatteryData data;
            SampleInfo info;
            while (reader->take_next_sample(&data, &info) == ReturnCode_t::RETCODE_OK) {
                if (info.valid_data) {
                    std::cout << "\n=== Battery Data ===\n";
                    std::cout << "Voltage: " << data.voltage() << "V\n";
                    std::cout << "Current: " << data.current() << "A\n";
                    std::cout << "Temperature: " << data.temperature() << "°C\n";
                    std::cout << "State of Charge: " << data.state_of_charge() << "%\n";
                    std::cout << "Power Consumption: " << data.power_consumption() << "W\n";
                    std::cout << "Charging Status: " << (data.charging_status() ? "Charging" : "Not Charging") << "\n";
                }
            }
        }
    } battery_listener_;

    class ADASListener : public DataReaderListener {
    public:
        void on_data_available(DataReader* reader) override {
            ADASData data;
            SampleInfo info;
            while (reader->take_next_sample(&data, &info) == ReturnCode_t::RETCODE_OK) {
                if (info.valid_data) {
                    std::cout << "\n=== ADAS Data ===\n";
                    std::cout << "Forward Collision Distance: " << data.forward_collision_distance() << "m\n";
                    std::cout << "Lane Deviation: " << data.lane_deviation() << "m\n";
                    std::cout << "Lane Departure Warning: " << (data.lane_departure_warning() ? "ACTIVE" : "inactive") << "\n";
                    std::cout << "Forward Collision Warning: " << (data.forward_collision_warning() ? "ACTIVE" : "inactive") << "\n";
                    std::cout << "Blind Spot Warning (L/R): " 
                             << (data.blind_spot_warning_left() ? "LEFT " : "")
                             << (data.blind_spot_warning_right() ? "RIGHT" : "") << "\n";
                    std::cout << "Adaptive Cruise Speed: " << data.adaptive_cruise_speed() << "km/h\n";
                    std::cout << "Time to Collision: " << data.time_to_collision() << "s\n";
                }
            }
        }
    } adas_listener_;

 // 토픽 구독 상태 관리
    std::map<std::string, bool> topic_status_;  // true: 구독 중, false: 구독 안함
    std::mutex topic_mutex_;

    // 토픽 구독/구독취소 함수
    bool subscribe_topic(const std::string& topic_name) {
        std::lock_guard<std::mutex> lock(topic_mutex_);
        
        if (topic_readers_.find(topic_name) != topic_readers_.end()) {
            std::cout << "Already subscribed to " << topic_name << std::endl;
            return true;
        }

        TopicReader topic_reader;
        if (topic_name == "powertrain") {
            topic_reader.type = TypeSupport(new PowertrainDataPubSubType());
            topic_reader.type.register_type(participant_);
            topic_reader.topic = participant_->create_topic("PowertrainTopic", "PowertrainData", TOPIC_QOS_DEFAULT);
            topic_reader.reader = subscriber_->create_datareader(topic_reader.topic, DATAREADER_QOS_DEFAULT, &powertrain_listener_);
        }
        else if (topic_name == "chassis") {
            topic_reader.type = TypeSupport(new ChassisDataPubSubType());
            topic_reader.type.register_type(participant_);
            topic_reader.topic = participant_->create_topic("ChassisTopic", "ChassisData", TOPIC_QOS_DEFAULT);
            topic_reader.reader = subscriber_->create_datareader(topic_reader.topic, DATAREADER_QOS_DEFAULT, &chassis_listener_);
        }
        else if (topic_name == "battery") {
            topic_reader.type = TypeSupport(new BatteryDataPubSubType());
            topic_reader.type.register_type(participant_);
            topic_reader.topic = participant_->create_topic("BatteryTopic", "BatteryData", TOPIC_QOS_DEFAULT);
            topic_reader.reader = subscriber_->create_datareader(topic_reader.topic, DATAREADER_QOS_DEFAULT, &battery_listener_);
        }
        else if (topic_name == "adas") {
            topic_reader.type = TypeSupport(new ADASDataPubSubType());
            topic_reader.type.register_type(participant_);
            topic_reader.topic = participant_->create_topic("ADASTopic", "ADASData", TOPIC_QOS_DEFAULT);
            topic_reader.reader = subscriber_->create_datareader(topic_reader.topic, DATAREADER_QOS_DEFAULT, &adas_listener_);
        }
        else {
            std::cout << "Unknown topic: " << topic_name << std::endl;
            return false;
        }

        topic_readers_[topic_name] = topic_reader;
        topic_status_[topic_name] = true;
        std::cout << "Successfully subscribed to " << topic_name << std::endl;
        return true;
    }

    bool unsubscribe_topic(const std::string& topic_name) {
        std::lock_guard<std::mutex> lock(topic_mutex_);
        
        auto it = topic_readers_.find(topic_name);
        if (it == topic_readers_.end()) {
            std::cout << "Not subscribed to " << topic_name << std::endl;
            return true;
        }

        subscriber_->delete_datareader(it->second.reader);
        participant_->delete_topic(it->second.topic);
        topic_readers_.erase(it);
        topic_status_[topic_name] = false;
        
        std::cout << "Successfully unsubscribed from " << topic_name << std::endl;
        return true;
    }

    void show_status() {
        std::lock_guard<std::mutex> lock(topic_mutex_);
        std::cout << "\nCurrent subscription status:\n";
        for (const auto& status : topic_status_) {
            std::cout << status.first << ": " 
                     << (status.second ? "Subscribed" : "Unsubscribed") << std::endl;
        }
    }




public:
    bool init() {
        // Create participant
        DomainParticipantQos participantQos;
        participantQos.name("VehicleSystems_Subscriber");
        participant_ = DomainParticipantFactory::get_instance()->create_participant(0, participantQos);
        if (participant_ == nullptr) return false;

        // Create subscriber
        subscriber_ = participant_->create_subscriber(SUBSCRIBER_QOS_DEFAULT);
        if (subscriber_ == nullptr) return false;

        // Initialize all topics by default
        std::vector<std::string> all_topics = {"powertrain", "chassis", "battery", "adas"};
        for (const auto& topic : all_topics) {
            topic_status_[topic] = false;  // 초기 상태 설정
            subscribe_topic(topic);        // 모든 토픽 구독
        }

        return true;
    }

    void run() {
        std::cout << "\nSubscriber running. Available commands:\n"
                  << "subscribe <topic>   : Subscribe to a topic\n"
                  << "unsubscribe <topic> : Unsubscribe from a topic\n"
                  << "status             : Show current subscription status\n"
                  << "quit               : Exit the program\n"
                  << "\nAvailable topics: powertrain, chassis, battery, adas\n" << std::endl;

        std::string command;
        std::string topic;

        while (true) {
            std::cout << "> ";
            std::cin >> command;

            if (command == "quit") {
                break;
            }
            else if (command == "status") {
                show_status();
            }
            else if (command == "subscribe" || command == "unsubscribe") {
                std::cin >> topic;
                if (command == "subscribe") {
                    subscribe_topic(topic);
                } else {
                    unsubscribe_topic(topic);
                }
            }
            else {
                std::cout << "Unknown command. Available commands: subscribe, unsubscribe, status, quit" << std::endl;
            }
        }
    }
};

int main() {
    VehicleSystemsSubscriber* subscriber = new VehicleSystemsSubscriber();
    if (subscriber->init()) {
        subscriber->run();
    }
    delete subscriber;
    return 0;
}