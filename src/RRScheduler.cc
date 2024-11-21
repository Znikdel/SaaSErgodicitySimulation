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

namespace ErgodicityTest {

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
    wakeUpTime=0;
    wentSleep=0;
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
    else if (msg == endServiceMsg) { // send to the tryserver app to produce the response
        if(sleepFlag==false)
        {
            ASSERT(jobServiced != nullptr);
            ASSERT(allocated);
            simtime_t d = simTime() - endServiceMsg->getSendingTime(); //e.g
          //  jobServiced->setTotalServiceTime(jobServiced->getTotalServiceTime() + d);
            simtime_t age= simTime()-jobServiced->getCreationTime();
//            if(age>7)
//                std:: cout<<"   jobServiced age :  "<< age <<endl;

            send(jobServiced, "out"); // jobsesrviced is the msg came from client
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
            simtime_t d = simTime() - wentSleep; //e.g 120-112=8 the remaining service
            simtime_t d2=wakeUpTime+d; //e.g 130+8=138
            scheduleAt(d2, endServiceMsg);
        }
    }
    else {  //msg came from client
        if(sleepFlag==false)  // when scheduler state is false or its our turn
        {
           if (!allocated)
                       error("job arrived, but the sender did not call allocate() previously");
           if (jobServiced)
               throw cRuntimeError("a new job arrived while already servicing one");
           emit(busySignal, true);
           jobServiced = msg;
//           std:: cout<<"msg age:  "<<simTime()-msg->getCreationTime()<<"   jobServiced age:  "<< simTime()-jobServiced->getCreationTime()<<endl;
//           std:: cout<<msg->getSenderModule()<<"   jobServiced:  "<< jobServiced->getName()<<endl;

           simtime_t serviceTime = par("serviceTime"); //e.g. 10s
//           if (simTime()>5100)
//           std::cout<<"  In  Scheduler: "<< "at:   "<<simTime()<< "serviceTime:   "<<serviceTime<<endl;
           simtime_t endServiceTime1 = simTime()+serviceTime; //e.g 110+10=120s
           simtime_t endServiceTime2 = wakeUpTime+shortDelay; //e.g 102+0.001=102.001s
           if (endServiceTime1>=endServiceTime2) //why? to make sure we have a little bit of service time
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

     }
}
void RRScheduler::sleep()
{
    sleepFlag=true;
    simtime_t sleepTime = par("sleepTime"); //e.g 20s
    wakeUpTime=simTime()+sleepTime; //e.g 112+20 = 132
    wentSleep=simTime();
    scheduleAt(wakeUpTime, wakeUpMsg); // first wake up then work

}
void RRScheduler::wakeup()
{
    sleepFlag=false;
    simtime_t wakeTime = par("scheduleShare");
    scheduleAt(simTime()+wakeTime, goToSleepMsg);
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

