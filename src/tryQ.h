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

#ifndef __ERGODICITYTEST_TRYQ_H_
#define __ERGODICITYTEST_TRYQ_H_

#include <omnetpp.h>
#include "QueueingDefs.h"
#include "IPassiveQueue.h"
//#include "SelectionStrategies.h"
#include "tryQStrategy.h"

using namespace queueing;

namespace ergodicitytest {

//class tryQStrategy;

/**
 * TODO - Generated class
 */
class QUEUEING_API TryQ : public cSimpleModule, public IPassiveQueue
{
     private:
        simsignal_t droppedSignal;
        simsignal_t receivedSignal;
        simsignal_t queueLengthSignal;
        simsignal_t queueingTimeSignal;

        bool fifo;
        int capacity;
        cQueue queue;
        tryQStrategy *selectionStrategy;

        simtime_t delay;

        void queueLengthChanged();
        void sendJob(cMessage *msg, int gateIndex);

    protected:
        virtual void initialize() override;
        virtual void handleMessage(cMessage *msg) override;
        virtual void refreshDisplay() const override;

    public:
        TryQ();
        virtual ~TryQ();
        // The following methods are called from IServer:
        virtual int length() override;
        virtual void request(int gateIndex) override;
};

}; //namespace

#endif
