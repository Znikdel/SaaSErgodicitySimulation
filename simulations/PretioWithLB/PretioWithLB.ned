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

package ErgodicityTest.simulations.PretioWithLB;

import ErgodicityTest.ClientNode;
import ErgodicityTest.PhysicalNode;
import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;
import inet.node.internetcloud.InternetCloud;
import ned.DatarateChannel;


network PretioWithLB
{
    parameters:
        int numClients;
        int numServers;

    types:
        channel C extends DatarateChannel
        {
            delay = 10ms;
            datarate = 5Mbps;
        }
    submodules:
        configurator: Ipv4NetworkConfigurator {
            parameters:
                @display("p=100,100;is=s");
        }

        //        client[numClients]: StandardHost {
        //            @display("p=250,100,row,100");
        //        }
        client[numClients]: ClientNode {
            @display("p=250,100,row,100");
        }
        server[numServers]: PhysicalNode {
            @display("p=250,500");
        }
        cloudNetwork: InternetCloud {
            @display("p=250,300;is=vl");
        }

    connections:

        for i=0..numClients-1 {
            client[i].pppg++ <--> C <--> cloudNetwork.pppg++;
        }
        for j=0..numServers-1 {
            server[j].pppg++ <--> C <--> cloudNetwork.pppg++;
        }
}



