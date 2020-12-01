#include <cstdlib>
#include <iostream>
#include <queue>
#include <vector>
#include <string>
#include <cstring>
#include <thread>
#include <mutex>
#include <chrono>
#include "mqtt/client.h"

const unsigned int NUMBER_OF_PUBLISHERS = 3;   
const std::string DEFAULT_SERVER_ADDRESS {"tcp://localhost:1883"};
const std::string DEFAULT_SUBSCRIBER_ID {"subsciber"};
const auto TIMEOUT = std::chrono::seconds(20);
const std::string TOPIC {"Sample Topic"};
const unsigned int QoS = 1;
std::mutex queueMutex;

void readMessages(std::queue<std::string>&);
void createPublishers(std::vector<mqtt::async_client_ptr>&, const std::string);
void connectPublishers(std::vector<mqtt::async_client_ptr>&, const mqtt::connect_options&);
void createThreads(std::vector<mqtt::async_client_ptr>&, std::queue<std::string>&);
void publishMessages(mqtt::async_client_ptr, std::queue<std::string>&);
void disconnectPublishers (std::vector<mqtt::async_client_ptr>&);

int main(int argc, char *argv[])
{
    /*  read all the messages from the user */    
    std::queue<std::string> messages;  
    readMessages(messages);

    /*  initialization of the server and establishing the connection's options */ 
    std::string address = (argc > 1) ? (argv[1]) : (DEFAULT_SERVER_ADDRESS);    
    
    mqtt::connect_options connectOpts;
    connectOpts.set_clean_session(true);
    connectOpts.set_keep_alive_interval(TIMEOUT);

    mqtt::message failureMessage(TOPIC,"disconnection", QoS, true);
    mqtt::will_options willOpts(failureMessage);
    connectOpts.set_will(willOpts);

    /* try to connect to the server */
    try{
       
       /* creating the publishers and connecting each of them to the server */
       std::vector<mqtt::async_client_ptr> publishers;
       std::vector<std::thread> threads;
       createPublishers(publishers, address);
       connectPublishers(publishers, connectOpts);

       /* creating associated threads to publishers */
       createThreads(publishers, messages);

       /* create the subscriber and connect it to the server */
       mqtt::async_client subscriber(address, DEFAULT_SUBSCRIBER_ID);
       mqtt::token_ptr connectionToken = subscriber.connect(connectOpts);
       connectionToken->wait();

       /* subscribe and consume */
       subscriber.start_consuming();
       subscriber.subscribe(TOPIC, QoS)->wait();
       bool isFinished = false;
       std::cout << "Received messages:" << std::endl;

       while (!isFinished){

           mqtt::const_message_ptr currentMessage = subscriber.consume_message();
           if (currentMessage == NULL){
               isFinished = true;
           }
           else{
               std::cout << "Messages : " << currentMessage->get_payload_str()
                         << " | Topic : " << currentMessage->get_topic() << std::endl;
           }
       }
       
       /* disconnect the publishers */
       disconnectPublishers(publishers);
       
       /* disconnect subscriber and stop consuming */
       subscriber.unsubscribe(TOPIC)->wait();
       subscriber.stop_consuming();
       mqtt::token_ptr disconnectionToken = subscriber.disconnect();
       disconnectionToken->wait(); /* wait for the disconnection to be completed */

    }
    catch (const mqtt::exception& exception){ /* connection failure has occurred */
        std::cerr << exception.what() << std::endl;
        return 1;
    } 
    return 0;
}

void readMessages(std::queue<std::string>& messages)
{
    std::string tmpMessage;
    bool isFinished = false;
    while (getline(std::cin, tmpMessage) && !isFinished){
        if(tmpMessage.empty()){
            isFinished = true;
        }
        else{
            messages.push(tmpMessage);
        }
    }
    return;
}

void createPublishers(std::vector<mqtt::async_client_ptr>& publishers, const std::string address)
{
    for (unsigned int it = 0; it < NUMBER_OF_PUBLISHERS; it++){
        mqtt::async_client_ptr client = std::make_shared<mqtt::async_client>(address, std::to_string(it+1));
        publishers.push_back(std::move(client));
    }
    return;
}

void connectPublishers(std::vector<mqtt::async_client_ptr>& publishers, const mqtt::connect_options& connectOpts)
{
    for(auto &currentPublisher:publishers){
        mqtt::token_ptr connectionToken = currentPublisher->connect(connectOpts);
        connectionToken->wait(); /* wait for the connection to be completed */
    }
    return;
}

void createThreads(std::vector<mqtt::async_client_ptr>& publishers, std::queue<std::string>& messages)
{
    std::vector<std::thread> threads;
    std::thread t1(publishMessages, publishers.at(0), std::ref(messages));
    threads.push_back(std::move(t1));
    
    std::thread t2(publishMessages, publishers.at(1), std::ref(messages));
    threads.push_back(std::move(t2));
    
    std::thread t3(publishMessages, publishers.at(2), std::ref(messages));
    threads.push_back(std::move(t3));
    
    for (auto &currentThread:threads){
        currentThread.join(); /* all the created threads are joined back to the main thread */
    }
    std::cout << "All messages are published to the topic" << std::endl;
    return;
}

void publishMessages(mqtt::async_client_ptr currentPublisher, std::queue<std::string>& messages)
{   
    while (!messages.empty()){
        queueMutex.lock(); /* synchronization of publishers */
        mqtt::message_ptr currentMessage = mqtt::make_message(TOPIC, messages.front());
        messages.pop(); 
        currentMessage->set_qos(QoS);
        currentPublisher->publish(currentMessage)->wait();
        std::cout << "Publisher " << currentPublisher->get_client_id()
                  << " Published " << currentMessage->get_payload_str()
                  << " to the Topic " << currentMessage->get_topic()
                  << std::endl;          
        queueMutex.unlock();   
    }
    return;
}

void disconnectPublishers (std::vector<mqtt::async_client_ptr>& publishers)
{
    for(auto &currentPublisher:publishers){
        mqtt::token_ptr disconnectionToken = currentPublisher->disconnect();
        disconnectionToken->wait(); /* wait for the disconnection to be completed */
    }    
    return;
}