#include "VehicleDiagnostics.h"
#include "VehicleDiagnosticsPubSubTypes.h"

#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/DataWriterListener.hpp>

#include <chrono>
#include <thread>
#include <random>
#include <iostream>
#include <string>
#include <atomic>
#include <mutex>

using namespace eprosima::fastdds::dds;

class VehicleDiagnosticsPublisher {
private:
    VehicleDiagnostics diagnostics_;
    DomainParticipant* participant_;
    Publisher* publisher_;
    Topic* topic_;
    DataWriter* writer_;
    TypeSupport type_;
    
    std::random_device rd_;
    std::mt19937 gen_;
    std::uniform_real_distribution<> rpm_dist_;
    std::uniform_real_distribution<> speed_dist_;
    std::uniform_real_distribution<> temp_dist_;
    std::uniform_real_distribution<> fuel_dist_;
    std::uniform_real_distribution<> voltage_dist_;

    std::mutex mtx_;
    std::atomic<bool> is_running_;
    bool use_random_values_;

public:
    VehicleDiagnosticsPublisher() 
        : participant_(nullptr)
        , publisher_(nullptr)
        , topic_(nullptr)
        , writer_(nullptr)
        , type_(new VehicleDiagnosticsPubSubType())
        , gen_(rd_())
        , rpm_dist_(800.0, 3000.0)
        , speed_dist_(0.0, 120.0)
        , temp_dist_(75.0, 95.0)
        , fuel_dist_(0.0, 100.0)
        , voltage_dist_(11.0, 14.4)
        , is_running_(true)
        , use_random_values_(true) {
    }

    bool init() {
        DomainParticipantQos participantQos;
        participantQos.name("VehicleDiagnostics_Publisher");
        
        participant_ = DomainParticipantFactory::get_instance()->create_participant(0, participantQos);
        if (participant_ == nullptr) return false;

        type_.register_type(participant_);

        publisher_ = participant_->create_publisher(PUBLISHER_QOS_DEFAULT);
        if (publisher_ == nullptr) return false;

        topic_ = participant_->create_topic("VehicleDiagnosticsTopic", "VehicleDiagnostics", TOPIC_QOS_DEFAULT);
        if (topic_ == nullptr) return false;

        writer_ = publisher_->create_datawriter(topic_, DATAWRITER_QOS_DEFAULT);
        if (writer_ == nullptr) return false;

        return true;
    }

    void set_value(const std::string& param, float value) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (param == "rpm") diagnostics_.engine_rpm(value);
        else if (param == "speed") diagnostics_.vehicle_speed(value);
        else if (param == "temp") diagnostics_.engine_temperature(value);
        else if (param == "fuel") diagnostics_.fuel_level(value);
        else if (param == "voltage") diagnostics_.battery_voltage(value);
    }

    void update_random_values() {
        std::lock_guard<std::mutex> lock(mtx_);
        diagnostics_.engine_rpm(rpm_dist_(gen_));
        diagnostics_.vehicle_speed(speed_dist_(gen_));
        diagnostics_.engine_temperature(temp_dist_(gen_));
        diagnostics_.fuel_level(fuel_dist_(gen_));
        diagnostics_.battery_voltage(voltage_dist_(gen_));
    }

    bool publish() {
        std::lock_guard<std::mutex> lock(mtx_);
        
        diagnostics_.timestamp(std::chrono::system_clock::now().time_since_epoch().count());
        diagnostics_.vehicle_id("VIN123456789");

        if (diagnostics_.engine_temperature() > 90.0) {
            ErrorCode error;
            error.code("P0217");
            error.description("Engine Overheating Warning");
            error.is_critical(true);
            diagnostics_.error_codes().push_back(error);
        }

        writer_->write(&diagnostics_);
        return true;
    }

    void stop() {
        is_running_ = false;
    }

    void disable_random() {
        use_random_values_ = false;
    }

    void run() {
        // 첫 번째는 랜덤값으로 시작
        update_random_values();
        
        // 발행 스레드 시작
        std::thread publish_thread([this]() {
            while (is_running_) {
                if (use_random_values_) {
                    update_random_values();
                }
                publish();
                std::this_thread::sleep_for(std::chrono::milliseconds(4000));
            }
        });

        // 사용자 입력 처리
        std::string command;
        while (is_running_) {
            std::cout << "\nCommands:\n"
                     << "rpm <value> : Set engine RPM\n"
                     << "speed <value> : Set vehicle speed\n"
                     << "temp <value> : Set engine temperature\n"
                     << "fuel <value> : Set fuel level\n"
                     << "voltage <value> : Set battery voltage\n"
                     << "random : Enable random values\n"
                     << "manual : Disable random values\n"
                     << "quit : Exit program\n"
                     << "> ";

            std::string param;
            float value;
            std::cin >> param;

            if (param == "quit") {
                stop();
                break;
            } else if (param == "random") {
                use_random_values_ = true;
                std::cout << "Random values enabled\n";
            } else if (param == "manual") {
                use_random_values_ = false;
                std::cout << "Manual mode enabled\n";
            } else {
                std::cin >> value;
                set_value(param, value);
                if (!use_random_values_) {
                    std::cout << param << " set to " << value << std::endl;
                }
            }
        }

        if (publish_thread.joinable()) {
            publish_thread.join();
        }
    }
};

int main() {
    VehicleDiagnosticsPublisher* publisher = new VehicleDiagnosticsPublisher();
    if (publisher->init()) {
        publisher->run();
    }
    delete publisher;
    return 0;
}