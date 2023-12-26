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

Define_Module(ClientTcpPoisson);

ClientTcpPoisson::~ClientTcpPoisson()
{
    cancelAndDelete(timeoutMsg);
}

void ClientTcpPoisson::initialize(int stage)
{
    TcpAppBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
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

    //    numReqSentSignal=registerSignal("numReqSent");
     //   numReqRecSignal=registerSignal("numReqRecieved");
     //   numLostReqSignal=registerSignal("numLostReq");
        startTime = par("startTime");
        stopTime = par("stopTime");
        burstLen = par("burstLen");
        startBurst= par("startBurst");
        if (stopTime >= SIMTIME_ZERO && stopTime < startTime)
            throw cRuntimeError("Invalid startTime/stopTime parameters");
        timeoutMsg = new cMessage("timer");
     //   last_packet = new Packet("data");
        replyTimeMax=16;
        repeated_req=0;
        reliableProtocol=par("reliableProtocol");
        burstyTraffic=par("burstyTraffic");
        burstStarted=false;
    }
}

void ClientTcpPoisson::handleStartOperation(LifecycleOperation *operation)
{
  //  std::cout<<"  In  handleStartOperation: "<< this->getFullPath()<< "   Socket Id: "<< socket.getSocketId()<<"stopTime:   "<<stopTime<<endl;
    simtime_t now = simTime();
    if (!burstyTraffic)
    {
    simtime_t start = std::max(startTime, now);
    if (timeoutMsg && ((stopTime < SIMTIME_ZERO) || (start < stopTime) || (start == stopTime && startTime == stopTime))) {
        timeoutMsg->setKind(MSGKIND_CONNECT);
        scheduleAt(start, timeoutMsg);
    }
    }
    else
    {
        std::cout<< this->getFullPath();
     //   std::cout<<"   In Bursty Traffic Start: "<<endl;

        simtime_t start = std::max(startBurst, now);
        stopTime =  start + burstLen;
        startTime=start;
//        std::cout<< "start:  "<<start<<"stop:  "<< stopTime<<endl;
        if (timeoutMsg && ((stopTime < SIMTIME_ZERO) || (start < stopTime) || (start == stopTime && startTime == stopTime))) {
            std::cout<<"stopTime in beginning :"<<stopTime<<endl;
            rescheduleOrDeleteTimer(start, MSGKIND_BURST_START);
//                timeoutMsg->setKind(MSGKIND_CONNECT);
//                scheduleAt(start, timeoutMsg);
            }

    }
}

void ClientTcpPoisson::handleStopOperation(LifecycleOperation *operation)
{
    std::cout<<"  In  handleStopOperation: "<< this->getFullPath()<< "   Socket Id: "<< socket.getSocketId()<<endl;

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
//    if (this->getFullPath()=="PretioWithLB.client[0].app[98]")
 //       std::cout<<"at:"<<simTime()<<"  In  sendRequest:    "<< this->getFullPath()<< "   Socket Id: "<< socket.getSocketId()<<endl<< "State:   " <<socket.getState() <<"Port Number:"<<socket.getLocalPort()<<endl;
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

//        if (this->getFullPath()=="PretioWithLB.client[0].app[98]")
//            std::cout << "sending request with " << requestLength << " bytes, expected reply length " << replyLength << " bytes,"
//                   << "remaining " << numRequestsToSend - 1 << " request\n"<<endl;
        if (socket.getState() == TcpSocket::CONNECTED || socket.getState() == TcpSocket::CONNECTING )
            sendPacket(packet);
        else
           {
            connect();
            sendPacket(packet);
            }

        sendInternalReqTime=simTime();
        simtime_t d = simTime() + replyTimeMax;
       // if (stopTime < SIMTIME_ZERO) {
    //    std::cout<<"sending reply out at:"<<simTime()<< "      for:"<<d<<endl;
        cMessage *msg=nullptr;
        msg = new cMessage("timer");
        msg->setKind(MSGKIND_REPLYTIMEOUT);
        scheduleAt(d, msg);  //here we check if we got reply in time  - this is end
    //  }
}

void ClientTcpPoisson::handleTimer(cMessage *msg)
{
 //   std::cout<<"handleTimer    in   ";
//    if (socket.getState() == TcpSocket::CONNECTED || socket.getState() == TcpSocket::CONNECTING)
    simtime_t d=simTime();
//    if(msg->getKind()==MSGKIND_BURST){
  //  std::cout<< this->getFullPath()<<":  "<<msg->getKind()<<"     :    ";
 //   std::cout<<"stop time:  "<<stopTime<< "   and now is:   "<<d  <<"  and start is: "<<startTime<<endl;
    if (((stopTime < SIMTIME_ZERO || d < stopTime) && d>startTime)||burstyTraffic==false)
        {
  //      std::cout<< "   INSIDE    :   "<<this->getFullPath()<<":  "<<msg->getKind()<<"     :    "<<endl;

            switch (msg->getKind()) {

            case MSGKIND_CONNECT:{
       //         std::cout<<"in handleTimer -> MSGKIND_CONNECT:       "<< "Port Number:"   <<socket.getLocalPort();//<<    "   OLD Socket ID:"<< socket.getSocketId()<<endl;
       //         std::cout<<"   state:           "<<socket.getState()<<endl;
                           // significance of earlySend: if true, data will be sent already
                           // in the ACK of SYN, otherwise only in a separate packet (but still
                           // immediately)
                if(connected==false)
                {
                    connect();
                    connected=true;
                }
                sendReqTime=simTime();
                if (earlySend)
                {
       //             std::cout<<"early send"<<endl;
                    sendRequest(); //sendInternalRequestTime will be adjusted in this function
                    numRequestsToSend--;

                }
            //    socketClosedFlag=false;
                socketEstablished(&socket);
          //
                break;

        }
            case MSGKIND_SEND:{
                //std::cout<<"in handleTimer -> MSGKIND_SEND:       "<< socket.getLocalAddress() << "Socket ID:"<< socket.getSocketId()<< "   Port Number:"<<socket.getLocalPort()<<endl;
                if (socket.getState() == TcpSocket::CONNECTED || socket.getState() == TcpSocket::CONNECTING)
                        {
                    sendRequest();  //sendInternalRequestTime will be adjusted in this function
                    numRequestsToSend--;

                }
                else
                {
          //          std::cout<<"Socket NOT connected in handleTimer -> MSGKIND_SEND:       "<< socket.getLocalAddress() << "Socket ID:"<< socket.getSocketId()<< "   Port Number:"<<socket.getLocalPort()<<endl;
                    connect();
                    sendRequest();  //sendInternalRequestTime will be adjusted in this function
                    numRequestsToSend--;
                //    std::cout<<"in handleTimer -> MSGKIND_SEND:       "<< socket.getLocalAddress() << "Socket ID:"<< socket.getSocketId()<< "   Port Number:"<<socket.getLocalPort()<<endl;
                }
                break;
            }

            case MSGKIND_REPLYTIMEOUT:{
             //

                //std::cout<<"in handleTimer -> MSGKIND_REPLYTIMEOUT:   "<< socket.getLocalAddress() << "    Socket ID: "  << socket.getSocketId()<< "      Port Number:"<<socket.getLocalPort()<<endl;
                simtime_t now_reply = simTime()-lastReplyTime;
            //    std::cout<<"lastReplyTime:   "<<lastReplyTime<<endl;
            //    std::cout<<"now-reply:   "<<now_reply<<endl;
                if (now_reply > replyTimeMax )
                {
                    EV_WARN << "didn't get reply from the server, re-sending a request," << "remaining " << numRequestsToSend  << " request\n";
             //       std::cout << "didn't get reply from the server, re-sending a request," << "remaining " << numRequestsToSend  << " request\n"<<endl;

                    if (reliableProtocol==false )
                    {
                  //      std::cout <<"now_reply:    "<<now_reply<< endl;
                 //       std::cout <<"In UDP sending the next message"<<endl;
                        lastReplyTime=simTime();
                        repeated_req=0;
                   //     TcpAppBase::socketDataArrived(socket, msg, urgent);
                        simtime_t d = simTime()-sendReqTime; //sendReqTime is the connect time
                        failedReqVector.record(d);
                        emit(failedReqSignal,d);

                        if (numRequestsToSend > 0) {
                              simtime_t myRespTime = simTime()-sendInternalReqTime;
                              EV_INFO << "UDP: reply hasn't arrived\n";
                              replyCount++;
                              numRequestsToRecieve--;
                           //   emit(numReqRecSignal, msg);
                          //    cancelEvent(timeoutMsg);
                           d = simTime() ;//+ par("thinkTime");
                           rescheduleOrDeleteTimer(d, MSGKIND_SEND);

                          }
                          else // if (numRequestsToSend = 0)
                              if (socketClosedFlag==false)
                              {
                                  replyCount++;

                                  EV_INFO << "reply to last request hasn't arrived but we waited enough, closing session\n";
                                  getReplyTime=simTime();
                                  respTime=getReplyTime-sendReqTime;  //it's actually session time
                                  respTimeVector.record(respTime);
                                  emit(respTimeSignal,respTime); //response time to all small requests, kind of demonstrates the server markov chain
                                  d = simTime();
                                  rescheduleOrDeleteTimer(d, MSGKIND_CLOSE);
                              }

                              else {//if (socket->getState() == TcpSocket::LOCALLY_CLOSED) {

                                         failedReqNoSendVector.record(replyCount);
                                         emit(failedReqNoSendSignal,replyCount);

                                         simtime_t d = simTime();
                                         rescheduleOrDeleteTimer(d, MSGKIND_CLOSE);

                                          }
                    }
                    else{
                    if (repeated_req>8)
                    {
                    //    std::cout<< this->getFullPath() <<"repeated_req MORE THAN 5:"<<repeated_req<<endl;
                        simtime_t d = simTime()-sendReqTime; //sendReqTime is the connect time
                        failedReqVector.record(d);
                        emit(failedReqSignal,d);
                        respTimeVector.record(d);
                        emit(respTimeSignal,d);

                        d = simTime();
                        rescheduleOrDeleteTimer(d, MSGKIND_CLOSE);
                    }
                    else if (timeoutMsg) {
                              //  cancelEvent(timeoutMsg);
                                numRequestsToSend++;
                                simtime_t d = simTime()-sendReqTime; //sendReqTime is the connect time
                                failedReqVector.record(d);
                                emit(failedReqSignal,d);
                                d = simTime();

//                                std::cout<< this->getFullPath() <<"     in MSGKIND_REPLYTIMEOUT       at:   "<< simTime() <<"   numRequestsToSend:     "<< numRequestsToSend<<endl;

                                ++repeated_req;
//                                std::cout <<"repeated_req:"<<repeated_req<<endl;

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
                    //std::cout<<"in handleTimer -> MSGKIND_CLOSE:   "<< socket.getLocalAddress() << "    Socket ID: "  << socket.getSocketId()<< "   Port Number:  "<<socket.getLocalPort()<< "     LastReplyTime:  "<<lastReplyTime<<endl;
                    close();

                break;
            }

            default:
                    throw cRuntimeError("Invalid timer msg: kind=%d", msg->getKind());
            }
    }
    else
    {
        switch (msg->getKind()) {

        case MSGKIND_BURST_FIN:
                            {
                            std::cout<<"Burst finished at:"<<stopTime<<endl;
                            burstStarted=false;
                            startTime = stopTime + burstLen;
                            stopTime =  startTime + burstLen;

                    //        std::cout<<"New Stop Time at:"<<stopTime<<endl;

                            if (startTime < stopTime) {
                                cMessage *timeoutMsg = nullptr;
                      //          std::cout<<"iN BURST FINISHED : New start Time at:"<<startTime<<endl;
                                timeoutMsg = new cMessage("timer");
                                timeoutMsg->setKind(MSGKIND_BURST_START);
                         //       std::cout<<"New start Time at:"<<startTime<<endl;

                                scheduleAt(startTime-1, timeoutMsg);
                                break;
                            }
                    case MSGKIND_BURST_START:
                                        {
                         //               std::cout<<"Burst start at:"<<simTime()<<endl;
                                        burstStarted=true;
                                   //     startTime = stopTime + burstLen;
                                        stopTime =  startTime + burstLen;
                                        numRequestsToSend = 0;
                                        numRequestsToRecieve=0;
                                        replyCount=0;
                                        lastReplyTime=startTime;
                                        sendInternalReqTime=startTime;

                                        repeated_req=0;
                                        respTime=0;

                                        sendReqTime=startTime;
                                        getReplyTime=startTime;
                                //        std::cout<<"New Stop Time at:"<<stopTime<<endl;
                                    //    int d=simTime();
                                 //       simtime_t i = simTime();
                                        if (startTime < stopTime) {

                    //                        while( i<startTime)
                                     //       {
                                       //     cancelEvent(timeoutMsg);
                                          //  cMessage *timeoutMsg = nullptr;
                                   //         std::cout<<"Start Time at:"<<startTime<<endl;
                                            timeoutMsg = new cMessage("timer");
                                            timeoutMsg->setKind(MSGKIND_CONNECT);
                                 //           std::cout<<"MSGKIND_CONNECT at:"<<startTime+1<<endl;

                                            scheduleAt(startTime+1, timeoutMsg);

                                        }
                                        cMessage *timeoutMsg = nullptr;
                               //         std::cout<<"Burst Stop Time at:"<<stopTime<<endl;
                                        timeoutMsg = new cMessage("timer");
                                        timeoutMsg->setKind(MSGKIND_BURST_FIN);
                              //          std::cout<<"New start Time at:"<<startTime<<endl;

                                        scheduleAt(stopTime, timeoutMsg);
                               //         std::cout<<"New start Time at:"<<startTime<<endl;
                                          break;
                                        }
                    case MSGKIND_REPLYTIMEOUT:{
                               //    std::cout<<"In handleTimer after burst finished-> MSGKIND_REPLYTIMEOUT:   "<< socket.getLocalAddress() << "    Socket ID: "  << socket.getSocketId()<< "      Port Number:"<<socket.getLocalPort()<< "     LastReplyTime:  "<<lastReplyTime<<endl;
                                   simtime_t now_reply = simTime()-lastReplyTime;
                                   if (now_reply > replyTimeMax )
                                   {
                                       EV_WARN << "didn't get reply from the server, re-sending a request," << "remaining " << numRequestsToSend  << " request\n";
                                //       std::cout << "didn't get reply from the server, re-sending a request," << "remaining " << numRequestsToSend  << " request\n"<<endl;

                                       if (reliableProtocol==false )
                                       {
                                     //      std::cout <<"now_reply:    "<<now_reply<< endl;
                                    //       std::cout <<"In UDP sending the next message"<<endl;
                                           lastReplyTime=simTime();
                                           repeated_req=0;
                                      //     TcpAppBase::socketDataArrived(socket, msg, urgent);
                                           simtime_t d = simTime()-sendReqTime; //sendReqTime is the connect time
                                           failedReqVector.record(d);
                                           emit(failedReqSignal,d);

                                           if (numRequestsToSend > 0) {
                                                 simtime_t myRespTime = simTime()-sendInternalReqTime;
                                                 EV_INFO << "non-reliable: reply hasn't arrived\n";
                                                 replyCount++;
                                                 numRequestsToRecieve--;
                                              //   emit(numReqRecSignal, msg);
                                             //    cancelEvent(timeoutMsg);
                                              d = simTime() ;//+ par("thinkTime");
                                              rescheduleOrDeleteTimer(d, MSGKIND_SEND_REPEAT);

                                             }
                                             else // if (numRequestsToSend = 0)
                                                 if (socketClosedFlag==false)
                                                 {
                                                     replyCount++;

                                                     EV_INFO << "reply to last request hasn't arrived but we waited enough, closing session\n";
                                                     getReplyTime=simTime();
                                                     respTime=getReplyTime-sendReqTime;  //it's actually session time
                                                     respTimeVector.record(respTime);
                                                     emit(respTimeSignal,respTime); //response time to all small requests, kind of demonstrates the server markov chain
                                                     d = simTime();
                                                   //  rescheduleOrDeleteTimer(d, MSGKIND_CLOSE);
                                                 }

                                                 else {//if (socket->getState() == TcpSocket::LOCALLY_CLOSED) {

                                                    failedReqNoSendVector.record(replyCount);
                                                    emit(failedReqNoSendSignal,replyCount);

                                                    simtime_t d = simTime();
                                               //     rescheduleOrDeleteTimer(d, MSGKIND_CLOSE);

                                                     }

                                       }
                                       else{
                                       if (repeated_req>8)
                                       {
                                       //    std::cout<< this->getFullPath() <<"repeated_req MORE THAN 5:"<<repeated_req<<endl;
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
                                                   failedReqVector.record(d);
                                                   emit(failedReqSignal,d);
                                                   d = simTime();

                   //                                std::cout<< this->getFullPath() <<"     in MSGKIND_REPLYTIMEOUT       at:   "<< simTime() <<"   numRequestsToSend:     "<< numRequestsToSend<<endl;

                                                   ++repeated_req;
                   //                                std::cout <<"repeated_req:"<<repeated_req<<endl;

                                                   rescheduleOrDeleteTimer(d, MSGKIND_SEND_REPEAT);
                                               }
                                       else
                                       {
                                           failedReqNoSendVector.record(numRequestsToSend);
                                           emit(failedReqNoSendSignal,numRequestsToSend);
                                           simtime_t d = simTime();
                                          // rescheduleOrDeleteTimer(d, MSGKIND_CLOSE);

                                       }
                                       }
                                   }
                                   break;
                               }
                    case MSGKIND_SEND_REPEAT:
                    {
                   //     std::cout<<"in handleTimer -> MSGKIND_SEND:       "<< socket.getLocalAddress() << "Socket ID:"<< socket.getSocketId()<< "   Port Number:"<<socket.getLocalPort()<<endl;
                        if (socket.getState() == TcpSocket::CONNECTED || socket.getState() == TcpSocket::CONNECTING)
                                {
                            sendRequest();  //sendInternalRequestTime will be adjusted in this function
                            numRequestsToSend--;
                        }
                        else
                        {
                            std::cout<<"Socket NOT connected in handleTimer -> MSGKIND_SEND:       "<< socket.getLocalAddress() << "Socket ID:"<< socket.getSocketId()<< "   Port Number:"<<socket.getLocalPort()<<endl;
                            connect();
                            sendRequest();  //sendInternalRequestTime will be adjusted in this function
                            numRequestsToSend--;
                            std::cout<<"in handleTimer -> MSGKIND_SEND:       "<< socket.getLocalAddress() << "Socket ID:"<< socket.getSocketId()<< "   Port Number:"<<socket.getLocalPort()<<endl;
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
  //  std::cout<<"In  socketEstablished"<<endl;
    socketClosedFlag=false;
    TcpAppBase::socketEstablished(socket);
  //  mySocket=&socket;

    // determine number of requests in this session
    numRequestsToSend = par("numRequestsPerSession");

    if (numRequestsToSend < 1)
        numRequestsToSend = 1;

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
   // std::cout<<"In rescheduleOrDeleteTimer, msgkind,  d:  "<< msgKind<<"   ,  "<<d<< endl;
    //std::cout<<"cancelling: "<<timeoutMsg->getKind()<<endl;
//    std::cout<<"In DELETE MSG "<< "time:"<< d<< ",  stop Time:  "<<stopTime<<endl;

    if(timeoutMsg)
        cancelEvent(timeoutMsg);

  //  if (stopTime < SIMTIME_ZERO || d < stopTime) {
        timeoutMsg->setKind(msgKind);
        scheduleAt(d, timeoutMsg);
   // }
//    else {
//        std::cout<<"In DELETE MSG "<< "time:"<< d<< ",  stop Time:  "<<stopTime<<endl;
//    //    timeoutMsg->setKind(msgKind);
//        if(timeoutMsg)
//        {
//            delete timeoutMsg;
//            timeoutMsg = nullptr;
//        }
//        if (socket.getState() == TcpSocket::CONNECTED || socket.getState() == TcpSocket::CONNECTING || socket.getState() == TcpSocket::PEER_CLOSED)
//                      socket.destroy();
//
//    }
}
void ClientTcpPoisson::replyTimeOut()
{
}
void ClientTcpPoisson::socketDataArrived(TcpSocket *socket, Packet *msg, bool urgent)
{

    lastReplyTime=simTime();
    repeated_req=0;
    TcpAppBase::socketDataArrived(socket, msg, urgent);

    if (numRequestsToSend > 0) {
        simtime_t myRespTime = simTime()-sendInternalReqTime;
        EV_INFO << "reply arrived\n";
        replyCount++;
        numRequestsToRecieve--;


     //   emit(numReqRecSignal, msg);
    //    cancelEvent(timeoutMsg);

        if (timeoutMsg) {
            simtime_t d = simTime() + par("thinkTime");
         ///   if (stopTime < SIMTIME_ZERO || d < stopTime)
                rescheduleOrDeleteTimer(d, MSGKIND_SEND);
        }
    }
    else //if (socket->getState() != TcpSocket::LOCALLY_CLOSED) { //There is no request left to send
        if (socketClosedFlag==false)
        {
            replyCount++;

            EV_INFO << "reply to last request arrived, closing session\n";
            getReplyTime=simTime();
            respTime=getReplyTime-sendReqTime;  //it's actually session time
            respTimeVector.record(respTime);
            emit(respTimeSignal,respTime); //response time to all small requests, kind of demonstrates the server markov chain
            simtime_t d = simTime();
       //     if (stopTime < SIMTIME_ZERO || d < stopTime)
                rescheduleOrDeleteTimer(d, MSGKIND_CLOSE);
        }

        else {

                   failedReqNoSendVector.record(replyCount);
                   emit(failedReqNoSendSignal,replyCount);

                   simtime_t d = simTime();
             //      if (stopTime < SIMTIME_ZERO || d < stopTime)
                       rescheduleOrDeleteTimer(d, MSGKIND_CLOSE);

                    }
    }
void ClientTcpPoisson::close()
{
   // std::cout<<"   in close"<< this->getFullPath()<<" socket   state:           "<<socket.getState()<<endl;
    socketClosedFlag=true;
    numRequestsToSend = 0;
    numRequestsToRecieve=0;
    replyCount=0;
    lastReplyTime=simTime();
    sendInternalReqTime=simTime();

//    WATCH(numRequestsToSend);
//    WATCH(earlySend);
    repeated_req=0;
    respTime=0;
    simtime_t d = simTime() + par("idleInterval");
    sendReqTime=d;
    getReplyTime=d;
    if (stopTime < SIMTIME_ZERO || d < stopTime) {
        if (timeoutMsg) {
         //   std::cout<<"*********************************"<<endl;
            rescheduleOrDeleteTimer(d, MSGKIND_CONNECT);
        }
    }
    else
    {
        if (socket.getState() == TcpSocket::CONNECTED || socket.getState() == TcpSocket::CONNECTING || socket.getState() == TcpSocket::PEER_CLOSED)
                           socket.destroy();
    }
   // TcpAppBase::close();
}
void ClientTcpPoisson::TimeOutSocketClosed()
{
    repeated_req=0;
    socketClosedFlag=true;
    EV_DETAIL << "socket Closed from client for TIMEOUT\n";
    simtime_t d=simTime()-sendReqTime;
    failedReqVector.record(d);
    emit(failedReqSignal,d);
    respTimeVector.record(d);
    emit(respTimeSignal,d);
    //TcpAppBase::close();
    std::cout<<"in  TimeOutSocketClosed:       "<< this->getFullPath();
    if (timeoutMsg) {
       // cancelEvent(timeoutMsg);

        simtime_t d = simTime() + par("idleInterval");
        if (stopTime < SIMTIME_ZERO || d < stopTime)
            rescheduleOrDeleteTimer(d, MSGKIND_CONNECT);
    }
}
void ClientTcpPoisson::socketClosed(TcpSocket *socket)  //no need for this function
{

//    TcpAppBase::finish();
//    TcpAppBase::close();
}

void ClientTcpPoisson::socketFailure(TcpSocket *socket, int code)
{
    TcpAppBase::socketFailure(socket, code);
//    if (this->getFullPath()=="PretioWithLB.client[6].app[0]")
//            {
//        std::cout<<"   in socketFailure  "<< this->getFullPath()<<"    state:           "<<socket->getState()<<endl;
//            }
    // reconnect after a delay
    simtime_t d = simTime() ;
    if (stopTime < SIMTIME_ZERO || d < stopTime)
        connect();
//    if (timeoutMsg) {
//        simtime_t d = simTime();// + par("reconnectInterval");
//        rescheduleOrDeleteTimer(d, MSGKIND_CONNECT);
//    }
}

} // namespace ErgodicityTest

