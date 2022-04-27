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

import inet.applications.contract.IApp;

module VirtualMachine
{
    parameters:
        int numApps = default(0);
        @display("bgb=931,486");
    gates:
        input socketIn;
        output socketOut;


    submodules:
        app[numApps]: <> like IApp {
            parameters:
                @display("p=570,280,row,150");
        }

        tryQ: TryQ {
            @display("p=576,76");
        }
        tryQserver[numApps]: RRScheduler {
            
            @display("p=567,166");
        }
    connections:
        socketIn --> tryQ.in++;
        for i=0..numApps-1{
        	tryQ.out++ --> tryQserver[i].in++;
        }
}
