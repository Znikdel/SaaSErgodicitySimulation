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

#ifndef __ErgodicityTest_TRYSERVER_H_
#define __ErgodicityTest_TRYSERVER_H_

#include <omnetpp.h>

#include "inet/common/packet/ChunkQueue.h"
#include "inet/transportlayer/contract/tcp/TcpSocket.h"
#include "inet/common/lifecycle/LifecycleUnsupported.h"

using namespace inet;


namespace ErgodicityTest {

/**
 * TODO - Generated class
 */
class INET_API tryServer : public cSimpleModule, public LifecycleUnsupported
{

  protected:
    TcpSocket socket;
    simtime_t delay;
    simtime_t maxMsgDelay;

    long msgsRcvd;
    long msgsSent;
    long bytesRcvd;
    long bytesSent;

    //cOutVector serviceTimeVector;

    std::map<int, ChunkQueue> socketQueue;

  protected:
    virtual void sendBack(cMessage *msg);
    virtual void sendOrSchedule(cMessage *msg, simtime_t delay);

    virtual void initialize(int stage) override;
    virtual int numInitStages() const override { return NUM_INIT_STAGES; }
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;
    virtual void refreshDisplay() const override;
};

} // namespace ergodicity test

#endif // ifndef _ErgodicityTest_TRYSERVER_H_

