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

//
// TODO auto-generated module
//
simple RRScheduler
{
      parameters:
        @group(Queueing);
        @display("i=block/server");
        @signal[busy](type="bool");
     //   @statistic[busy](title="server busy state";record=vector?,timeavg,mean,max;interpolationmode=sample-hold);

        string fetchingAlgorithm @enum("priority","random","roundRobin","longestQueue") = default("priority");
             // how the next job will be choosen from the attached queues
        volatile double serviceTime @unit(s); // service time of a job, it simulates if other apps were on the VM because it's delaying the app service
		volatile double sleepTime @unit(s);   		
		volatile double scheduleShare @unit(s);
        bool schedulerStatus= default(false);
      gates:
        input in[];
        output out;
}
