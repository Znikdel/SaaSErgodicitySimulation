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

package ergodicitytest;

import inet.linklayer.ieee80211.mac.queue.CompoundPendingQueue;
import inet.queueing.queue.CompoundPacketQueue;
import inet.queueing.queue.PacketQueue;


module AppQ
{
    parameters:
        @display("i=block/queue;bgb=285,308");
        int numQ = default(1);  // each Q represents one app in the application layer

    gates:
        input socketIn[numQ] @labels(TcpCommand/up);
        output socketOut[numQ] @labels(TcpCommand/down);

    submodules:

        Q[numQ]: TryQ {
            capacity = default(10);
            @display("p=169.54666,99.22333");
        }


        S[numQ]: RRScheduler {

            @display("p=74,100");
        }
    connections:
        for i=0..numQ-1 {
            socketIn[i] --> Q[i].in++;
            Q[i].out++ --> S[i].in++;
            S[i].out --> socketOut[i];


        }

}
