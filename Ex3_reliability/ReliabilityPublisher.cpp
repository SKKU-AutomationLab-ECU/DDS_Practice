#include "ReliabilityTest.h"
#include "ReliabilityTestPubSubTypes.h"

#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/qos/DataWriterQos.hpp>

#include <chrono>
#include <thread>
#include <iostream>
#include <csignal>
#include <atomic>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <termios.h>
#include <queue>

using namespace eprosima::fastdds::dds;

// 전역 변수를 volatile sig_atomic_t로 변경
std::atomic<bool> g_is_paused{false};
std::atomic<bool> g_running{true};

// 키보드 입력을 비동기적으로 처리하는 함수
void keyboard_control() {
    struct termios old_tio, new_tio;
    
    // 터미널 설정 저장
    tcgetattr(STDIN_FILENO, &old_tio);
    new_tio = old_tio;
    
    // 새로운 터미널 설정 (canonical 모드 끄기)
    new_tio.c_lflag &= (~ICANON & ~ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);

    while (g_running) {
        char c;
        if (read(STDIN_FILENO, &c, 1) > 0) {
            if (c == 'p' || c == 'P') {  // 'p' 키로 일시정지/재개
                g_is_paused = !g_is_paused;
                std::cout << (g_is_paused ? "Publisher PAUSED" : "Publisher RESUMED") << std::endl;
            }
            else if (c == 'q' || c == 'Q') {  // 'q' 키로 종료
                g_running = false;
                std::cout << "Stopping publisher..." << std::endl;
            }
        }
    }

    // 터미널 설정 복구
    tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);
}

class ReliabilityPublisher {
    
private:
    DomainParticipant* participant_;
    Publisher* publisher_;
    Topic* reliable_topic_;
    Topic* best_effort_topic_;
    DataWriter* reliable_writer_;
    DataWriter* best_effort_writer_;
    TypeSupport type_;
    TestData data_;
    uint32_t sequence_number_;
    std::queue<TestData> paused_reliable_messages;


public:
    ReliabilityPublisher()
        : participant_(nullptr)
        , publisher_(nullptr)
        , reliable_topic_(nullptr)
        , best_effort_topic_(nullptr)
        , reliable_writer_(nullptr)
        , best_effort_writer_(nullptr)
        , type_(new TestDataPubSubType())
        , sequence_number_(0) {
    }

    ~ReliabilityPublisher() {
        if (publisher_ != nullptr) {
            if (reliable_writer_ != nullptr)
                publisher_->delete_datawriter(reliable_writer_);
            if (best_effort_writer_ != nullptr)
                publisher_->delete_datawriter(best_effort_writer_);
        }
        if (participant_ != nullptr) {
            if (reliable_topic_ != nullptr)
                participant_->delete_topic(reliable_topic_);
            if (best_effort_topic_ != nullptr)
                participant_->delete_topic(best_effort_topic_);
            if (publisher_ != nullptr)
                participant_->delete_publisher(publisher_);
            DomainParticipantFactory::get_instance()->delete_participant(participant_);
        }
    }

    bool init() {
        DomainParticipantQos participantQos;
        participantQos.name("Reliability_Publisher");
        participant_ = DomainParticipantFactory::get_instance()->create_participant(0, participantQos);
        if (participant_ == nullptr) return false;

        type_.register_type(participant_);

        publisher_ = participant_->create_publisher(PUBLISHER_QOS_DEFAULT);
        if (publisher_ == nullptr) return false;

        // Create topics
        reliable_topic_ = participant_->create_topic("ReliableTopic", "TestData", TOPIC_QOS_DEFAULT);
        best_effort_topic_ = participant_->create_topic("BestEffortTopic", "TestData", TOPIC_QOS_DEFAULT);

        if (reliable_topic_ == nullptr || best_effort_topic_ == nullptr) return false;

        // Configure RELIABLE QoS
        DataWriterQos reliable_qos = DATAWRITER_QOS_DEFAULT;
        reliable_qos.reliability().kind = RELIABLE_RELIABILITY_QOS;
        reliable_qos.history().kind = KEEP_ALL_HISTORY_QOS;
        reliable_writer_ = publisher_->create_datawriter(reliable_topic_, reliable_qos);

        // Configure BEST_EFFORT QoS
        DataWriterQos best_effort_qos = DATAWRITER_QOS_DEFAULT;
        best_effort_qos.reliability().kind = BEST_EFFORT_RELIABILITY_QOS;
        best_effort_qos.history().kind = KEEP_ALL_HISTORY_QOS;
        best_effort_writer_ = publisher_->create_datawriter(best_effort_topic_, best_effort_qos);

        if (reliable_writer_ == nullptr || best_effort_writer_ == nullptr) return false;

        return true;
    }

    bool publish() {
        

        data_.timestamp(std::chrono::system_clock::now().time_since_epoch().count());
        data_.sequence_number(sequence_number_);
        data_.is_critical(sequence_number_ % 5 == 0);
        

        std::string msg = "Message #" + std::to_string(sequence_number_);
        if (data_.is_critical()) {
            msg += " (CRITICAL)";
        }
        data_.message(msg);

        if (!g_is_paused) {
            // 일시정지 해제 시, 쌓여있던 reliable 메시지들 먼저 전송
            while (!paused_reliable_messages.empty()) {
                reliable_writer_->write(&paused_reliable_messages.front());
                std::cout << "Sending queued message: " 
                          << paused_reliable_messages.front().message() << std::endl;
                paused_reliable_messages.pop();
            }
            
            // 현재 메시지 전송
            reliable_writer_->write(&data_);
            best_effort_writer_->write(&data_);
            
            std::cout << "Published " << msg << std::endl;
        } else {
            // 일시정지 중에는 reliable 메시지만 큐에 저장
            if (data_.is_critical()) {
                paused_reliable_messages.push(data_);
                std::cout << "Queued critical message: " << msg << std::endl;
            } else {
                std::cout << "Skipped non-critical message while paused: " << msg << std::endl;
            }
        }
        
        std::cout.flush();
        sequence_number_++;
        return true;
    }

    void run() {
        std::cout << "Publisher running. Commands:\n"
                  << "- Press 'p' to toggle pause\n"
                  << "- Press 'q' to quit\n"
                  << "PID: " << getpid() << std::endl;
        std::cout.flush();

        // 키보드 입력을 처리할 스레드 시작
        std::thread keyboard_thread(keyboard_control);

        while (g_running) {
            publish();
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        // 키보드 스레드 종료 대기
        if (keyboard_thread.joinable()) {
            keyboard_thread.join();
        }
    }
};

int main() {
    try {
        ReliabilityPublisher publisher;
        if (publisher.init()) {
            publisher.run();
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}