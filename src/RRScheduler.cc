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

#include "tryQStrategy.h"
#include "IPassiveQueue.h"
#include "RRScheduler.h"

namespace ergodicitytest {

Define_Module(RRScheduler);

RRScheduler::RRScheduler()
{
    selectionStrategy = nullptr;
    jobServiced = nullptr;
    endServiceMsg = nullptr;
    goToSleepMsg = nullptr;
    wakeUpMsg = nullptr;
    allocated = false;
    sleepFlag=false;
    schedulerFlag=false;
    shortDelay=0.001;

}

RRScheduler::~RRScheduler()
{
    delete selectionStrategy;
    delete jobServiced;
    cancelAndDelete(endServiceMsg);
    cancelAndDelete(goToSleepMsg);
    cancelAndDelete(wakeUpMsg);

}

void RRScheduler::initialize()
{
    busySignal = registerSignal("busy");
    emit(busySignal, false);
    endServiceMsg = new cMessage("end-service");
    goToSleepMsg = new cMessage("sleepQ");
    wakeUpMsg=new cMessage("wakeUpQ");
    jobServiced = nullptr;
    allocated = false;
    selectionStrategy = tryQStrategy::create(par("fetchingAlgorithm"), this, true);
    if (!selectionStrategy)
        throw cRuntimeError("invalid selection strategy");
    schedulerFlag=par("schedulerStatus");
    if(schedulerFlag)
        wakeup();
}

void RRScheduler::handleMessage(cMessage *msg)
{

    if (msg == goToSleepMsg)
    {
        sleep();
    }
    else if (msg == wakeUpMsg) {
        wakeup();
    }
    else if (msg == endServiceMsg) {
        if(sleepFlag==false)
        {
            ASSERT(jobServiced != nullptr);
            ASSERT(allocated);
            simtime_t d = simTime() - endServiceMsg->getSendingTime();
          //  jobServiced->setTotalServiceTime(jobServiced->getTotalServiceTime() + d);
            send(jobServiced, "out");
            jobServiced = nullptr;
            allocated = false;
            emit(busySignal, false);

            // examine all input queues, and request a new job from a non empty queue
            int k = selectionStrategy->select();
            if (k >= 0) {
                EV << "requesting job from attached queue " << endl;
                cGate *gate = selectionStrategy->selectableGate(k);
                check_and_cast<IPassiveQueue *>(gate->getOwnerModule())->request(gate->getIndex());
            }
        }
        else
        {
            wakeUpTime=wakeUpTime+shortDelay;
            scheduleAt(wakeUpTime, msg);
        }
    }
    else {
        if(sleepFlag==false)
        {
           if (!allocated)
                       error("job arrived, but the sender did not call allocate() previously");
           if (jobServiced)
               throw cRuntimeError("a new job arrived while already servicing one");
           emit(busySignal, true);
           jobServiced = msg;
           simtime_t serviceTime = par("serviceTime");
           simtime_t endServiceTime1 = simTime()+serviceTime;
           simtime_t endServiceTime2 = wakeUpTime+shortDelay;
           if (endServiceTime1>=endServiceTime2)
           {
               scheduleAt(endServiceTime1, endServiceMsg);
           }
           else
           {
               scheduleAt(endServiceTime2, endServiceMsg);

           }
        }
        else
        {
            wakeUpTime=wakeUpTime+shortDelay;
            scheduleAt(wakeUpTime, msg);
        }

       /* send(jobServiced, "out");
        allocated = false;
        jobServiced=nullptr;
        emit(busySignal, false);


        // examine all input queues, and request a new job from a non empty queue
        int k = selectionStrategy->select();
        if (k >= 0) {
           EV << "requesting job from queue " << k << endl;
           cGate *gate = selectionStrategy->selectableGate(k);
           check_and_cast<IPassiveQueue *>(gate->getOwnerModule())->request(gate->getIndex());
                }*/
    }
}
void RRScheduler::sleep()
{
   // allocated=true;
    sleepFlag=true;
    /*if (jobServiced)
    {
        send(jobServiced, "out");
        allocated = false;
        jobServiced=nullptr;
        emit(busySignal, false);
    }*/
    simtime_t sleepTime = par("sleepTime");
    wakeUpTime=simTime()+sleepTime;
    scheduleAt(wakeUpTime, wakeUpMsg);
  //  emit(busySignal, true);

}
void RRScheduler::wakeup()
{
  //  allocated=false;
    sleepFlag=false;
    simtime_t wakeTime = par("scheduleShare");
    scheduleAt(simTime()+wakeTime, goToSleepMsg);
   // emit(busySignal, false);


}
void RRScheduler::refreshDisplay() const
{
    getDisplayString().setTagArg("i2", 0, jobServiced ? "status/execute" : "");
}

void RRScheduler::finish()
{
}

bool RRScheduler::isIdle()
{
    return !allocated;  // we are idle if nobody has allocated us for processing
}

void RRScheduler::allocate()
{
    allocated = true;
}

}; //namespace

