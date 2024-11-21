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

#ifndef __ErgodicityTest_ErgodicityTestClient_H_
#define __ErgodicityTest_ErgodicityTestClient_H_

#include <omnetpp.h>

#include "inet/applications/tcpapp/TcpAppBase.h"
#include "inet/common/lifecycle/ILifecycle.h"
#include "inet/common/lifecycle/NodeStatus.h"
#include "inet/common/INETDefs.h"
#include "inet/common/packet/chunk/FieldsChunk.h"

using namespace inet;

namespace ErgodicityTest {

/**
 * TODO - Generated class
 */
class INET_API ErgodicityTestClient : public TcpAppBase

/**
 * An example request-reply based client application.
 */

{
  protected:
    cMessage *timeoutMsg = nullptr;
    cMessage *reliabletimeoutMsg = nullptr;
    cMessage *bursttimeoutMsg = nullptr;
    cMessage *rtostimeoutMsg = nullptr;

    bool earlySend = false;    // if true, don't wait with sendRequest() until established()
    bool replyFlag=true;
    bool socketClosedFlag=false;
    bool connected=false;
    bool reliableProtocol=true;
    bool burstyTraffic=false;
    bool burstStarted=false;
    bool randomBurst=false;
    bool firstConnection=true;
    bool RTOS=false;
    bool dynamicScaling=false;
    bool dynamicScalingAdd=false;

    int numRequestsToSend = 0;    // requests to send in this session
    int numRequestsToRecieve = 0;
    int replyCount=0;
    int mainSocketID=0;
    int failed_req=0;
    int RTOS_hard_limit=0;

    simtime_t startBurst;
    simtime_t startTime;
    simtime_t stopTime;
    simtime_t burst_finished;
    simtime_t sendReqTime;
    simtime_t getReplyTime;
    simtime_t respTime;
    simtime_t lastReplyTime;
    simtime_t sendInternalReqTime;

    int replyTimeMax;
    int burstLen = 0;
    int portCounter=0;

    cOutVector respTimeVector;
    simsignal_t respTimeSignal;

    cOutVector failedReqVector;
    simsignal_t failedReqSignal;

    cOutVector failedReqNoSendVector;
    simsignal_t failedReqNoSendSignal;

    simsignal_t newContainerSignal;

    bool LOCALLY_CLOSED_flag=false;
  //  TcpSocket mySocket;
    int repeated_req=0;
    int connectPort = 0;
 //   int localPort = 0;
    Packet *last_packet;
    //auto sequenceNumber;

    void TimeOutSocketClosed();



    virtual int numInitStages() const override { return NUM_INIT_STAGES; }
    virtual void initialize(int stage) override;

    virtual void connect() override;
    virtual void socketEstablished(TcpSocket *socket) override;
    virtual void socketDataArrived(TcpSocket *socket, Packet *msg, bool urgent) override;
    virtual void socketClosed(TcpSocket *socket) override;
    virtual void socketFailure(TcpSocket *socket, int code) override;

    virtual void handleStartOperation(LifecycleOperation *operation) override;
    virtual void handleStopOperation(LifecycleOperation *operation) override;
    virtual void handleCrashOperation(LifecycleOperation *operation) override;
    virtual void handleTimer(cMessage *msg) override;

    Packet* makePacket(bool resend);
    virtual void sendRequest(bool resend);
    virtual void rescheduleOrDeleteTimer(simtime_t d, short int msgKind);

    void msg_Connect(cMessage *msg);
    void msg_Send(cMessage *msg, bool resend);
    void msg_ReplyTimeOut(cMessage *msg);

    void msg_Burst_Start(cMessage *msg);
    void msg_Burst_Finish(cMessage *msg);

    void msg_RTOS(cMessage *msg);


    virtual void close() override;

  public:
    ErgodicityTestClient() {}
    virtual ~ErgodicityTestClient();
};

} // namespace ErgodicityTest

#endif // ifndef ___ErgodicityTest_ErgodicityTestClient_H_

