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

namespace ergodicitytest {


#define MSGKIND_CONNECT    0
#define MSGKIND_SEND       1
#define MSGKIND_REPLYTIMEOUT       2

Define_Module(ClientTcpPoisson);

ClientTcpPoisson::~ClientTcpPoisson()
{
    cancelAndDelete(timeoutMsg);
}

void ClientTcpPoisson::initialize(int stage)
{
    TcpAppBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        if (this->getFullPath()=="PretioWithLB.client[6].app[0]")
        {
            std::cout<<"Initialize in client today December 5"<<endl;}

        numRequestsToSend = 0;
        numRequestsToRecieve=0;
        replyCount=0;
        lastReplyTime=simTime();
        sendInternalReqTime=simTime();
        earlySend = false;    // TBD make it parameter
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
        if (stopTime >= SIMTIME_ZERO && stopTime < startTime)
            throw cRuntimeError("Invalid startTime/stopTime parameters");
        timeoutMsg = new cMessage("timer");
     //   last_packet = new Packet("data");
        replyTimeMax=32;
        repeated_req=0;

    }
}

void ClientTcpPoisson::handleStartOperation(LifecycleOperation *operation)
{
    simtime_t now = simTime();
    simtime_t start = std::max(startTime, now);
    if (timeoutMsg && ((stopTime < SIMTIME_ZERO) || (start < stopTime) || (start == stopTime && startTime == stopTime))) {
        timeoutMsg->setKind(MSGKIND_CONNECT);
        scheduleAt(start, timeoutMsg);
    }
}

void ClientTcpPoisson::handleStopOperation(LifecycleOperation *operation)
{
    cancelEvent(timeoutMsg);
    if (socket.getState() == TcpSocket::CONNECTED || socket.getState() == TcpSocket::CONNECTING || socket.getState() == TcpSocket::PEER_CLOSED)
        close();
}

void ClientTcpPoisson::handleCrashOperation(LifecycleOperation *operation)
{
    EV_WARN << "handle crash operation in client";
    cancelEvent(timeoutMsg);
    if (operation->getRootModule() != getContainingNode(this))
        socket.destroy();
}

void ClientTcpPoisson::sendRequest()
{
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
        sendPacket(packet);
//        if (this->getFullPath()=="PretioWithLB.client[6].app[0]")
//        { std::cout<< this->getFullPath() <<"    sendRequest NO.:    "<< numRequestsToSend <<  "    at:   "<< simTime()<<endl;}


        sendInternalReqTime=simTime();
       // last_packet = new Packet("data");
      //  last_packet->insertAtBack(payload);

   // numReqSent++;    //counting number of req per session
  //  numReqSent=last_packet;
  //  emit(numReqSentSignal, numReqSent);

    if (timeoutMsg) {
            simtime_t d = simTime() + replyTimeMax;
            rescheduleOrDeleteTimer(d, MSGKIND_REPLYTIMEOUT);
      }
}

void ClientTcpPoisson::handleTimer(cMessage *msg)
{
    switch (msg->getKind()) {
        case MSGKIND_CONNECT:

       //     std::cout<<"in  MSGKIND_CONNECT:       "<< socket->getLocalAddress();
         //     std::cout<<"   state:           "<<socket->getState();
           //   std::cout<<"           at:           "<< simTime()<<endl;

            connect();    // active OPEN
            sendReqTime=simTime();
                       // significance of earlySend: if true, data will be sent already
                       // in the ACK of SYN, otherwise only in a separate packet (but still
                       // immediately)
            if (earlySend)
            {
                  sendRequest();
                  numRequestsToSend--;
            }
            break;


        case MSGKIND_SEND:

                sendRequest();
                numRequestsToSend--;
         //   }
            break;

        case MSGKIND_REPLYTIMEOUT:
        {
//            if (this->getFullPath()=="PretioWithLB.client[6].app[0]")
//            {
//                std::cout<< this->getFullPath() <<"    in MSGKIND_REPLYTIMEOUT       at:   "<< simTime()<<endl;
//            }
           // if (((simTime()-lastReplyTime) > replyTimeMax) || ((simTime()-lastReplyTime) == replyTimeMax) || replyCount==0)
            simtime_t now_reply = simTime()-lastReplyTime;
            if (now_reply > replyTimeMax)
            {
                EV_WARN << "didn't get reply from the server, re-sending a request," << "remaining " << numRequestsToSend  << " request\n";
                if     (repeated_req>10)
                {
                  //  socketClosed(mySocket);
                    TimeOutSocketClosed();
                }

                else if (timeoutMsg) {
                            cancelEvent(timeoutMsg);
                            numRequestsToSend++;
                            simtime_t d = simTime();
                            rescheduleOrDeleteTimer(d, MSGKIND_SEND);
//                            if (this->getFullPath()=="PretioWithLB.client[6].app[0]")
//                                                std::cout<< this->getFullPath() <<"     in MSGKIND_REPLYTIMEOUT       at:   "<< simTime() <<"   numRequestsToSend:     "<< numRequestsToSend<<endl;

                            ++repeated_req;


                        }
                else
                {
                    failedReqNoSendVector.record(numRequestsToSend);
                    emit(failedReqNoSendSignal,numRequestsToSend);
                }

             /*   else{
                                replyCount++;

                                EV_INFO << "reply to last request hasn't arrived in max wait, closing session\n";
                                getReplyTime=simTime();
                                respTime=getReplyTime-sendReqTime;  //it's actually session time
                                respTimeVector.record(respTime);
                                emit(respTimeSignal,respTime); //response time to all small requests, kind of demonstrates the server markov chain
                               // emit(numReqRecSignal, totalnumReqRecieved);
                            //    respTimeHist.collect(respTime);
                                close();
                              //  socketClosed(socket);
                            }
                }*/

            }
            break;
        }
        default:
            throw cRuntimeError("Invalid timer msg: kind=%d", msg->getKind());
    }
}

void ClientTcpPoisson::socketEstablished(TcpSocket *socket)
{
    socketClosedFlag=false;
    TcpAppBase::socketEstablished(socket);
  //  mySocket=&socket;

    // determine number of requests in this session
    numRequestsToSend = par("numRequestsPerSession");
//    if (this->getFullPath()=="PretioWithLB.client[6].app[0]")
//    {
//    std::cout<<"in  socketEstablished:        "<< this->getFullPath();
//    std::cout<<"           numRequestsToSend:           "<<numRequestsToSend;
//    std::cout<<"           at:           "<< simTime()<<endl;
//    }
    if (numRequestsToSend < 1)
        numRequestsToSend = 1;

    // perform first request if not already done (next one will be sent when reply arrives)
    if (!earlySend)
    {
        sendRequest();
        numRequestsToSend--;
     //   numReqSent++;
    //    emit(numReqSentSignal, numReqSent);

    }
    numRequestsToRecieve=numRequestsToSend;

}

void ClientTcpPoisson::rescheduleOrDeleteTimer(simtime_t d, short int msgKind)
{
    cancelEvent(timeoutMsg);

    if (stopTime < SIMTIME_ZERO || d < stopTime) {
        timeoutMsg->setKind(msgKind);
        scheduleAt(d, timeoutMsg);
    }
    else {
        delete timeoutMsg;
        timeoutMsg = nullptr;
    }
}
void ClientTcpPoisson::replyTimeOut()
{




}
void ClientTcpPoisson::socketDataArrived(TcpSocket *socket, Packet *msg, bool urgent)
{

    lastReplyTime=simTime();
    repeated_req=0;
    TcpAppBase::socketDataArrived(socket, msg, urgent);
//    if (this->getFullPath()=="PretioWithLB.client[6].app[0]")
//               {
//                    std::cout<<"in  socketDataArrived:       "<< this->getFullPath();
//                    std::cout<<"   numRequestsToSend:           "<<numRequestsToSend<<endl;
//               }

    if (numRequestsToSend > 0) {
        simtime_t myRespTime = simTime()-sendInternalReqTime;
//        if (this->getFullPath()=="PretioWithLB.client[6].app[0]")
//        {
//        std::cout<<"in  socketDataArrived:       "<< this->getFullPath();
//        std::cout<<"   numRequestsToSend:           "<<numRequestsToSend;
//        std::cout<<"   myRespTime:           "<<myRespTime;
//        std::cout<<"   state:           "<<socket->getState();
//        std::cout<<"   at:           "<< simTime()<<endl;
//        }

        EV_INFO << "reply arrived\n";
        replyCount++;
        numRequestsToRecieve--;


     //   emit(numReqRecSignal, msg);
    //    cancelEvent(timeoutMsg);



        if (timeoutMsg) {
            simtime_t d = simTime() + par("thinkTime");
            rescheduleOrDeleteTimer(d, MSGKIND_SEND);
        }
    }
    else if (socket->getState() != TcpSocket::LOCALLY_CLOSED) {
        if (socketClosedFlag==false)
        {
            replyCount++;

            EV_INFO << "reply to last request arrived, closing session\n";
            getReplyTime=simTime();
            respTime=getReplyTime-sendReqTime;  //it's actually session time
            respTimeVector.record(respTime);
            emit(respTimeSignal,respTime); //response time to all small requests, kind of demonstrates the server markov chain
           // emit(numReqRecSignal, totalnumReqRecieved);
        //    respTimeHist.collect(respTime);
//            if (timeoutMsg) {
//                 simtime_t d = simTime() + par("idleInterval");
//                 rescheduleOrDeleteTimer(d, MSGKIND_CONNECT);
//             }
//            close();
//            if (this->getFullPath()=="PretioWithLB.client[6].app[0]")
//            {
//
//
////            std::cout<<"in  socketDataArrived:       "<< this->getFullPath();
////            std::cout<<"   numRequestsToSend:           "<<numRequestsToSend;
////            std::cout<<"   state:           "<<socket->getState();
////            std::cout<<"   respTime:           "<<respTime;
////            std::cout<<"   at:           "<< simTime()<<endl;
//            }
            cancelEvent(timeoutMsg);

            socketClosed(socket);
        }
    }
    else if (socket->getState() == TcpSocket::LOCALLY_CLOSED) {

                     //   socketClosed(socket);
                    //    if (LOCALLY_CLOSED_flag==true){

//        if (this->getFullPath()=="PretioWithLB.client[6].app[0]")
//                   {
//
//
////                   std::cout<<"in  socketDataArrived:       "<< this->getFullPath();
////                   std::cout<<"   numRequestsToSend:           "<<numRequestsToSend;
////                   std::cout<<"   STATE is LOCALLY_CLOSED           "<<endl;
//
//                   }
                              // simtime_t d = simTime() + par("idleInterval");
                              // rescheduleOrDeleteTimer(d, MSGKIND_CONNECT);
                            failedReqNoSendVector.record(replyCount);
                            emit(failedReqNoSendSignal,replyCount);

                   //         LOCALLY_CLOSED_flag=false;
                    //    }

                    }

    }


void ClientTcpPoisson::close()
{

//    if (this->getFullPath()=="PretioWithLB.client[6].app[0]")
//                {
////            std::cout<<"   in close   state:           "<<endl;
//                }
    replyCount=-1;
    TcpAppBase::close();
   // cancelEvent(timeoutMsg);
 //   LOCALLY_CLOSED_flag=true;

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
   // TcpAppBase::close();
    //TcpAppBase::socketClosed(socket);
  // numLostReq=numReqSent-numReqRecieved;

  //  emit(numLostReqSignal,numLostReq);
    // start another session after a delay
//    if (this->getFullPath()=="PretioWithLB.client[6].app[0]")
//    {
////        std::cout<<"in  TimeOutSocketClosed:       "<< this->getFullPath();
//      //  std::cout<<"   state:           "<<socket->getState();
//        std::cout<<"           at:           "<< simTime()<<endl;
//    }

    if (timeoutMsg) {
        cancelEvent(timeoutMsg);
        simtime_t d = simTime() + par("idleInterval");
        rescheduleOrDeleteTimer(d, MSGKIND_CONNECT);
    }
}
void ClientTcpPoisson::socketClosed(TcpSocket *socket)
{
    socketClosedFlag=true;
    EV_DETAIL << "socket Closed from client\n";
   // TcpAppBase::close();
    TcpAppBase::socketClosed(socket);
  // numLostReq=numReqSent-numReqRecieved;

  //  emit(numLostReqSignal,numLostReq);
    // start another session after a delay
//    if (this->getFullPath()=="PretioWithLB.client[6].app[0]")
//    {
//        std::cout<<"in  socketClosed:       "<< this->getFullPath();
//        std::cout<<"   state:           "<<socket->getState();
//        std::cout<<"           at:           "<< simTime()<<endl;
//    }

    if (timeoutMsg) {
        simtime_t d = simTime() + par("idleInterval");
        rescheduleOrDeleteTimer(d, MSGKIND_CONNECT);
    }
}

void ClientTcpPoisson::socketFailure(TcpSocket *socket, int code)
{
    TcpAppBase::socketFailure(socket, code);
//    if (this->getFullPath()=="PretioWithLB.client[6].app[0]")
//            {
//        std::cout<<"   in socketFailure   state:           "<<socket->getState()<<endl;
//            }
    // reconnect after a delay
    if (timeoutMsg) {
        simtime_t d = simTime() + par("reconnectInterval");
        rescheduleOrDeleteTimer(d, MSGKIND_CONNECT);
    }
}

} // namespace ergodicitytest

