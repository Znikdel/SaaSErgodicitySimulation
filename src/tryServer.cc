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

#include "tryServer.h"

#include "inet/applications/common/SocketTag_m.h"
#include "inet/applications/tcpapp/GenericAppMsg_m.h"
#include "inet/common/ModuleAccess.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/common/lifecycle/NodeStatus.h"
#include "inet/common/packet/Message.h"
#include "inet/common/packet/chunk/ByteCountChunk.h"
#include "inet/common/TimeTag_m.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include "inet/transportlayer/contract/tcp/TcpCommand_m.h"

namespace ErgodicityTest {

Define_Module(tryServer);

void tryServer::initialize(int stage)
{
    cSimpleModule::initialize(stage);

    if (stage == INITSTAGE_LOCAL) {
        delay = par("replyDelay");
        maxMsgDelay = 0;

        //statistics
        msgsRcvd = msgsSent = bytesRcvd = bytesSent = 0;

     //   WATCH(msgsRcvd);
     //   WATCH(msgsSent);
     //   WATCH(bytesRcvd);
     //   WATCH(bytesSent);
    //    WATCH(socketQueue);
       // socketQueue.
    }
    else if (stage == INITSTAGE_APPLICATION_LAYER) {
        const char *localAddress = par("localAddress");
        int localPort = par("localPort");
        socket.setOutputGate(gate("socketOut"));
        socket.bind(localAddress[0] ? L3AddressResolver().resolve(localAddress) : L3Address(), localPort);
        socket.listen();

        cModule *node = findContainingNode(this);
        NodeStatus *nodeStatus = node ? check_and_cast_nullable<NodeStatus *>(node->getSubmodule("status")) : nullptr;
        bool isOperational = (!nodeStatus) || nodeStatus->getState() == NodeStatus::UP;
        if (!isOperational)
            throw cRuntimeError("This module doesn't support starting in node DOWN state");
    }
}

void tryServer::sendOrSchedule(cMessage *msg, simtime_t delay)
{
    if (delay == 0)
    {
        sendBack(msg);
    }
    else
    {
        scheduleAt(simTime() + delay, msg);
    }
}

void tryServer::sendBack(cMessage *msg)
{
    EV_INFO << "in send back self msg\n";
    Packet *packet = dynamic_cast<Packet *>(msg);

    if (packet) {
        msgsSent++;
        bytesSent += packet->getByteLength();
        emit(packetSentSignal, packet);

        EV_INFO << "sending \"" << packet->getName() << "\" to TCP, " << packet->getByteLength() << " bytes\n";
    }
    else {
        EV_INFO << "sending \"" << msg->getName() << "\" to TCP\n";
    }

    auto& tags = getTags(msg);
    tags.addTagIfAbsent<DispatchProtocolReq>()->setProtocol(&Protocol::tcp);
    send(msg, "socketOut");
}

void tryServer::handleMessage(cMessage *msg)
{

    /*//msg kind:
    TCP_I_DATA = 1;              // data packet (set on data packet)
    TCP_I_URGENT_DATA = 2;       // urgent data (set on data packet)
    TCP_I_AVAILABLE = 3;         // conncetion available
    TCP_I_ESTABLISHED = 4;       // connection established
    TCP_I_PEER_CLOSED = 5;       // FIN received from remote TCP
    TCP_I_CLOSED = 6;            // connection closed normally (via FIN exchange)
    TCP_I_CONNECTION_REFUSED = 7; // connection refused
    TCP_I_CONNECTION_RESET = 8;  // connection reset
    TCP_I_TIMED_OUT = 9;         // conn-estab timer went off, or max retransm. count reached
    TCP_I_STATUS = 10;           // status info (will carry ~TcpStatusInfo)
    TCP_I_SEND_MSG = 11;         // send queue abated, send more messages
    TCP_I_DATA_NOTIFICATION = 12; // notify the upper layer that data has arrived
    */
    if (msg->isSelfMessage()) {
        sendBack(msg);
    }
    else if (msg->getKind() == TCP_I_PEER_CLOSED) {
        // we'll close too, but only after there's surely no message
        // pending to be sent back in this connection
        //std::cout<<"in  tryserver:handlemsg:    "<< this->getFullPath();
        std::cout<<"   TCP_I_PEER_CLOSED      "<<endl;
        int connId = check_and_cast<Indication *>(msg)->getTag<SocketInd>()->getSocketId();
        //std::cout<<"   connId    "<< connId <<endl;
        delete msg;
        auto request = new Request("close", TCP_C_CLOSE);
        TcpCommand *cmd = new TcpCommand();
        request->addTag<SocketReq>()->setSocketId(connId);
        request->setControlInfo(cmd);
    //    //std::cout<<"   before scheduling in server msg handling    "<< connId <<endl;
        sendOrSchedule(request, delay + maxMsgDelay);
    }
    else if (msg->getKind() == TCP_I_DATA || msg->getKind() == TCP_I_URGENT_DATA) {
      //  std::cout<<"in  tryserver:handlemsg:    "<< this->getFullPath();
    //    std::cout<<"   TCP_I_DATA  -  TCP_I_URGENT_DATA      ";//<<endl;  //here send the reply
        Packet *packet = check_and_cast<Packet *>(msg);
        int connId = packet->getTag<SocketInd>()->getSocketId();
        ChunkQueue &queue = socketQueue[connId];
        auto chunk = packet->peekDataAt(B(0), packet->getTotalLength(), Chunk::PF_ALLOW_INCOMPLETE);
        queue.push(chunk);
        emit(packetReceivedSignal, packet);
     //   std::cout<<"   connId    "<< connId <<endl;
        bool doClose = false;
        while (const auto& appmsg = queue.pop<GenericAppMsg>(b(-1), Chunk::PF_ALLOW_NULLPTR)) {
            msgsRcvd++;
            bytesRcvd += B(appmsg->getChunkLength()).get();
            B requestedBytes = appmsg->getExpectedReplyLength();
            simtime_t msgDelay = appmsg->getReplyDelay();
            if (msgDelay > maxMsgDelay)
                maxMsgDelay = msgDelay;

            if (requestedBytes > B(0)) {
                Packet *outPacket = new Packet(msg->getName(), TCP_C_SEND);
                outPacket->addTag<SocketReq>()->setSocketId(connId);
                const auto& payload = makeShared<GenericAppMsg>();
                payload->setChunkLength(requestedBytes);
                payload->setExpectedReplyLength(B(0));
                payload->setReplyDelay(0);
                payload->addTag<CreationTimeTag>()->setCreationTime(simTime());
                outPacket->insertAtBack(payload);
                sendOrSchedule(outPacket, delay + msgDelay);
            }
            if (appmsg->getServerClose()) {
                doClose = true;
                break;
            }
        }
        delete msg;

        if (doClose) {
            //std::cout<<"in  tryserver:handlemsg:    "<< this->getFullPath();
            std::cout<<"   TCP_I_DATA  -  Do Close      "<<endl;
            auto request = new Request("close", TCP_C_CLOSE);
            TcpCommand *cmd = new TcpCommand();
            request->addTag<SocketReq>()->setSocketId(connId);
            request->setControlInfo(cmd);
            sendOrSchedule(request, delay + maxMsgDelay);
        }
    }
    else if (msg->getKind() == TCP_I_AVAILABLE)
    {
        //std::cout<<"in  tryserver:handlemsg:    "<< this->getFullPath();
        //std::cout<<"   TCP_I_AVAILABLE      "<<endl;
        int connId = check_and_cast<Indication *>(msg)->getTag<SocketInd>()->getSocketId();
        //std::cout<<"   connId    "<< connId <<endl;
        socket.processMessage(msg);
    }
    else {
        // some indication -- ignore
        EV_WARN << "drop msg: " << msg->getName() << ", kind:" << msg->getKind() << "(" << cEnum::get("inet::TcpStatusInd")->getStringFor(msg->getKind()) << ")\n";
        delete msg;
    }
}

void tryServer::refreshDisplay() const
{
    char buf[64];
    sprintf(buf, "rcvd: %ld pks %ld bytes\nsent: %ld pks %ld bytes", msgsRcvd, bytesRcvd, msgsSent, bytesSent);
    getDisplayString().setTagArg("t", 0, buf);
}

void tryServer::finish()
{
    EV_INFO << getFullPath() << ": sent " << bytesSent << " bytes in " << msgsSent << " packets\n";
    EV_INFO << getFullPath() << ": received " << bytesRcvd << " bytes in " << msgsRcvd << " packets\n";
}

} // namespace inet
