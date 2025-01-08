#include "VehicleSystems.h"
#include "VehicleSystemsPubSubTypes.h"

#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>

#include <thread>
#include <chrono>
#include <mutex>
#include <atomic>
#include <random>
#include <iostream>
#include <map>

using namespace eprosima::fastdds::dds;

class VehicleSystemsPublisher {
private:
    // DDS Entities
    DomainParticipant* participant_;
    Publisher* publisher_;
    
    // Topics and Writers for each system
    struct TopicWriter {
        Topic* topic;
        DataWriter* writer;
        TypeSupport type;
    };

    std::map<std::string, TopicWriter> topic_writers_;
    
    // Data structures
    PowertrainData powertrain_data_;
    ChassisData chassis_data_;
    BatteryData battery_data_;
    ADASData adas_data_;

    // Thread control
    std::atomic<bool> is_running_;
    std::mutex mtx_;
    bool use_random_values_;

    // Random generators
    std::random_device rd_;
    std::mt19937 gen_;

public:
    VehicleSystemsPublisher() 
        : participant_(nullptr)
        , publisher_(nullptr)
        , is_running_(true)
        , use_random_values_(true)
        , gen_(rd_()) {
    }

    bool init() {
        // Create participant
        DomainParticipantQos participantQos;
        participantQos.name("VehicleSystems_Publisher");
        participant_ = DomainParticipantFactory::get_instance()->create_participant(0, participantQos);
        if (participant_ == nullptr) return false;

        // Create publisher
        publisher_ = participant_->create_publisher(PUBLISHER_QOS_DEFAULT);
        if (publisher_ == nullptr) return false;

        // Initialize Powertrain topic and writer
        TopicWriter powertrain;
        powertrain.type = TypeSupport(new PowertrainDataPubSubType());
        powertrain.type.register_type(participant_);
        powertrain.topic = participant_->create_topic("PowertrainTopic", "PowertrainData", TOPIC_QOS_DEFAULT);
        powertrain.writer = publisher_->create_datawriter(powertrain.topic, DATAWRITER_QOS_DEFAULT);
        topic_writers_["powertrain"] = powertrain;

        // Initialize Chassis topic and writer
        TopicWriter chassis;
        chassis.type = TypeSupport(new ChassisDataPubSubType());
        chassis.type.register_type(participant_);
        chassis.topic = participant_->create_topic("ChassisTopic", "ChassisData", TOPIC_QOS_DEFAULT);
        chassis.writer = publisher_->create_datawriter(chassis.topic, DATAWRITER_QOS_DEFAULT);
        topic_writers_["chassis"] = chassis;

        // Initialize Battery topic and writer
        TopicWriter battery;
        battery.type = TypeSupport(new BatteryDataPubSubType());
        battery.type.register_type(participant_);
        battery.topic = participant_->create_topic("BatteryTopic", "BatteryData", TOPIC_QOS_DEFAULT);
        battery.writer = publisher_->create_datawriter(battery.topic, DATAWRITER_QOS_DEFAULT);
        topic_writers_["battery"] = battery;

        // Initialize ADAS topic and writer
        TopicWriter adas;
        adas.type = TypeSupport(new ADASDataPubSubType());
        adas.type.register_type(participant_);
        adas.topic = participant_->create_topic("ADASTopic", "ADASData", TOPIC_QOS_DEFAULT);
        adas.writer = publisher_->create_datawriter(adas.topic, DATAWRITER_QOS_DEFAULT);
        topic_writers_["adas"] = adas;

        return true;
    }

    void update_random_values() {
    std::lock_guard<std::mutex> lock(mtx_);
    
    // Update Powertrain data
    powertrain_data_.timestamp(std::chrono::system_clock::now().time_since_epoch().count());
    powertrain_data_.engine_rpm(std::uniform_real_distribution<>(800.0, 3000.0)(gen_));
    powertrain_data_.engine_temperature(std::uniform_real_distribution<>(75.0, 95.0)(gen_));
    powertrain_data_.engine_load(std::uniform_real_distribution<>(0.0, 100.0)(gen_));
    powertrain_data_.transmission_temp(std::uniform_real_distribution<>(70.0, 90.0)(gen_));
    powertrain_data_.current_gear(std::uniform_int_distribution<>(1, 6)(gen_));
    powertrain_data_.throttle_position(std::uniform_real_distribution<>(0.0, 100.0)(gen_));
    // Random DTC codes
    if (std::uniform_real_distribution<>(0.0, 1.0)(gen_) < 0.1) {
        std::vector<std::string> codes = {"P0301", "P0302", "P0303"};
        powertrain_data_.dtc_codes(codes);
    }

    // Update Chassis data
    chassis_data_.timestamp(std::chrono::system_clock::now().time_since_epoch().count());
    chassis_data_.brake_pressure(std::uniform_real_distribution<>(0.0, 100.0)(gen_));
    chassis_data_.steering_angle(std::uniform_real_distribution<>(-30.0, 30.0)(gen_));
    // Update arrays
    for (int i = 0; i < 4; i++) {
        chassis_data_.suspension_height()[i] = std::uniform_real_distribution<>(150.0, 200.0)(gen_);
        chassis_data_.wheel_speed()[i] = std::uniform_real_distribution<>(0.0, 120.0)(gen_);
        chassis_data_.brake_pad_wear()[i] = std::uniform_real_distribution<>(0.0, 100.0)(gen_);
    }
    chassis_data_.abs_active(std::uniform_real_distribution<>(0.0, 1.0)(gen_) < 0.1);
    chassis_data_.traction_control_active(std::uniform_real_distribution<>(0.0, 1.0)(gen_) < 0.1);

    // Update Battery data
    battery_data_.timestamp(std::chrono::system_clock::now().time_since_epoch().count());
    battery_data_.voltage(std::uniform_real_distribution<>(11.0, 14.4)(gen_));
    battery_data_.current(std::uniform_real_distribution<>(-20.0, 100.0)(gen_));
    battery_data_.temperature(std::uniform_real_distribution<>(20.0, 40.0)(gen_));
    battery_data_.state_of_charge(std::uniform_real_distribution<>(0.0, 100.0)(gen_));
    battery_data_.power_consumption(std::uniform_real_distribution<>(0.0, 3000.0)(gen_));
    battery_data_.charging_cycles(std::uniform_int_distribution<>(0, 1000)(gen_));
    battery_data_.charging_status(std::uniform_real_distribution<>(0.0, 1.0)(gen_) < 0.2);

    // Update ADAS data
    adas_data_.timestamp(std::chrono::system_clock::now().time_since_epoch().count());
    adas_data_.forward_collision_distance(std::uniform_real_distribution<>(0.0, 100.0)(gen_));
    adas_data_.lane_deviation(std::uniform_real_distribution<>(-1.0, 1.0)(gen_));
    adas_data_.lane_departure_warning(std::uniform_real_distribution<>(0.0, 1.0)(gen_) < 0.1);
    adas_data_.forward_collision_warning(std::uniform_real_distribution<>(0.0, 1.0)(gen_) < 0.1);
    adas_data_.blind_spot_warning_left(std::uniform_real_distribution<>(0.0, 1.0)(gen_) < 0.1);
    adas_data_.blind_spot_warning_right(std::uniform_real_distribution<>(0.0, 1.0)(gen_) < 0.1);
    // Update obstacle distances
    std::vector<float> obstacles;
    int num_obstacles = std::uniform_int_distribution<>(1, 3)(gen_);
    for (int i = 0; i < num_obstacles; i++) {
        obstacles.push_back(std::uniform_real_distribution<>(1.0, 50.0)(gen_));
    }
    adas_data_.obstacle_distances(obstacles);
    adas_data_.adaptive_cruise_speed(std::uniform_real_distribution<>(0.0, 120.0)(gen_));
    adas_data_.time_to_collision(std::uniform_real_distribution<>(0.0, 10.0)(gen_));
}

    void publish_data() {
        std::lock_guard<std::mutex> lock(mtx_);
        
        topic_writers_["powertrain"].writer->write(&powertrain_data_);
        topic_writers_["chassis"].writer->write(&chassis_data_);
        topic_writers_["battery"].writer->write(&battery_data_);
        topic_writers_["adas"].writer->write(&adas_data_);
    }

    void run() {
        std::thread publish_thread([this]() {
            while (is_running_) {
                if (use_random_values_) {
                    update_random_values();
                }
                publish_data();
                std::this_thread::sleep_for(std::chrono::milliseconds(4000));  // 10Hz update rate
            }
        });

        handle_user_input();

        if (publish_thread.joinable()) {
            publish_thread.join();
        }
    }

    void handle_user_input() {
        std::string command;
        while (is_running_) {
            std::cout << "\nAvailable commands:\n"
          << "Powertrain commands:\n"
          << "  powertrain rpm <value> : Set engine RPM\n"
          << "  powertrain temp <value> : Set engine temperature\n"
          << "  powertrain load <value> : Set engine load\n"
          << "  powertrain trans_temp <value> : Set transmission temperature\n"
          << "  powertrain gear <value> : Set current gear\n"
          << "  powertrain throttle <value> : Set throttle position\n"
          << "\nChassis commands:\n"
          << "  chassis brake <value> : Set brake pressure\n"
          << "  chassis steering <value> : Set steering angle\n"
          << "  chassis susp_fl/fr/rl/rr <value> : Set suspension height\n"
          << "  chassis wheel_fl/fr/rl/rr <value> : Set wheel speed\n"
          << "  chassis abs <0|1> : Set ABS status\n"
          << "  chassis traction <0|1> : Set traction control status\n"
          << "\nBattery commands:\n"
          << "  battery voltage <value> : Set battery voltage\n"
          << "  battery current <value> : Set battery current\n"
          << "  battery temp <value> : Set battery temperature\n"
          << "  battery charge <value> : Set state of charge\n"
          << "  battery power <value> : Set power consumption\n"
          << "  battery cycles <value> : Set charging cycles\n"
          << "  battery charging <0|1> : Set charging status\n"
          << "\nADAS commands:\n"
          << "  adas distance <value> : Set forward collision distance\n"
          << "  adas deviation <value> : Set lane deviation\n"
          << "  adas lane_warning <0|1> : Set lane departure warning\n"
          << "  adas collision_warning <0|1> : Set collision warning\n"
          << "  adas blind_left <0|1> : Set left blind spot warning\n"
          << "  adas blind_right <0|1> : Set right blind spot warning\n"
          << "  adas cruise_speed <value> : Set adaptive cruise speed\n"
          << "  adas collision_time <value> : Set time to collision\n"
          << "\nOther commands:\n"
          << "  random : Enable random mode\n"
          << "  manual : Disable random mode\n"
          << "  quit : Exit program\n"
          << "> ";

            std::string system, param;
            std::cin >> system;

            if (system == "quit") {
                is_running_ = false;
                break;
            } else if (system == "random") {
                use_random_values_ = true;
                std::cout << "Random mode enabled\n";
            } else if (system == "manual") {
                use_random_values_ = false;
                std::cout << "Manual mode enabled\n";
            } else {
                std::cin >> param;
                float value;
                std::cin >> value;
                set_value(system, param, value);
            }
        }
    }

    void set_value(const std::string& system, const std::string& param, float value) {
    std::lock_guard<std::mutex> lock(mtx_);
    
    if (system == "powertrain") {
        if (param == "rpm") powertrain_data_.engine_rpm(value);
        else if (param == "temp") powertrain_data_.engine_temperature(value);
        else if (param == "load") powertrain_data_.engine_load(value);
        else if (param == "trans_temp") powertrain_data_.transmission_temp(value);
        else if (param == "gear") powertrain_data_.current_gear(static_cast<long>(value));
        else if (param == "throttle") powertrain_data_.throttle_position(value);
    }
    else if (system == "chassis") {
        if (param == "brake") chassis_data_.brake_pressure(value);
        else if (param == "steering") chassis_data_.steering_angle(value);
        else if (param == "susp_fl") chassis_data_.suspension_height()[0] = value;
        else if (param == "susp_fr") chassis_data_.suspension_height()[1] = value;
        else if (param == "susp_rl") chassis_data_.suspension_height()[2] = value;
        else if (param == "susp_rr") chassis_data_.suspension_height()[3] = value;
        else if (param == "wheel_fl") chassis_data_.wheel_speed()[0] = value;
        else if (param == "wheel_fr") chassis_data_.wheel_speed()[1] = value;
        else if (param == "wheel_rl") chassis_data_.wheel_speed()[2] = value;
        else if (param == "wheel_rr") chassis_data_.wheel_speed()[3] = value;
        else if (param == "abs") chassis_data_.abs_active(value > 0);
        else if (param == "traction") chassis_data_.traction_control_active(value > 0);
    }
    else if (system == "battery") {
        if (param == "voltage") battery_data_.voltage(value);
        else if (param == "current") battery_data_.current(value);
        else if (param == "temp") battery_data_.temperature(value);
        else if (param == "charge") battery_data_.state_of_charge(value);
        else if (param == "power") battery_data_.power_consumption(value);
        else if (param == "cycles") battery_data_.charging_cycles(static_cast<long>(value));
        else if (param == "charging") battery_data_.charging_status(value > 0);
    }
    else if (system == "adas") {
        if (param == "distance") adas_data_.forward_collision_distance(value);
        else if (param == "deviation") adas_data_.lane_deviation(value);
        else if (param == "lane_warning") adas_data_.lane_departure_warning(value > 0);
        else if (param == "collision_warning") adas_data_.forward_collision_warning(value > 0);
        else if (param == "blind_left") adas_data_.blind_spot_warning_left(value > 0);
        else if (param == "blind_right") adas_data_.blind_spot_warning_right(value > 0);
        else if (param == "cruise_speed") adas_data_.adaptive_cruise_speed(value);
        else if (param == "collision_time") adas_data_.time_to_collision(value);
    }
}
};

int main() {
    VehicleSystemsPublisher* publisher = new VehicleSystemsPublisher();
    if (publisher->init()) {
        publisher->run();
    }
    delete publisher;
    return 0;
}