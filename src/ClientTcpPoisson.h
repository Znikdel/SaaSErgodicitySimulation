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

#ifndef __ERGODICITYTEST_CLIENTTCPPOISSON_H_
#define __ERGODICITYTEST_CLIENTTCPPOISSON_H_

#include <omnetpp.h>

#include "inet/common/INETDefs.h"
#include "inet/applications/tcpapp/TcpAppBase.h"
#include "inet/common/lifecycle/ILifecycle.h"
#include "inet/common/lifecycle/NodeStatus.h"


using namespace inet;

namespace ergodicitytest {

/**
 * TODO - Generated class
 */
class INET_API ClientTcpPoisson : public TcpAppBase

/**
 * An example request-reply based client application.
 */

{
  protected:
    cMessage *timeoutMsg = nullptr;
    bool earlySend = false;    // if true, don't wait with sendRequest() until established()
    bool replyFlag=true;
    bool socketClosedFlag=false;
    int numRequestsToSend = 0;    // requests to send in this session
    int numRequestsToRecieve = 0;
    int replyCount=0;
    simtime_t startTime;
    simtime_t stopTime;
    simtime_t sendReqTime;
    simtime_t getReplyTime;
    simtime_t respTime;
    simtime_t lastReplyTime;
    simtime_t sendInternalReqTime;
    int replyTimeMax;
    cOutVector respTimeVector;
    simsignal_t respTimeSignal;

    cOutVector failedReqVector;
    simsignal_t failedReqSignal;

    cOutVector failedReqNoSendVector;
    simsignal_t failedReqNoSendSignal;

    bool LOCALLY_CLOSED_flag=false;
  //  TcpSocket mySocket;
    int repeated_req=0;
 //   simsignal_t numReqRecSignal;
 //   simsignal_t numReqSentSignal;
  //  simsignal_t numLostReqSignal;

   // long numReqSent=0;
   // long numReqRecieved=0;
  //  Packet *numLostReq;
  //  Packet *numReqSent;
  //  Packet *numReqRecieved;
    Packet *last_packet;

    void replyTimeOut();
    void TimeOutSocketClosed();

    virtual void sendRequest();
    virtual void rescheduleOrDeleteTimer(simtime_t d, short int msgKind);

    virtual int numInitStages() const override { return NUM_INIT_STAGES; }
    virtual void initialize(int stage) override;
    virtual void handleTimer(cMessage *msg) override;
    virtual void socketEstablished(TcpSocket *socket) override;
    virtual void socketDataArrived(TcpSocket *socket, Packet *msg, bool urgent) override;
    virtual void socketClosed(TcpSocket *socket) override;
    virtual void socketFailure(TcpSocket *socket, int code) override;

    virtual void handleStartOperation(LifecycleOperation *operation) override;
    virtual void handleStopOperation(LifecycleOperation *operation) override;
    virtual void handleCrashOperation(LifecycleOperation *operation) override;

    virtual void close() override;

  public:
    ClientTcpPoisson() {}
    virtual ~ClientTcpPoisson();
};

} // namespace ergodicitytest

#endif // ifndef ___ERGODICITYTEST_CLIENTTCPPOISSON_H_

