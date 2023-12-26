//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 

#include "tryQ.h"
#include "Job.h"
#include "IServer.h"
#include "inet/applications/tcpapp/GenericAppMsg_m.h"

namespace ErgodicityTest {

Define_Module(TryQ);

TryQ::TryQ()
{
    selectionStrategy = nullptr;
}

TryQ::~TryQ()
{
    delete selectionStrategy;
}

void TryQ::initialize()
{
    delay=par("droppedMsgSendDelay");

    droppedSignal = registerSignal("dropped");
    receivedSignal = registerSignal("received");
    queueingTimeSignal = registerSignal("queueingTime");
    queueLengthSignal = registerSignal("queueLength");
    emit(queueLengthSignal, 0);

    capacity = par("capacity");
    queue.setName("queue");

    fifo = par("fifo");

      selectionStrategy = tryQStrategy::create(par("sendingAlgorithm"), this, false);
  //  selectionStrategy= tryQStrategy::create("roundRobin", this, false);

    if (!selectionStrategy)
        throw cRuntimeError("invalid selection strategy");
}

void TryQ::handleMessage(cMessage *msg)
{
   // Job *job = check_and_cast<Job *>(msg);
  //  job->setTimestamp();

    emit(receivedSignal, 1);
       msg->setTimestamp(simTime());
    // check for container capacity
    if (capacity >= 0 && queue.getLength() >= capacity) {
        EV_WARN << "Server Scheduler - Queue full! Msg dropped.\n";
        if (hasGUI())
            bubble("Dropped!");
        emit(droppedSignal, 1);
        EV_WARN <<"msg dropped is(msg Kind):" <<msg->getKind()<<endl;
        //scheduleAt(simTime()+delay, msg);   //try to push message to the queue after delay in hope queue free up some space  , we can't do this because we mess up the ordering
        delete msg;
        return;
    }

    int k = selectionStrategy->select();
    if (k < 0) {
        // enqueue if no idle server found
        queue.insert(msg);
        emit(queueLengthSignal, length());
       // job->setQueueCount(job->getQueueCount() + 1);

    }
    else if (length() == 0) {
        // send through without queueing
        sendJob(msg, k);
    }
      else
        throw cRuntimeError("This should not happen. Queue is NOT empty and there is an IDLE server attached to us.");
}

void TryQ::refreshDisplay() const
{
    // change the icon color
    getDisplayString().setTagArg("i", 1, queue.isEmpty() ? "" : "cyan");
}

int TryQ::length()
{
    return queue.getLength();
}

void TryQ::request(int gateIndex)
{
    Enter_Method("request()!");

    ASSERT(!queue.isEmpty());

    cMessage *msg;
    if (fifo) {
        msg = (cMessage *)queue.pop();
    }
    else {
        msg = (cMessage *)queue.back();
        // FIXME this may have bad performance as remove uses linear search
        queue.remove(msg);
    }
    emit(queueLengthSignal, length());

//    job->setQueueCount(job->getQueueCount()+1);
    simtime_t d = simTime() - msg->getTimestamp();
  //  job->setTotalQueueingTime(job->getTotalQueueingTime() + d);

    emit(queueingTimeSignal, d);

    sendJob(msg, gateIndex);
}

void TryQ::sendJob(cMessage *msg, int gateIndex)
{
    cGate *out = gate("out", gateIndex);
    send(msg, out);
    check_and_cast<IServer *>(out->getPathEndGate()->getOwnerModule())->allocate();
}

}; //namespace

