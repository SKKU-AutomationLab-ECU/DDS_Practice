#include "VehicleDiagnostics.h"
#include "VehicleDiagnosticsPubSubTypes.h"

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
#include <ctime>

using namespace eprosima::fastdds::dds;

class VehicleDiagnosticsSubscriber {
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
            VehicleDiagnostics sample;
            SampleInfo info;
            
            while (reader->take_next_sample(&sample, &info) == ReturnCode_t::RETCODE_OK) {
                if (info.valid_data) {
                    // Convert timestamp to human readable format
                    auto timestamp = std::chrono::system_clock::time_point(
                        std::chrono::nanoseconds(sample.timestamp()));
                    auto time_t = std::chrono::system_clock::to_time_t(timestamp);
                    
                    std::cout << "\033[2J\033[H";  // Clear screen and move cursor to top
                    std::cout << "=== Vehicle Diagnostics Report ===\n";
                    std::cout << "Time: " << std::ctime(&time_t);
                    std::cout << "Vehicle ID: " << sample.vehicle_id() << "\n\n";
                    
                    // Display gauge for RPM
                    std::cout << "Engine RPM: " << std::fixed << std::setprecision(1) 
                              << sample.engine_rpm() << " RPM";
                    if (sample.engine_rpm() > 2500) {
                        std::cout << " \033[31m[HIGH]\033[0m";
                    }
                    std::cout << "\n";
                    
                    // Display other metrics
                    std::cout << "Vehicle Speed: " << sample.vehicle_speed() << " km/h\n";
                    std::cout << "Engine Temp: " << sample.engine_temperature() << " °C";
                    if (sample.engine_temperature() > 90) {
                        std::cout << " \033[31m[WARNING]\033[0m";
                    }
                    std::cout << "\n";
                    
                    std::cout << "Fuel Level: " << sample.fuel_level() << "%";
                    if (sample.fuel_level() < 20) {
                        std::cout << " \033[33m[LOW]\033[0m";
                    }
                    std::cout << "\n";
                    
                    std::cout << "Battery: " << sample.battery_voltage() << "V";
                    if (sample.battery_voltage() < 11.5) {
                        std::cout << " \033[31m[LOW]\033[0m";
                    }
                    std::cout << "\n\n";

                    // Display error codes if any
                    if (!sample.error_codes().empty()) {
                        std::cout << "=== Active Error Codes ===\n";
                        for (const auto& error : sample.error_codes()) {
                            std::cout << error.code() << ": " << error.description();
                            if (error.is_critical()) {
                                std::cout << " \033[31m[CRITICAL]\033[0m";
                            }
                            std::cout << "\n";
                        }
                    }
                }
            }
        }
    } listener_;

public:
    VehicleDiagnosticsSubscriber()
        : participant_(nullptr)
        , subscriber_(nullptr)
        , topic_(nullptr)
        , reader_(nullptr)
        , type_(new VehicleDiagnosticsPubSubType()) {
    }

    bool init() {
        DomainParticipantQos participantQos;
        participantQos.name("VehicleDiagnostics_Subscriber");
        
        participant_ = DomainParticipantFactory::get_instance()->create_participant(0, participantQos);
        if (participant_ == nullptr) return false;

        type_.register_type(participant_);

        subscriber_ = participant_->create_subscriber(SUBSCRIBER_QOS_DEFAULT);
        if (subscriber_ == nullptr) return false;

        topic_ = participant_->create_topic("VehicleDiagnosticsTopic", "VehicleDiagnostics", TOPIC_QOS_DEFAULT);
        if (topic_ == nullptr) return false;

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
    VehicleDiagnosticsSubscriber* subscriber = new VehicleDiagnosticsSubscriber();
    if (subscriber->init()) {
        subscriber->run();
    }
    return 0;
}