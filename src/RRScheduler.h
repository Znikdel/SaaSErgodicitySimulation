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

#ifndef __ERGODICITYTEST_RRSCHEDULER_H_
#define __ERGODICITYTEST_RRSCHEDULER_H_

#include <omnetpp.h>
#include "IServer.h"

using namespace queueing;

namespace ergodicitytest {

class tryQStrategy;

/**
 * The queue server. It cooperates with several Queues that which queue up
 * the jobs, and send them to Server on request.
 *
 * @see PassiveQueue
 */
class QUEUEING_API RRScheduler : public cSimpleModule, public IServer
{
    private:
        simsignal_t busySignal;
        bool allocated;
        bool sleepFlag;
        bool schedulerFlag;
     //   simtime_t serviceTime=par("serviceTime");
        tryQStrategy *selectionStrategy;
        simtime_t wakeUpTime;
        simtime_t shortDelay;

        cMessage *jobServiced;
        cMessage *endServiceMsg;
        cMessage *goToSleepMsg;
        cMessage *wakeUpMsg;

    public:
        RRScheduler();
        virtual ~RRScheduler();

    protected:
        virtual void initialize() override;
        virtual int numInitStages() const override {return 2;}
        virtual void handleMessage(cMessage *msg) override;
        virtual void refreshDisplay() const override;
        virtual void finish() override;
        void sleep();
        void wakeup();


    public:
        virtual bool isIdle() override;
        virtual void allocate() override;
};

}; //namespace

#endif

