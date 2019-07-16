#include <iostream>
#include <cstdlib>
#include <string>
#include <vector>
#include <assert.h>

#include "common.hpp"
#include "interconnect.hpp"

Interconnect::Interconnect(void *_sender, void *_receiver, float _clock, float _bw, float _receiver_capacity,
						   bool _is_sender_main_memory, std::vector<request> *senderqueue, std::vector<request> *servedqueue,
						   std::vector<request> *waitingqueue, std::vector<request> *requestqueue) {
	sender = _sender;
	receiver = _receiver;
	clock = _clock;
	bw = _bw;

	bpc = bw / clock;

	receiver_capacity = _receiver_capacity;
	is_sender_main_memory = _is_sender_main_memory;

	idle_cycle = 0;
	busy_cycle = 0;
	sent_size = 0;

	sender_queue = senderqueue;
	served_queue = servedqueue;
	waiting_queue = waitingqueue;
	request_queue = requestqueue;
}

Interconnect::~Interconnect() {
	delete sender_queue;
	delete served_queue;
	delete waiting_queue;
	delete request_queue;
}

/* Called when another request is made from the receiver.
 * The request is pushed into the receiver's request_queue, and taken care of in Cycle() */
void Interconnect::ReceiveRequest(request req) {
	//request r = req;
	request_queue->push_back(MakeRequest(req.order, req.size));
}

/* Checks to see if the receiver buffer is not overwhelmed with data
 * Returns true if receiver cannot receive any more data, false otherwise
 * Should be called when at least one of waiting_queue and sender_queue is not empty
 * But waiting_queue is empty only when sender_queue is empty,
 * so we should make sure sender_queue is nonempty */
bool Interconnect::ReceiverFull() {
	assert(!sender_queue->empty());

	float totalsize = 0;
	std::vector<request>::iterator it;
	for (it = served_queue->begin(); it != served_queue->end(); ++it) {
		totalsize += it->size;
	}
	int current_index = sender_queue->front().order;
	for (it = waiting_queue->begin(); it != waiting_queue->end(); ++it) {
		if (it->order == current_index) {
			totalsize += it->size;
			break;
		}
	}
	// if not found, then this is the start the transmission of new data
	if (it == waiting_queue->end())
		totalsize += sender_queue->front().size;
	// totalsize is amount of data in receiver if we finish the transmission about to start right now
	return (receiver_capacity < totalsize);
}

bool Interconnect::IsIdle() {
	return (sender_queue->empty() && served_queue->empty() && waiting_queue->empty() && request_queue->empty());
}

void Interconnect::Cycle() {
	std::vector<request>::iterator it;
	// send all pending requests in request_queue to sender
	while (!request_queue->empty()) {
		request req = MakeRequest(request_queue->front().order, request_queue->front().size);
		waiting_queue->push_back(req);
		pop_front(*request_queue);
		// if sender is main memory (DRAM, CPU), then they automagically have all the data they need to send already
		if (is_sender_main_memory) {
			sender_queue->push_back(req);
		}
	}
	
	// cycle count
	if (sender_queue->empty() || ReceiverFull())
		idle_cycle++;
	else {
		busy_cycle++;
		// sender sends data
		int order = sender_queue->front().order;
		float bts = sender_queue->front().size;
		bts = ((bts - bpc) < 0) ? 0 : (bts - bpc);
		// take care of request in queue
		if (bts != 0)
			sender_queue->front().size = bts;
		else {
			// the sender is done sending
			// from here on, bts holds value of the original requested size
			pop_front(*sender_queue);
			for (it = waiting_queue->begin(); it != waiting_queue->end(); ++it) {
				if (it->order == order) {
					bts = it->size;
					break;
				}
			}
			// update sent_size
			sent_size += bts;
			if (it != waiting_queue->end()) {
				// found
				waiting_queue->erase(it);
				served_queue->push_back(MakeRequest(order, bts));
			}
			else {
				// not found... something wrong
				assert(0);
			}
		}
	}
}

void Interconnect::PrintStats() {
   std::cout << "======================================================================" << std::endl;
   std::cout << "idle cycles: " << idle_cycle << ", busy cycles: " << busy_cycle << std::endl;
   std::cout << "total bytes sent over this interconnect: " << sent_size << std::endl;
   std::cout << "======================================================================" << std::endl;
}