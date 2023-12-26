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

//
// TODO auto-generated module
//
simple TryQ
{
    
     parameters:
        @group(Queueing);
        @signal[dropped](type="long");
        @signal[received](type="long");
        @signal[queueLength](type="long");
        @signal[queueingTime](type="simtime_t");
        
        @statistic[dropped](title="drop event";record=vector?,count;interpolationmode=none);
        @statistic[received](title="received";record=vector?,count;interpolationmode=none);
        @statistic[queueLength](title="queue length";record=vector,timeavg,max;interpolationmode=sample-hold);
        @statistic[queueingTime](title="queueing time at dequeue";record=vector?,mean,max;unit=s;interpolationmode=none);
        @display("i=block/passiveq;q=queue");

        int capacity = default(-1);  // negative capacity means unlimited queue
        bool fifo = default(true);   // whether the module works as a queue (fifo=true) or a stack (fifo=false)
        string sendingAlgorithm @enum("priority","random","roundRobin","minDelay") = default("priority");
		double droppedMsgSendDelay @unit(s) = default(1s);                                     // how the queue tries to find a suitable server for an incoming job
    gates:
        input in[];
        output out[];
}
