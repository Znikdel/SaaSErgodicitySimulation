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

#include "ClientTcpPoisson.h"

#include "inet/applications/tcpapp/GenericAppMsg_m.h"
#include "inet/common/ModuleAccess.h"
#include "inet/common/lifecycle/ModuleOperations.h"
#include "inet/common/packet/Packet.h"
#include "inet/common/TimeTag_m.h"

namespace ErgodicityTest {


#define MSGKIND_CONNECT            0
#define MSGKIND_SEND               1
#define MSGKIND_REPLYTIMEOUT       2
#define MSGKIND_CLOSE              3
#define MSGKIND_BURST_START        4
#define MSGKIND_BURST_FIN          5
#define MSGKIND_SEND_REPEAT        6
#define MSGKIND_CONNECT_BURSTY     7
#define MSGKIND_RTOS               8

Define_Module(ClientTcpPoisson);

ClientTcpPoisson::~ClientTcpPoisson()
{
    cancelAndDelete(timeoutMsg);
}

void ClientTcpPoisson::initialize(int stage)
{
    TcpAppBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        mainSocketID=socket.getSocketId();
        numRequestsToSend = 0;
        numRequestsToRecieve=0;
        replyCount=0;

        lastReplyTime=simTime();
        sendInternalReqTime=simTime();
        earlySend = false;    // TBD make it parameter
        connected=false;
    //    WATCH(numRequestsToSend);
    //    WATCH(earlySend);
        sendReqTime=simTime();
        getReplyTime=simTime();
        respTime=0;
        respTimeVector.setName("ResponseTime");
        respTimeSignal=registerSignal("respTime");

        failedReqVector.setName("FailedRequest");
        failedReqSignal=registerSignal("failedReq");
        failedReqNoSendVector.setName("FailedNoSend");
        failedReqNoSendSignal=registerSignal("failedNoReq");

        startTime = par("startTime");
        stopTime = par("stopTime");
        burstLen = par("burstLen");
        startBurst= par("startBurst");
        if (stopTime >= SIMTIME_ZERO && stopTime < startTime)
            throw cRuntimeError("Invalid startTime/stopTime parameters");

        timeoutMsg = new cMessage("timer");
        reliabletimeoutMsg=new cMessage("timer");
        bursttimeoutMsg = new cMessage("timer");

        replyTimeMax=16;
        repeated_req=0;
        failed_req=0;  //for non-reliable protocol
        reliableProtocol=par("reliableProtocol");
        burstyTraffic=par("burstyTraffic");
        RTOS=par("RTOS");
        burstStarted=false;
        burst_finished=0;

        RTOS_hard_limit=8;   //with RTOS reliable should be false
    }
}

void ClientTcpPoisson::handleStartOperation(LifecycleOperation *operation)
{
//    if (this->getFullPath()=="PretioWithLB.client[0].app[0]")
//        std::cout<<"  In  handleStartOperation: "<<
//       this->getId()<<"   ,   "<< this->getFullPath()<< "   Socket Id: "<< socket.getSocketId()<<"at:   "<<simTime()<<"mainSocketID: "<< mainSocketID<<endl;
    simtime_t now = simTime();
    if (!burstyTraffic)
    {
        simtime_t start = std::max(startTime, now);
        if (timeoutMsg && ((stopTime < SIMTIME_ZERO) || (start < stopTime) || (start == stopTime && startTime == stopTime)))
        {
            timeoutMsg->setKind(MSGKIND_CONNECT);
            scheduleAt(start, timeoutMsg);
        }
    }
    else
    {
        simtime_t start = std::max(startBurst, now);
        stopTime =  start + burstLen;
        startTime=start;

        if (timeoutMsg && ((stopTime < SIMTIME_ZERO) || (start < stopTime) || (start == stopTime && startTime == stopTime))) {
            if (this->getFullPath()=="PretioWithLB.client[0].app[0]")
                std::cout<<"stopTime in beginning :"<<stopTime<<endl;
            bursttimeoutMsg->setKind(MSGKIND_BURST_START);
            scheduleAt(start, bursttimeoutMsg);
            }
    }
}

void ClientTcpPoisson::handleStopOperation(LifecycleOperation *operation)
{
    if(timeoutMsg)
        cancelEvent(timeoutMsg);
    if (socket.getState() == TcpSocket::CONNECTED || socket.getState() == TcpSocket::CONNECTING || socket.getState() == TcpSocket::PEER_CLOSED)
        socket.destroy();
}

void ClientTcpPoisson::handleCrashOperation(LifecycleOperation *operation)
{
    EV_WARN << "handle crash operation in client";
    if(timeoutMsg)
        cancelEvent(timeoutMsg);
    if (operation->getRootModule() != getContainingNode(this))
        socket.destroy();
}

void ClientTcpPoisson::sendRequest()

{
//        if (this->getFullPath()=="PretioWithLB.client[0].app[0]")
//            std::cout<<"  In  send Request: "<< "at:   "<<simTime()<<
//           "   ,    Socket Id---> "<< socket.getSocketId()<<endl;

    if (socket.getSocketId()!=mainSocketID)
    {
        mainSocketID=socket.getSocketId();
        sendReqTime=simTime();
    }

    long requestLength = par("requestLength");
    long replyLength = par("replyLength");
    if (requestLength < 1)
        requestLength = 1;
    if (replyLength < 1)
        replyLength = 1;

    const auto& payload = makeShared<GenericAppMsg>();

        Packet *packet = new Packet("data");
        payload->setChunkLength(B(requestLength));
        payload->setExpectedReplyLength(B(replyLength));
        payload->setServerClose(false);
        payload->addTag<CreationTimeTag>()->setCreationTime(simTime());
        packet->insertAtBack(payload);

        EV_INFO << "sending request with " << requestLength << " bytes, expected reply length " << replyLength << " bytes,"
                << "remaining " << numRequestsToSend - 1 << " request\n";

        if (socket.getState() == TcpSocket::CONNECTED || socket.getState() == TcpSocket::CONNECTING )
                  {
//            if (this->getFullPath()=="PretioWithLB.client[0].app[0]")
//                                                           std::cout<<"In send Packet - Socket Connected at: "<<simTime()<<endl;
                      sendPacket(packet);
                      sendInternalReqTime=simTime();
                      simtime_t d = simTime() + replyTimeMax;
                      if (reliabletimeoutMsg)
                          cancelEvent(reliabletimeoutMsg);  // We got the reply, so no need to check for reply time out
                      reliabletimeoutMsg->setKind(MSGKIND_REPLYTIMEOUT);
                      scheduleAt(d, reliabletimeoutMsg);  //here we check if we got reply in time
                      if(RTOS)
                      {
//                          if (this->getFullPath()=="PretioWithLB.client[0].app[0]")
//                                                std::cout<<"In send Packet - MSGKIND_RTOS at: "<<simTime()<<endl;
                          d=simTime() + RTOS_hard_limit;
                          rescheduleOrDeleteTimer(d, MSGKIND_RTOS);
                      }
                  }
            else
            {
                    simtime_t d = simTime();
                    std::cout<<"Socket is not connected$$$$$$$$$$$$$$$$$$$$$$$$$$:"<<socket.getSocketId()<<"   state:   "<<socket.getState()<<simTime()<< "      for:"<<d<<endl;

                    rescheduleOrDeleteTimer(d, MSGKIND_CLOSE);

            }
}

void ClientTcpPoisson::handleTimer(cMessage *msg)
{
//    if (this->getFullPath()=="PretioWithLB.client[0].app[0]")
//        std::cout<<"In handle Timer"<<endl;
    simtime_t d=simTime();
    //std::cout<<"In handle Timer at:"<<d<<    "    stopTime:    "   <<  stopTime<< ",  bt: "<< burstyTraffic <<"startTime:    "<<startTime<<  endl;
    if (((stopTime < SIMTIME_ZERO || d < stopTime) && d>=startTime) and burstyTraffic==false)   //NOT BURSTY
        {
//        if (this->getFullPath()=="PretioWithLB.client[0].app[0]")
//               std::cout<<"In not bursty"<<endl;

            switch (msg->getKind()) {

            case MSGKIND_CONNECT:{
                           // significance of earlySend: if true, data will be sent already
                           // in the ACK of SYN, otherwise only in a separate packet (but still
                           // immediately)
                if(connected==false)   // connected changes to false when we have too many (8)dropped messages
                    {
                    connect();
                    connected=true;
                    mainSocketID=socket.getSocketId();
                    firstConnection=false;

                    }

                mainSocketID=socket.getSocketId();
                sendReqTime=simTime();  // The request initiated by user
                if (earlySend)
                    sendRequest(); //sendInternalRequestTime will be adjusted in this function


                socketClosedFlag=false;
                socketEstablished(&socket); // number of needed middle servers specified here

                break;

        }
            case MSGKIND_SEND:{
                if (socket.getState() == TcpSocket::CONNECTED || socket.getState() == TcpSocket::CONNECTING)
                {
                    sendRequest();  //sendInternalRequestTime will be adjusted in this function
                    numRequestsToSend--;
                }
                else
                {
                    rescheduleOrDeleteTimer(d, MSGKIND_CLOSE);
                    std::cout<<"Socket NOT connected in handleTimer -> MSGKIND_SEND:       "<< socket.getLocalAddress() << "Socket ID:"<< socket.getSocketId()<< "   Port Number:"<<socket.getLocalPort()<<endl;
                }
                break;
            }
            case MSGKIND_RTOS:{
//                if (this->getFullPath()=="PretioWithLB.client[0].app[0]")
//                        std::cout<<"In handle Timer - MSGKIND_RTOS at: "<<simTime()<<endl;
                long requestLength = par("requestLength");
                    long replyLength = par("replyLength");
                    if (requestLength < 1)
                        requestLength = 1;
                    if (replyLength < 1)
                        replyLength = 1;

                    const auto& payload = makeShared<GenericAppMsg>();
                    Packet *packet = new Packet("data");
                        payload->setChunkLength(B(requestLength));
                        payload->setExpectedReplyLength(B(replyLength));
                        payload->setServerClose(false);
                        payload->addTag<CreationTimeTag>()->setCreationTime(simTime());
                        packet->insertAtBack(payload);
                socketDataArrived(&socket,packet,false);

                break;
            }
            case MSGKIND_REPLYTIMEOUT:{
             //
                simtime_t now_reply = simTime()-lastReplyTime;

                if (now_reply >= replyTimeMax )
                {
                    EV_WARN << "didn't get reply from the server, re-sending a request," << "remaining " << numRequestsToSend  << " request\n";

                    if (reliableProtocol==false )
                    {
//                        if (this->getFullPath()=="PretioWithLB.client[0].app[1]")
//                           std::cout<<"  In  MSGKIND_REPLYTIMEOUT: "<<
//                           this->getId()<<"   ,   "<< this->getFullPath()<< "   Socket Id: "<<
//                           socket.getSocketId()<<"at:   "<<simTime()<< " mainSocketID: "<< mainSocketID<<endl<<
//                           "failed req:      " << failed_req <<endl<<
//                           "sendReqTime:     "  << sendReqTime <<  endl;

                        simtime_t d = simTime()-sendReqTime; //sendReqTime is the customer connect time

                        failedReqVector.record(d);
                        emit(failedReqSignal,d);

                        failedReqNoSendVector.record(numRequestsToSend);
                        emit(failedReqNoSendSignal,numRequestsToSend);

                        if (failed_req>7)
                          {
                               failed_req=0;  // check if too many failed messages maybe the socket has problem so re-establish the connection
                               connected=false;
                               d = simTime()+ (rand()%10)*0.001;  // add random milliseconds to give the queue some time to change
                               rescheduleOrDeleteTimer(d, MSGKIND_CLOSE);
                          }
                  //         lastReplyTime=simTime();

                        failed_req++;

                        d = simTime() + replyTimeMax;

                        cancelEvent(reliabletimeoutMsg);  // We got the reply, so no need to check for reply time out
                        reliabletimeoutMsg->setKind(MSGKIND_REPLYTIMEOUT);
                        scheduleAt(d, reliabletimeoutMsg);  //here we check if we got reply in time

                    }
                    else{  // reliable protocol
                    if (timeoutMsg and (repeated_req>8))
                    {
                        simtime_t d = simTime()-sendReqTime; //sendReqTime is the connect time
                        failedReqVector.record(d);
                        emit(failedReqSignal,d);
                        failedReqNoSendVector.record(numRequestsToSend);
                        emit(failedReqNoSendSignal,numRequestsToSend);
                        d = simTime();
                        connected=false;
                        rescheduleOrDeleteTimer(d, MSGKIND_CLOSE);
                    }
                    else if (timeoutMsg) {   //re-sending the request
                                numRequestsToSend++;
                                simtime_t d = simTime()-sendReqTime; //sendReqTime is the connect time
                                d = simTime()+(rand()%10)*0.001;

                                ++repeated_req;
                                rescheduleOrDeleteTimer(d, MSGKIND_SEND);
                            }
                    else
                    {
                        failedReqNoSendVector.record(numRequestsToSend);
                        emit(failedReqNoSendSignal,numRequestsToSend);
                        simtime_t d = simTime();
                        rescheduleOrDeleteTimer(d, MSGKIND_CLOSE);
                    }
                    }
                }
                break;
            }
            case MSGKIND_CLOSE:{

                close();

                break;
            }

            default:
                    throw cRuntimeError("Invalid timer msg: kind=%d", msg->getKind());
            }
    }
    else   // Bursty Traffic
    {
        switch (msg->getKind()) {

        case MSGKIND_BURST_FIN:
                            {
//                            if (this->getFullPath()=="PretioWithLB.client[0].app[0]")
//                                std::cout<<"Burst finished at:"<<stopTime<<endl;
                            burstStarted=false;
                            startTime = stopTime + burstLen;
                            stopTime =  startTime + burstLen;

                            if (startTime < stopTime) {

//                                if (this->getFullPath()=="PretioWithLB.client[0].app[0]")
//                                    std::cout<<"iN BURST FINISHED : New start Time at:"<<startTime<<endl;

                                if(bursttimeoutMsg)
                                    cancelEvent(bursttimeoutMsg);
                                bursttimeoutMsg->setKind(MSGKIND_BURST_START);

                                scheduleAt(startTime, bursttimeoutMsg);// why -1?
                                break;
                            }
                    case MSGKIND_BURST_START:
                                        {
//                                        if (this->getFullPath()=="PretioWithLB.client[0].app[0]")
//                                            std::cout<<"IN BURST START ------>  Burst start at:"<<simTime()<<endl;
                                        burstStarted=true;
                                   //     startTime = stopTime + burstLen;
                                        stopTime =  startTime + burstLen;
                                        burst_finished=stopTime;
                                        numRequestsToSend = 0;
                                        numRequestsToRecieve=0;
                                        replyCount=0;
                                        lastReplyTime=startTime;
                                        sendInternalReqTime=startTime;

                                        repeated_req=0;
                                        respTime=0;

                                        sendReqTime=startTime;
                                        getReplyTime=startTime;

                                        if (startTime < stopTime) {
//                                            if (this->getFullPath()=="PretioWithLB.client[0].app[0]")
//                                                std::cout<<"Start Time at:"<<startTime<<endl;
                                            rescheduleOrDeleteTimer(startTime+1, MSGKIND_CONNECT_BURSTY);
                                              }
                                    //    cMessage *timeoutMsg = nullptr;
                                    //    timeoutMsg = new cMessage("timer");
                                        if(bursttimeoutMsg)
                                            cancelEvent(bursttimeoutMsg);

                                        bursttimeoutMsg->setKind(MSGKIND_BURST_FIN);
                                        scheduleAt(stopTime, bursttimeoutMsg);

                                        break;
                                        }
                    case MSGKIND_CONNECT_BURSTY:{
                                               // significance of earlySend: if true, data will be sent already
                                               // in the ACK of SYN, otherwise only in a separate packet (but still
                                               // immediately)
//                        if (this->getFullPath()=="PretioWithLB.client[0].app[0]")
//                                std::cout<<"MSGKIND_CONNECT_BURSTY at time:"<<simTime()<<endl;

                                 if(connected==false)   // connected changes to false when we have too many (8)dropped messages
                                        {
                                        connect();
                                        connected=true;
                                        mainSocketID=socket.getSocketId();
                                        firstConnection=false;
                                        }

                                    mainSocketID=socket.getSocketId();
                                    sendReqTime=simTime();  // The request initiated by user
                                    if (earlySend)
                                        sendRequest(); //sendInternalRequestTime will be adjusted in this function


                                    socketClosedFlag=false;
                                    socketEstablished(&socket); // number of needed middle servers specified here
//                                    if (this->getFullPath()=="PretioWithLB.client[0].app[0]")
//                                          std::cout<<"AFTER SOCKET ESTABLISHED -----> MSGKIND_CONNECT_BURSTY at time:"<<simTime()<<endl;
                                    break;

                            }
                    case MSGKIND_REPLYTIMEOUT:{
//                                    if (this->getFullPath()=="PretioWithLB.client[0].app[0]")
//                                                                  std::cout<<"MSGKIND_REPLYTIMEOUT at time:"<<simTime()<<endl;
                                   simtime_t now_reply = simTime()-lastReplyTime;
                                   if (now_reply >= replyTimeMax )
                                   {
                                       EV_WARN << "didn't get reply from the server, re-sending a request," << "remaining " << numRequestsToSend  << " request\n";
//                                       if (this->getFullPath()=="PretioWithLB.client[0].app[0]")
//                                           std::cout << "didn't get reply from the server, re-sending a request," << "remaining " << numRequestsToSend  << " request\n"<<endl;

                                       if (reliableProtocol==false )
                                       {
                                           //                        if (this->getFullPath()=="PretioWithLB.client[0].app[1]")
                                           //                           std::cout<<"  In  MSGKIND_REPLYTIMEOUT: "<<
                                           //                           this->getId()<<"   ,   "<< this->getFullPath()<< "   Socket Id: "<<
                                           //                           socket.getSocketId()<<"at:   "<<simTime()<< " mainSocketID: "<< mainSocketID<<endl<<
                                           //                           "failed req:      " << failed_req <<endl<<
                                           //                           "sendReqTime:     "  << sendReqTime <<  endl;
                                           if (failed_req>7)
                                            {
                                                 failed_req=0;  // check if too many failed messages maybe the socket has problem so re-establish the connection
                                                 connected=false;
                                                 TimeOutSocketClosed();
                                            }

                                           lastReplyTime=simTime();
                                           failed_req++;

                                           simtime_t d = simTime()-sendReqTime; //sendReqTime is the connect time
                                           failedReqVector.record(d);
                                           emit(failedReqSignal,d);

                                           d = simTime()+replyTimeMax;

                                           if (reliabletimeoutMsg)
                                               cancelEvent(reliabletimeoutMsg);  // We got the reply, so no need to check for reply time out
                                           reliabletimeoutMsg->setKind(MSGKIND_REPLYTIMEOUT);
                                           scheduleAt(d, reliabletimeoutMsg);  //here we check if we got reply in time

                                       }
                                       else{
//                                           if (this->getFullPath()=="PretioWithLB.client[0].app[0]")
//                                               std::cout<<"In reliable Protocol"<<endl;
                                       if (timeoutMsg and repeated_req>8)
                                       {
//                                           if (this->getFullPath()=="PretioWithLB.client[0].app[0]")
//                                               std::cout<< this->getFullPath() <<"repeated_req MORE THAN at:"<<simTime()<<endl;
                                           simtime_t d = simTime()-sendReqTime; //sendReqTime is the connect time
                                           failedReqVector.record(d);
                                           emit(failedReqSignal,d);
                                           respTimeVector.record(d);
                                           emit(respTimeSignal,d);

                                           d = simTime();
                                        //   rescheduleOrDeleteTimer(d, MSGKIND_CLOSE);
                                       }
                                       else if (timeoutMsg) {
                                                 //  cancelEvent(timeoutMsg);
                                                   numRequestsToSend++;
                                                   simtime_t d = simTime()-sendReqTime; //sendReqTime is the connect time
                                            //       failedReqVector.record(d);
                                           //        emit(failedReqSignal,d);
                                                   d = simTime();

                                     //              std::cout<< this->getFullPath() <<"     in MSGKIND_REPLYTIMEOUT       at:   "<< simTime() <<"   numRequestsToSend:     "<< numRequestsToSend<<endl;

                                                   ++repeated_req;
                                            //      std::cout <<"repeated_req:"<<repeated_req<<endl;

                                                   rescheduleOrDeleteTimer(d, MSGKIND_SEND_REPEAT);
                                               }
                                       else
                                       {
                                           failedReqNoSendVector.record(numRequestsToSend);
                                           emit(failedReqNoSendSignal,numRequestsToSend);
                                        //   simtime_t d = simTime();
                                          // rescheduleOrDeleteTimer(d, MSGKIND_CLOSE);

                                       }
                                       }
                                   }
                                   break;
                               }
                    case MSGKIND_SEND_REPEAT:
                    {
                        if (socket.getState() == TcpSocket::CONNECTED || socket.getState() == TcpSocket::CONNECTING)
                        {
//                            if (this->getFullPath()=="PretioWithLB.client[0].app[0]")
//                                std::cout<<"In   MSGKIND_SEND_REPEAT at:"<< simTime()<<"numRequestsToSend:"<<numRequestsToSend<<endl;


                            sendRequest();  //sendInternalRequestTime will be adjusted in this function
                            numRequestsToSend--;
                      }
                        else
                        {
              //              std::cout<<"Socket NOT connected in handleTimer -> MSGKIND_SEND_REPEAT:       "<< socket.getLocalAddress() << "Socket ID:"<< socket.getSocketId()<< "   Port Number:"<<socket.getLocalPort()<<endl;
                            connect();
                            sendRequest();  //sendInternalRequestTime will be adjusted in this function
                            numRequestsToSend--;
            //                std::cout<<"in handleTimer -> MSGKIND_SEND:       "<< socket.getLocalAddress() << "Socket ID:"<< socket.getSocketId()<< "   Port Number:"<<socket.getLocalPort()<<endl;
                        }
                    }
                        break;
                    }
                   default:
                   {
                       if(timeoutMsg)
                           cancelEvent(timeoutMsg);
                   }
    }
    }

}

void ClientTcpPoisson::socketEstablished(TcpSocket *socket)
{
    socketClosedFlag=false;
    TcpAppBase::socketEstablished(socket);
  //  mySocket=&socket;

    // determine number of requests in this session
    numRequestsToSend = par("numRequestsPerSession");

    if (numRequestsToSend < 1)
        numRequestsToSend = 1;
//    if (this->getFullPath()=="PretioWithLB.client[0].app[0]")
//            std::cout<<"In socketEstablished at:  "<< simTime()<<"    numRequestsToSend----->"<<numRequestsToSend<<endl;
    // perform first request if not already done (next one will be sent when reply arrives)
    if (!earlySend)
    {
        sendRequest();
        numRequestsToSend--;

    }
    numRequestsToRecieve=numRequestsToSend; // Why Here?

}

void ClientTcpPoisson::rescheduleOrDeleteTimer(simtime_t d, short int msgKind)
{
//    if (this->getFullPath()=="PretioWithLB.client[0].app[0]")
//           std::cout<<"In rescheduleOrDeleteTimer old msg:"<<timeoutMsg->getKind()<<  "     at:"<< simTime()<<"    new msg::"<<   msgKind<<endl;
    if(timeoutMsg)
        cancelEvent(timeoutMsg);

    timeoutMsg->setKind(msgKind);
    scheduleAt(d, timeoutMsg);

}

void ClientTcpPoisson::socketDataArrived(TcpSocket *socket, Packet *msg, bool urgent)
{
    cancelEvent(reliabletimeoutMsg);
    lastReplyTime=simTime();
    repeated_req=0;
    TcpAppBase::socketDataArrived(socket, msg, urgent);
    if (numRequestsToSend > 0) {

        simtime_t myRespTime = simTime()-sendInternalReqTime;
//        if (this->getFullPath()=="PretioWithLB.client[0].app[0]")
//                                    std::cout<<"/////////////////////////////   reply arrived at:    "<<simTime()<<"  ,  myRespTime:"<<myRespTime<<endl;
        EV_INFO << "reply arrived\n";
        replyCount++;
        numRequestsToRecieve--;
        failed_req=0;

//        if (this->getFullPath()=="PretioWithLB.client[0].app[0]")
//                    std::cout<<"numRequestsToSend ------>   "<<numRequestsToSend<<endl;
        if (timeoutMsg) {
            simtime_t d = simTime() + par("thinkTime");
            if(!burstyTraffic)
                rescheduleOrDeleteTimer(d, MSGKIND_SEND);
            else
                rescheduleOrDeleteTimer(d, MSGKIND_SEND_REPEAT);
        }
    }
    else  //There is no request left to send
        if (socketClosedFlag==false)
        {


            replyCount++;
            failed_req=0;
           // cancelEvent(reliabletimeoutMsg);
            EV_INFO << "reply to last request arrived, closing session\n";
            getReplyTime=simTime();
            respTime=getReplyTime-sendReqTime;  //it's actually session time
            respTimeVector.record(respTime);
            emit(respTimeSignal,respTime); //response time to all small requests, kind of demonstrates the server markov chain
            simtime_t d = simTime();
//            if (this->getFullPath()=="PretioWithLB.client[0].app[0]")
//                               std::cout<<"///////////////////////////// LAst  socketDataArrived    ////////////////   at   ---->"<<simTime()<<endl
//                               <<"resp time: ---->"<<respTime<<endl;
            if (!burstyTraffic)
                rescheduleOrDeleteTimer(d, MSGKIND_CLOSE);
            else
                TimeOutSocketClosed();
        }

        else {

                   failedReqNoSendVector.record(replyCount);
                   emit(failedReqNoSendSignal,replyCount);

                   simtime_t d = simTime();
                   rescheduleOrDeleteTimer(d, MSGKIND_CLOSE);

                    }
    }
void ClientTcpPoisson::close()
{
    socketClosedFlag=true;

    numRequestsToSend = 0;
    numRequestsToRecieve=0;
    replyCount=0;
    repeated_req=0;
    respTime=0;
    lastReplyTime=simTime();
    sendInternalReqTime=simTime();
    simtime_t d = simTime() + par("idleInterval");
    sendReqTime=d;
    getReplyTime=d;
    if (stopTime < SIMTIME_ZERO || d < stopTime) {  //we don't call close in bursts
        if (timeoutMsg) {
            rescheduleOrDeleteTimer(d, MSGKIND_CONNECT);
        }
     }
    else
    {
        if (socket.getState() == TcpSocket::CONNECTED || socket.getState() == TcpSocket::CONNECTING || socket.getState() == TcpSocket::PEER_CLOSED)
        {

           socket.destroy();
           TcpAppBase::close();
        }
    }
   //
}
void ClientTcpPoisson::TimeOutSocketClosed()
{
//    if (this->getFullPath()=="PretioWithLB.client[0].app[0]")
//                      std::cout<<" TimeOutSocketClosed    at:    "<<simTime()<<endl;
    numRequestsToSend = 0;
    numRequestsToRecieve=0;
    replyCount=0;
    lastReplyTime=simTime();
    sendInternalReqTime=simTime();

    repeated_req=0;
    respTime=0;

    sendReqTime=simTime();
    getReplyTime=simTime();
    simtime_t d = simTime() + par("idleInterval");

    if (stopTime < SIMTIME_ZERO || d < burst_finished) {
        if (timeoutMsg)
            rescheduleOrDeleteTimer(d, MSGKIND_CONNECT_BURSTY);
    }
}
void ClientTcpPoisson::socketClosed(TcpSocket *socket)  //no need for this function
{
    socketClosedFlag=true;
//    TcpAppBase::finish();
//    TcpAppBase::close();
}

void ClientTcpPoisson::socketFailure(TcpSocket *socket, int code)
{

    std::cout<<"   In socketFailure  "<< this->getFullPath()<<"    state:           "<<socket->getState()<<endl;
    TcpAppBase::socketFailure(socket, code);
    // reconnect after a delay
    simtime_t d = simTime() ;
    if (stopTime < SIMTIME_ZERO || d < stopTime)
    {
        connect();
        connected=true;
        mainSocketID=socket->getSocketId();
        firstConnection=false;
    }
    sendReqTime=simTime();  // The request initiated by user
    if (earlySend)
        sendRequest(); //sendInternalRequestTime will be adjusted in this function


    socketClosedFlag=false;
    socketEstablished(socket); // number of needed middle servers specified here

}
} // namespace ErgodicityTest

