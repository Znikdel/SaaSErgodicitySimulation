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

#include "ErgodicityTestClient.h"

#include "inet/applications/tcpapp/GenericAppMsg_m.h"
#include "inet/common/ModuleAccess.h"
#include "inet/common/lifecycle/ModuleOperations.h"
#include "inet/common/packet/Packet.h"
#include "inet/common/TimeTag_m.h"
#include "inet/networklayer/common/L3AddressResolver.h"

namespace ErgodicityTest {


#define MSGKIND_CONNECT            0
#define MSGKIND_SEND               1
#define MSGKIND_SEND_REPEAT        2
#define MSGKIND_REPLYTIMEOUT       3
#define MSGKIND_BURST_START        4
#define MSGKIND_BURST_FIN          5
#define MSGKIND_RTOS               6
#define MSGKIND_CLOSE              7

Define_Module(ErgodicityTestClient);

ErgodicityTestClient::~ErgodicityTestClient()
{
    cancelAndDelete(timeoutMsg);
}
void ErgodicityTestClient::initialize(int stage)
{
  //  std::cout<<"initialize:   "<<this->getFullPath()<<endl;

    TcpAppBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        mainSocketID=socket.getSocketId();
        numRequestsToSend = -1;
        numRequestsToRecieve=0;
        replyCount=0;

        lastReplyTime=simTime();
        sendInternalReqTime=simTime();
        earlySend = false;    // TBD make it parameter
        connected=false;

        sendReqTime=simTime();
        getReplyTime=simTime();
        respTime=0;
        respTimeVector.setName("ResponseTime");
        respTimeSignal=registerSignal("respTime");

        failedReqVector.setName("FailedRequest");
        failedReqSignal=registerSignal("failedReq");
        failedReqNoSendVector.setName("FailedNoSend");
        failedReqNoSendSignal=registerSignal("failedNoReq");

        newContainerSignal=registerSignal("newContainer");

        startTime = par("startTime");
        stopTime = par("stopTime");
        burstLen = par("burstLen");
        startBurst= par("startBurst");
        if (stopTime >= SIMTIME_ZERO && stopTime < startTime)
            throw cRuntimeError("Invalid startTime/stopTime parameters");

        timeoutMsg = new cMessage("timer");
        reliabletimeoutMsg=new cMessage("timer");
        bursttimeoutMsg = new cMessage("timer");
        rtostimeoutMsg= new cMessage("timer");

        replyTimeMax=32;
        repeated_req=0;
        failed_req=0;  //for non-reliable protocol
        reliableProtocol=par("reliableProtocol");
        burstyTraffic=par("burstyTraffic");
        RTOS=par("RTOS");
        dynamicScaling=par("dynamicScaling");

        burstStarted=false;
        burst_finished=0;

        RTOS_hard_limit=8;   //with RTOS reliable should be false

        connectPort = par("connectPort");
   //     localPort = par("localPort");
        portCounter=0;

    }
}
void ErgodicityTestClient::handleStartOperation(LifecycleOperation *operation)
{
//    if (this->getFullPath()=="PretioWithLB.client[0].app[0]")
//        std::cout<<"  In  handleStartOperation: "<< endl;
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
//            if (this->getFullPath()=="PretioWithLB.client[0].app[0]")
//                std::cout<<"stopTime in beginning :"<<stopTime<<endl;
            bursttimeoutMsg->setKind(MSGKIND_BURST_START);
            scheduleAt(start, bursttimeoutMsg);
            }
    }
}
void ErgodicityTestClient::handleStopOperation(LifecycleOperation *operation)
{
    std::cout<<"now in handleStopOperation:  "<<simTime() <<"   startTime:   "<<startTime<<"   stopTime:    "<<stopTime<<endl;
    if(timeoutMsg)
        cancelEvent(timeoutMsg);
    if (socket.getState() == TcpSocket::CONNECTED || socket.getState() == TcpSocket::CONNECTING || socket.getState() == TcpSocket::PEER_CLOSED)
        socket.destroy();
}
void ErgodicityTestClient::handleCrashOperation(LifecycleOperation *operation)
{
    EV_WARN << "handle crash operation in client";
    if(timeoutMsg)
        cancelEvent(timeoutMsg);
    if (operation->getRootModule() != getContainingNode(this))
        socket.destroy();
}
void ErgodicityTestClient::handleTimer(cMessage *msg)
{
    simtime_t d=simTime();
//    if (this->getFullPath()=="PretioWithLB.client[3].app[0]")
//        std::cout<<"now in handleTimer:  "<<d <<"   startTime:   "<<startTime<<"   stopTime:    "<<stopTime<<endl;
    if (((stopTime < SIMTIME_ZERO || d < stopTime) && d>=startTime) || burstyTraffic)
    {

            switch (msg->getKind()) {

            case MSGKIND_CONNECT:
            {
//                if (this->getFullPath()=="PretioWithLB.client[0].app[4]")
//                    std::cout<<"    MSGKIND_CONNECT:"<<msg->getKind()<<"connect port:   "<<connectPort<<endl;
                msg_Connect(msg);
                break;
            }
            case MSGKIND_SEND:
            {
//                if (this->getFullPath()=="PretioWithLB.client[0].app[0]")
//                    std::cout<<"    MSGKIND_SEND:"<<msg->getKind()<<endl;
                sendRequest(0);
                break;
            }
            case MSGKIND_SEND_REPEAT:
            {
//                if (this->getFullPath()=="PretioWithLB.client[0].app[4]")
//                    std::cout<<"    MSGKIND_SEND_REPEAT:   "<<repeated_req<< endl;
                sendRequest(1);
                break;
            }
            case MSGKIND_REPLYTIMEOUT:
            {
//                if (this->getFullPath()=="PretioWithLB.client[0].app[0]")
//                    std::cout<<" connectPort:  "<< connectPort <<endl;
                msg_ReplyTimeOut(msg);
                break;
            }
            case MSGKIND_BURST_START:
            {
//                if (this->getFullPath()=="PretioWithLB.client[0].app[0]")
//                    std::cout<<"    MSGKIND_BURST_START:"<<msg->getKind()<<endl;
                msg_Burst_Start(msg);
                break;
            }
            case MSGKIND_BURST_FIN:
            {
//                if (this->getFullPath()=="PretioWithLB.client[0].app[0]")
//                    std::cout<<"    MSGKIND_BURST_FIN:"<<msg->getKind()<<endl;
                msg_Burst_Finish(msg);
                break;
            }
            case MSGKIND_RTOS:
            {
//                if (this->getFullPath()=="PretioWithLB.client[3].app[0]")
//                    std::cout<<this->getFullPath()<<"    MSGKIND_RTOS:"<<msg->getKind()<<endl;
                msg_RTOS(msg);
                break;
            }
            case MSGKIND_CLOSE:
            {
//                if (this->getFullPath()=="PretioWithLB.client[0].app[0]")
//                        std::cout<<"    MSGKIND_CLOSE:"<<msg->getKind()<<endl;
                if (!burstyTraffic)
                    close();
                else
                    TimeOutSocketClosed();
                break;
            }
            default:
            {
               if(timeoutMsg)
               cancelEvent(timeoutMsg);
               throw cRuntimeError("Invalid timer msg: kind=%d", msg->getKind());
            }
            }

    }
}
Packet* ErgodicityTestClient::makePacket(bool resend)
{

       long requestLength  = par("requestLength");
       long replyLength;

       if(resend)
           replyLength= (numRequestsToSend+1)*10;
       else
           replyLength= numRequestsToSend*10;

       if (requestLength < 1)
           requestLength = 1;
       if (replyLength < 1)
           replyLength = 1;
      // if(RTOS)
       requestLength=replyLength;
       const auto& payload = makeShared<GenericAppMsg>();

           Packet *packet = new Packet("data");
           payload->setChunkLength(B(requestLength));
           payload->setExpectedReplyLength(B(replyLength));
           payload->setServerClose(false);
           payload->addTag<CreationTimeTag>()->setCreationTime(simTime());

           packet->insertAtBack(payload);

           EV_INFO << "sending request with " << requestLength << " bytes, expected reply length " << replyLength << " bytes,"
                   << "remaining " << numRequestsToSend << " request\n";

//           if(!resend)
//               std::cout << "sending request with " << requestLength << " bytes, expected reply length " << replyLength << " bytes,"
//                              << "remaining " << numRequestsToSend-1 << " request\n";
//           if(resend)
//               if (this->getFullPath()=="PretioWithLB.client[0].app[0]")
//                   std::cout <<"at:"<<simTime()<< "Re-sending request with " << requestLength << " bytes, expected reply length " << replyLength << " bytes,"
//                                             << "remaining " << numRequestsToSend << " request\n";

           return packet;
}
void ErgodicityTestClient::sendRequest(bool resend)
{
//        if (this->getFullPath()=="PretioWithLB.client[0].app[0]")
//            std::cout<<"  In  send Request: "<< "at:   "<<simTime()<< "numRequestsToSend:   "<<numRequestsToSend<< "    resend:   "<<resend <<
//           "   ,    Socket Id---> "<< socket.getSocketId()<<endl;

        if (socket.getState() == TcpSocket::CONNECTED || socket.getState() == TcpSocket::CONNECTING )
                  {
//                      if (this->getFullPath()=="PretioWithLB.client[0].app[4]")
//                          std::cout<<"In send Packet - Socket Connected at: "<<simTime()<< "numRequestsToSend:   "<<numRequestsToSend<<endl;
                      Packet *packet = makePacket(resend);
                      sendPacket(packet);
                      if (!resend)
                          numRequestsToSend--;  // only if it's the first time sending this message

//                      std::cout<<"In send Packet - numRequestsToSend: "<<numRequestsToSend<<endl;
                      sendInternalReqTime=simTime();

                      simtime_t d;
//                      if (this->getFullPath()=="PretioWithLB.client[0].app[0]" and resend)
 //                         std::cout<<"  In  send Request: "<<this->getFullPath() << "    at:   "<<simTime()<< "numRequestsToSend:   "<<numRequestsToSend<< endl;
//                              "    resend:   "<<resend << "     reliabletimeoutMsg---> "<<d<<
//                                 "   ,    Socket Id---> "<< socket.getSocketId()<<endl;
//                      if (reliabletimeoutMsg)
//                          cancelEvent(reliabletimeoutMsg);
//                      reliabletimeoutMsg->setKind(MSGKIND_REPLYTIMEOUT);
//                      scheduleAt(d, reliabletimeoutMsg);  // check if we'll get reply in time

                      if(RTOS)
                      {
                            d=sendInternalReqTime + RTOS_hard_limit;
//                          if (this->getFullPath()=="PretioWithLB.client[0].app[0]")
//                                                std::cout<<"In send Packet - MSGKIND_RTOS at: "<<simTime()<<"d:    "<<d<<endl;

                            if (rtostimeoutMsg)
                                cancelEvent(rtostimeoutMsg);
                            rtostimeoutMsg->setKind(MSGKIND_RTOS);
                            scheduleAt(d, rtostimeoutMsg);  // check if we'll get reply in time
                        }
                      else
                      {
                          d = sendInternalReqTime + replyTimeMax;
                          if (reliabletimeoutMsg)
                              cancelEvent(reliabletimeoutMsg);
                          reliabletimeoutMsg->setKind(MSGKIND_REPLYTIMEOUT);
                          scheduleAt(d, reliabletimeoutMsg);  // check if we'll get reply in time

                      }
                  }
            else
            {
                    simtime_t d = simTime();
                    std::cout<<"Socket is not connected$$$$$$$$$$$$$$$$$$$$$$$$$$:"<<socket.getSocketId()<<"   state:   "<<socket.getState()<<simTime()<< "      for:"<<d<<endl;
                    connect();
                    sendRequest(resend);  //sendInternalRequestTime will be adjusted in this function

            }
}
void ErgodicityTestClient::msg_Connect(cMessage *msg)
{
    // The request initiated by user
        // significance of earlySend: if true, data will be sent already
        // in the ACK of SYN, otherwise only in a separate packet (but still
        // immediately)
   // std::cout<<"In msg_Connect:  connected:   "<<connected<<endl;
    if(connected==false)   // connected changes to false when we have too many (8)dropped messages
    {
     //   std::cout<<"In msg_Connect:  connected: BEFORE:   "<<connected<<endl;
        connect();    // get a new socket id
        connected=true;
//        if (this->getFullPath()=="PretioWithLB.client[0].app[4]")
//            std::cout<<"In msg_Connect:  connected: at:  "<<simTime()<<" ,  and connect port is:"<<socket.getRemotePort()<<endl;
    }
    else
    {

        socketEstablished(&socket); // number of needed middle servers specified here
    }
    socketClosedFlag=false;


    if (earlySend)
    {
        std::cout<<"earlySend:  "<<earlySend<<endl;
        sendRequest(0);
        sendReqTime=simTime();
        sendInternalReqTime=simTime();

    }
}
void ErgodicityTestClient::connect()
{
    // we need a new connId if this is not the first connection
    if (this->getFullPath()=="PretioWithLB.client[3].app[0]")
        std::cout<<this->getFullPath()<<"    In connect:  connected: at:  "<<simTime()<<"       Current Connect Port:   "<< connectPort <<endl;

    socket.renewSocket();

    const char *localAddress = par("localAddress");
    int localPort = getEnvir()->getUniqueNumber();//par("localPort");
    if (localPort>65535)
        localPort=-1;
//    localPort = localPort+1;
    if (this->getFullPath()=="PretioWithLB.client[0].app[0]")
        std::cout<<this->getFullPath()<<":   In connect:  connected: at:  "<<simTime()<<"  Current Connect Port:   "<< localPort <<endl;
    socket.bind(*localAddress ? L3AddressResolver().resolve(localAddress) : L3Address(), localPort);
    if (this->getFullPath()=="PretioWithLB.client[0].app[0]")
        std::cout<<this->getFullPath()<<":   In connect:  connected: at:  "<<simTime()<<"  AFTER local  Port:   "<< socket.getLocalPort() <<endl;

    int timeToLive = par("timeToLive");
    if (timeToLive != -1)
        socket.setTimeToLive(timeToLive);

    int dscp = par("dscp");
    if (dscp != -1)
        socket.setDscp(dscp);

    int tos = par("tos");
    if (tos != -1)
        socket.setTos(tos);

    // connect
    const char *connectAddress = par("connectAddress");
//    int connectPort = par("connectPort");
//    if(dynamicScalingAdd)
//    {
//        connectPort=connectPort+1;
//        portCounter=portCounter+1;
//        if(portCounter>=10)
//            connectPort = par("connectPort");
//    //    if (this->getFullPath()=="PretioWithLB.client[0].app[4]")
//            std::cout<<this->getFullPath()<<"    : In connect: dynamicScalingAdd connected: at:  "<<simTime()<<"Connect Port:   "<< connectPort <<endl;
//        dynamicScalingAdd=false;
//    }
    L3Address destination;
    L3AddressResolver().tryResolve(connectAddress, destination);
    if (destination.isUnspecified()) {
        std::cout << "Connecting to " << connectAddress << " port=" << connectPort << ": cannot resolve destination address\n";
    }
    else {
     //  std::cout << "Connecting to " << connectAddress << "(" << destination << ") port=" << connectPort << endl;

        socket.connect(destination, connectPort);

        numSessions++;
        emit(connectSignal, 1L);
    }
}

void ErgodicityTestClient::msg_RTOS(cMessage *msg)
{

    simtime_t d=simTime();
    d = simTime()-sendInternalReqTime;
//    if (this->getFullPath()=="PretioWithLB.client[0].app[0]")
//        std::cout<<"In MSGKIND_RTOS at: "<<simTime()<<" , resp time:    "<<d<<endl;
      failedReqVector.record(d);
      emit(failedReqSignal,d);

      failedReqNoSendVector.record(numRequestsToSend);
      emit(failedReqNoSendSignal,numRequestsToSend);
     // cancelEvent(timeoutMsg);
    d = simTime();
//                                 ++repeated_req;
    std::string fullPath = this->getFullPath();
    std::string searchString = "client[0]"; // we do auto scaling only for client 0 which is our system
    if(dynamicScaling and (fullPath.find(searchString) != std::string::npos))
//    if(dynamicScaling)
    {
                 //                             simtime_t d = simTime()-sendReqTime; //sendReqTime is the connect time
         //                                     failedReqVector.record(d);
           //                                   emit(failedReqSignal,d);
             //                                 failedReqNoSendVector.record(numRequestsToSend);
               //                               emit(failedReqNoSendSignal,numRequestsToSend);
                     //                         d = simTime();
                                  //            if (fullPath.find(searchString) != std::string::npos)
                                    //            std::cout<<this->getFullPath()<<":    @@@@@@@@@@@@@@@@@@@@@  In  DYNAMIC SCALING re-sending:  repeated_req  --->"<<repeated_req<<endl;
                                //        if(failed_req>8)

                                              connected=false;
//                                              repeated_req=0;
                                             // dynamicScalingAdd=true;

                                              portCounter=portCounter+1;//std::rand()%10;
                                       //       std::cout<<this->getFullPath()<<"   portCounter:"<<portCounter<<endl;
                                              connectPort=connectPort+1;
                                              if(portCounter>=10)
                                              {
                                                  connectPort = par("connectPort");
                                                  connectPort=connectPort+int(portCounter%10);
                                              }
                                              //    if (this->getFullPath()=="PretioWithLB.client[0].app[4]")
                                              std::cout<<this->getFullPath()<<"    : dynamicScaling at:  "<<simTime()<<"   Connect Port:   "<< connectPort <<endl;
                                              //dynamicScalingAdd=false;
                                            //  newContainer.record(portCounter);
                                              if(portCounter<=10)
                                                  emit(newContainerSignal,portCounter);
                                        }
//                                              rescheduleOrDeleteTimer(d, MSGKIND_CLOSE);

   //   d = simTime();
  //  else{
      if(!burstyTraffic)
          rescheduleOrDeleteTimer(d, MSGKIND_CLOSE);
    // we need to move on to the next request
//}

    //   std::cout <<"CLOSING    ===>>>>>" <<this->getFullPath()<<"    didn't get reply from the server,   distance with last reply:    "<< now_reply<< "     remaining " << numRequestsToSend  << " request\n";

    //         }
    //                          if (this->getFullPath()=="PretioWithLB.client[0].app[0]")
    //   std::cout<<"In send Packet - MSGKIND_RTOS at: "<<simTime()<<"d:    "<<simTime()-sendInternalReqTime<<endl;


    //                              }


}
void ErgodicityTestClient::msg_ReplyTimeOut(cMessage *msg)
{
    simtime_t d=simTime();
    simtime_t now_reply = d-lastReplyTime;

//    if (this->getFullPath()=="PretioWithLB.client[0].app[0]")
//        std::cout<<"  In  MSGKIND_REPLYTIMEOUT at: "<< d <<" ,   now_reply:     "<<now_reply<< endl;

                   if (now_reply >= replyTimeMax )
                   {
//                       if (this->getFullPath()=="PretioWithLB.client[0].app[0]")
//                           std::cout << "didn't get reply from the server, re-sending a request," << "remaining " << numRequestsToSend  << " request\n";
//                       if(failed_req>0)
//                           if (this->getFullPath()=="PretioWithLB.client[0].app[0]")
//                               std::cout<<"  In  MSGKIND_REPLYTIMEOUT: "<<
//                                                     this->getFullPath()<< "   Socket Id: "<<
//                                                     socket.getSocketId()<<"at:   "<<simTime() <<
//                                                     "   failed req:   " << failed_req <<
//                                                     "   repeated req: " << repeated_req <<
//                                                     "   now_reply:  "  << now_reply << endl;
                       if (!reliableProtocol)
                       {
//                          if (this->getFullPath()=="PretioWithLB.client[0].app[0]")
//                          {     std::cout<<"  In  MSGKIND_REPLYTIMEOUT_Non-Reliable: "<<  this->getFullPath()<< "at:   "<<
//                                   simTime() <<"   failed req:   " << failed_req <<
//                                   "----> numRequestsToSend ------>   "<<numRequestsToSend<<endl;
//                          }
                          if (failed_req>4)  // we waited for 4 times of max_reply_time. If there is no reply it means it's a failed request
                             {
//                                  if (this->getFullPath()=="PretioWithLB.client[0].app[0]")
//                                     {
//                                      std::cout<<"  In  MSGKIND_REPLYTIMEOUT_Non-Reliable: "<<
//                                                                   this->getFullPath()<< "at:   "<<simTime() <<
//                                                                   "   failed req:   " << failed_req <<
//                                                                   "   repeated req: " << repeated_req <<
//                                                                   "   sendReqTime:  "  << sendReqTime <<  endl;
//                                     }
                                   if (repeated_req>4)
                                     {
                                          repeated_req=0;
                                          connected=false;
                                      }
                                      failed_req=0;  // check if too many failed messages maybe the socket has problem so re-establish the connection
                                      repeated_req++;

                                      d = simTime()-sendReqTime;

                                      failedReqVector.record(d);
                                      emit(failedReqSignal,d);

                                      failedReqNoSendVector.record(numRequestsToSend);
                                      emit(failedReqNoSendSignal,numRequestsToSend);

                                      d = simTime();
                                //      if(!burstyTraffic)
                                      rescheduleOrDeleteTimer(d, MSGKIND_CLOSE); // we need to move on to the next request
                             }

                           ++failed_req; // wait more
                           std::cout<<"  In  MSGKIND_REPLYTIMEOUT_Non-Reliable: "<<
                                                                                          this->getFullPath()<< "at:   "<<simTime() <<
                                                                                          "   failed req:   " << failed_req <<endl;
                           d = simTime() + replyTimeMax;

                           if (reliabletimeoutMsg)
                                 cancelEvent(reliabletimeoutMsg);
                           reliabletimeoutMsg->setKind(MSGKIND_REPLYTIMEOUT);
                           scheduleAt(d, reliabletimeoutMsg);  // check if we'll get reply in time

                       }
                       else{  // reliable protocol
                           //if(repeated_req>4)
//                           if (this->getFullPath()=="PretioWithLB.client[0].app[0]")
//                               std::cout<<"  In  MSGKIND_REPLYTIMEOUT: "<<
//                                                                               this->getFullPath()<< "   Socket Id: "<<
//                                                                                socket.getSocketId()<<"at:   "<<simTime() <<                                                                                "   repeated req: " << repeated_req <<
//                                                                                "   sendReqTime:  "  << sendReqTime <<  endl;
                       if (timeoutMsg and (repeated_req>8))
                       {
//                           if (this->getFullPath()=="PretioWithLB.client[0].app[0]")
//                               std::cout<<"  In  failed req:  repeated_req  --->"<<repeated_req<<endl;
                           simtime_t d = simTime()-sendReqTime; //sendReqTime is the connect time
                           failedReqVector.record(d);
                           emit(failedReqSignal,d);
                           failedReqNoSendVector.record(numRequestsToSend);
                           emit(failedReqNoSendSignal,numRequestsToSend);
                           d = simTime();
                           ++failed_req;
                           if(failed_req>8)
                           {
                               connected=false;
                               failed_req=0;
                           }
                           rescheduleOrDeleteTimer(d, MSGKIND_CLOSE);
                       }
                       else if (timeoutMsg) {   //re-sending the request
                                   cancelEvent(timeoutMsg);
                                   d = simTime();
                                   ++repeated_req;
                                   std::string fullPath = this->getFullPath();
                                   std::string searchString = "client[0]"; // we do auto scaling only for client 0 which is our system
                                   if(dynamicScaling and (fullPath.find(searchString) != std::string::npos))
                               //    if(dynamicScaling)
                                   {
                                         simtime_t d = simTime()-sendReqTime; //sendReqTime is the connect time
                                         failedReqVector.record(d);
                                         emit(failedReqSignal,d);
                                         failedReqNoSendVector.record(numRequestsToSend);
                                         emit(failedReqNoSendSignal,numRequestsToSend);
                                         d = simTime();
                             //            if (fullPath.find(searchString) != std::string::npos)
                               //            std::cout<<this->getFullPath()<<":    @@@@@@@@@@@@@@@@@@@@@  In  DYNAMIC SCALING re-sending:  repeated_req  --->"<<repeated_req<<endl;
                           //        if(failed_req>8)

                                         connected=false;
                                         repeated_req=0;
                                        // dynamicScalingAdd=true;

                                         portCounter=portCounter+1;//std::rand()%10;
                                  //       std::cout<<this->getFullPath()<<"   portCounter:"<<portCounter<<endl;
                                         connectPort=connectPort+1;
                                         if(portCounter>=10)
                                         {
                                             connectPort = par("connectPort");
                                             connectPort=connectPort+int(portCounter%10);
                                         }
                                         //    if (this->getFullPath()=="PretioWithLB.client[0].app[4]")
                                         std::cout<<this->getFullPath()<<"    : dynamicScaling at:  "<<simTime()<<"   Connect Port:   "<< connectPort <<endl;
                                         //dynamicScalingAdd=false;
                                       //  newContainer.record(portCounter);
                                         if(portCounter<=10)
                                             emit(newContainerSignal,portCounter);

                                         rescheduleOrDeleteTimer(d, MSGKIND_CLOSE);
                                        // rescheduleOrDeleteTimer(d, MSGKIND_CONNECT);
                                     }
                                   else
                                   {
                                       rescheduleOrDeleteTimer(d, MSGKIND_SEND_REPEAT);
                                   }

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
}
void ErgodicityTestClient::msg_Burst_Start(cMessage *msg)
{

          burstStarted=true;
          stopTime =  startTime + burstLen;
          burst_finished=stopTime;
          if (startTime < stopTime and numRequestsToSend==-1)
              rescheduleOrDeleteTimer(startTime, MSGKIND_CONNECT);

          if(bursttimeoutMsg)
              cancelEvent(bursttimeoutMsg);

          bursttimeoutMsg->setKind(MSGKIND_BURST_FIN);
          scheduleAt(stopTime, bursttimeoutMsg);
}
void ErgodicityTestClient::msg_Burst_Finish(cMessage *msg)
{
//    if (this->getFullPath()=="PretioWithLB.client[0].app[0]")
//                                          std::cout<<"------------#########-------IN BURST FINISH ------>  Burst    finished at:"<<simTime()<<endl;
        burstStarted=false;
        startTime = stopTime + burstLen;  // next start time
        stopTime =  startTime + burstLen; // next stop time

        if (startTime < stopTime) {

            if(bursttimeoutMsg)
                cancelEvent(bursttimeoutMsg);
            bursttimeoutMsg->setKind(MSGKIND_BURST_START);
            scheduleAt(startTime, bursttimeoutMsg);
        }

}
void ErgodicityTestClient::socketEstablished(TcpSocket *socket)
{
  //  std::cout<<"In socketEstablished START at:  "<<simTime()<<endl;

    socketClosedFlag=false;
    int oldnumRequestsToSend=numRequestsToSend;
    numRequestsToSend = par("numRequestsPerSession");
    if(numRequestsToSend==oldnumRequestsToSend)
        --numRequestsToSend;
    if (numRequestsToSend < 1)
        numRequestsToSend = 1;
  //  if (this->getFullPath()=="PretioWithLB.client[0].app[4]")
    //   std::cout<<this->getFullPath()<<"In socketEstablished at:  "<< simTime()<<"    numRequestsToSend----->"<<numRequestsToSend<<"   earlySend:   "<<earlySend<<endl;
    // perform first request if not already done (next one will be sent when reply arrives)
    if (!earlySend)
    {
//        if (this->getFullPath()=="PretioWithLB.client[0].app[0]" and simTime()>5100)
//        std::cout<<"In socketEstablished and sendReqTime at:  "<<simTime()<<"   sendinternalReqTime:   "<<sendInternalReqTime<<endl;
        sendRequest(0);
        sendReqTime=simTime();
        sendInternalReqTime=simTime();

    }
// numRequestsToRecieve=numRequestsToSend; // Why Here?

    //  std::cout<<"In socketEstablished END at:  "<<simTime()<<endl;
}
void ErgodicityTestClient::rescheduleOrDeleteTimer(simtime_t d, short int msgKind)
{
//    if (this->getFullPath()=="PretioWithLB.client[0].app[0]")
//           std::cout<<"In rescheduleOrDeleteTimer old msg:"<<timeoutMsg->getKind()<<  "     at:"<< simTime()<<"    new msg::"<<   msgKind<<endl;
    if(timeoutMsg)
        cancelEvent(timeoutMsg);

    timeoutMsg->setKind(msgKind);
    scheduleAt(d, timeoutMsg);
}
void ErgodicityTestClient::socketDataArrived(TcpSocket *socket, Packet *msg, bool urgent)
{
    int msgRecieved=msg->getByteLength()/10;
//    if (this->getFullPath()=="PretioWithLB.client[3].app[0]")
//    std::cout<<this->getFullPath()<<"   In socketDataArrived at: "<<simTime()<<"   msgRecieved ---->"<<msgRecieved<<"   numRequestsToSend:   "<<numRequestsToSend<< "      Current Connect Port:" << connectPort <<endl;

    TcpAppBase::socketDataArrived(socket, msg, urgent); // deleting msg

    if(msgRecieved==(numRequestsToSend+1))
    {
        cancelEvent(reliabletimeoutMsg);
        cancelEvent(rtostimeoutMsg);


        lastReplyTime=simTime();
        repeated_req=0;

        if (numRequestsToSend > 0) {
            simtime_t myRespTime = simTime()-sendInternalReqTime;
            if (myRespTime>8)
//            if(this->getFullPath()=="PretioWithLB.client[3].app[0]")
//                std::cout<<this->getFullPath() <<"  //////////////   reply arrived at:    "<<simTime()<<"  ,  myRespTime:"<<myRespTime<<"    ---> numRequestsToSend ------>   "<<numRequestsToSend<<endl;
            EV_INFO << "reply arrived\n";
            replyCount++;
            numRequestsToRecieve--;
            failed_req=0;

            if (timeoutMsg) {
                simtime_t d = simTime() + par("thinkTime");
                rescheduleOrDeleteTimer(d, MSGKIND_SEND);
            }
        }
        else{  //There is no request left to send
            if (socketClosedFlag==false)
            {
                replyCount++;
                failed_req=0;

                EV_INFO << "reply to last request arrived, closing session\n";
                getReplyTime=simTime();
                respTime=getReplyTime-sendReqTime;  //it's actually session time
                respTimeVector.record(respTime);
                emit(respTimeSignal,respTime); //response time to all small requests, kind of demonstrates the server markov chain
                simtime_t d = simTime();
                if (this->getFullPath()=="PretioWithLB.client[3].app[0]")
                if(respTime>56)
                    std::cout<<this->getFullPath()<<"     ///////// LAST  socketDataArrived  //////////   at   ---->"<<simTime()<<"   sendReqTime:   "<<sendReqTime<<"    resp time: ---->"<<respTime<<endl;
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
    }
    else
    {
       // std::cout<< this->getFullPath()<<"  msgRecieved: "<<msgRecieved<< "     we are waiting for:---> "<<(numRequestsToSend+1)<<endl;
    }
}
void ErgodicityTestClient::close()
{
  //  if (this->getFullPath()=="PretioWithLB.client[0].app[0]")
    //           std::cout<<" SocketClosed   and sendReqTime at:    "<<simTime()<<endl;
    socketClosedFlag=true;

 //   numRequestsToSend = 0;
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
        //    if (this->getFullPath()=="PretioWithLB.client[0].app[0]")
          //      std::cout<<" SocketClosed   and sendReqTime at:    "<<simTime()<<endl;
           // socket.destroy();
         //   TcpAppBase::close();
            rescheduleOrDeleteTimer(d, MSGKIND_CONNECT);
        }
     }
    else
    {
        if (socket.getState() == TcpSocket::CONNECTED || socket.getState() == TcpSocket::CONNECTING || socket.getState() == TcpSocket::PEER_CLOSED)
        {
           // if (this->getFullPath()=="PretioWithLB.client[0].app[0]")
//                           std::cout<<this->getFullPath()<<"  ,   SocketClosed   destroy socket at:    "<<simTime()<<endl;

      //     socket.destroy();
        //   TcpAppBase::close();
        }
    }
   //
}
void ErgodicityTestClient::TimeOutSocketClosed()
{
//    if (this->getFullPath()=="PretioWithLB.client[0].app[0]")
//    std::cout<<this->getFullPath()<<":  TimeOutSocketClosed    at:    "<<simTime()<<"    burst_finished:" <<burst_finished<< endl;
    numRequestsToSend = -1;
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
        {
         //   socket.destroy();
         //   TcpAppBase::close();
           // connected=false;
            rescheduleOrDeleteTimer(d, MSGKIND_CONNECT);
        }
    }
}
void ErgodicityTestClient::socketClosed(TcpSocket *socket)  //no need for this function
{
    socketClosedFlag=true;

}
void ErgodicityTestClient::socketFailure(TcpSocket *socket, int code)
{

    std::cout<<this->getFullPath()<<"    In socketFailure  "<< this->getFullPath()<<"    state:           "<<socket->getState()<<endl;
    TcpAppBase::socketFailure(socket, code);
    // reconnect after a delay
    simtime_t d = simTime() ;
    connected=false;
    connectPort=par("connectPort");
    if (stopTime < SIMTIME_ZERO || d < stopTime)
    {
        std::cout<<"at:"<<simTime()<<endl;
        if (timeoutMsg)
            rescheduleOrDeleteTimer(d, MSGKIND_CONNECT);
    }
//        connect();
//        connected=true;
//        mainSocketID=socket->getSocketId();
//        firstConnection=false;
//    }
//    sendReqTime=simTime();  // The request initiated by user
//    if (earlySend)
//        sendRequest(0); //sendInternalRequestTime will be adjusted in this function
//
//
//    socketClosedFlag=false;
//    socketEstablished(socket); // number of needed middle servers specified here

}
} // namespace ErgodicityTest

