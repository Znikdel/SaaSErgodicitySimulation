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

package ErgodicityTest;

import inet.applications.contract.IApp;
import inet.common.MessageDispatcher;
import inet.node.base.TransportLayerNodeBase;
import inet.transportlayer.tcp_common.TcpSpoof;


module MiddleServer extends TransportLayerNodeBase
{
    parameters:
        @display("p=230,200;i=device/server");
        @figure[submodules];

        forwarding = default(false);  // disable routing by default
        ipv4.arp.proxyArpInterfaces = default(""); // proxy arp is disabled on hosts by default
        *.routingTableModule = default("^.ipv4.routingTable");
        int numContainers = default(0);
        @figure[applicationLayer](type=rectangle; pos=250,6; size=1000,130; lineColor=#808080; cornerRadius=5; fillColor=#ffff00; fillOpacity=0.1);
        @figure[applicationLayer.title](type=text; pos=1245,11; anchor=ne; text="application layer");

    submodules:
        container[numContainers]: Container {
            parameters:
                @display("p=375,76,row,150");
        }
        at: MessageDispatcher {
            parameters:
                @display("p=750,146;b=1000,5,,,,1");
        }


        OS_scheduler: AppQ {
            numQ = numVms;
            @display("p=702,49");
        }
        tcpSpoof: TcpSpoof {
            @display("p=381.15,86.625");
        }
    connections allowunconnected:
        for i=0..numContainers-1 {
            container[i].socketOut --> at.in++;
            OS_scheduler.socketOut[i] --> container[i].socketIn;
            at.out++ --> OS_scheduler.socketIn[i];

        }

        at.out++ --> udp.appIn if hasUdp;
        at.in++ <-- udp.appOut if hasUdp;

        at.out++ --> tcp.appIn if hasTcp;
        at.in++ <-- tcp.appOut if hasTcp;

        at.out++ --> sctp.appIn if hasSctp;
        at.in++ <-- sctp.appOut if hasSctp;

        at.out++ --> tn.in++;
        at.in++ <-- tn.out++;
}
